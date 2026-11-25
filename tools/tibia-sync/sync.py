#!/usr/bin/env python3
"""Vendor the official Tibia client's UI and API into apps/qtclient/client/upstream.

  sync.py --tibia /path/to/Tibia.app [--msgmap msgmap.jsonl] [--assets-dir DIR]
  sync.py --check

Sync extracts, from the installed client:
  qml/qmlcomponents/**          the embedded QML tree (all rcc roots merged)
  qml/QtQuick/LegacyControls/** QML carved from the install LegacyControls plugin
                                (C++ stays in third_party/Qt5LegacyControls)
  shaders/*.qsb                 the world shaders
  strings/client.en.qm          qsTrId translations
  conf/clientoptions.json       default options
  conf/config.ini               bundled client settings (embedded :/conf/config.ini)
  proto/*.proto                 embedded protobuf schemas
  contract/                     moc metaobjects, QML elements and enum scopes, typed classes, option ids
  protocol/messages.json        message opcodes (requires --msgmap from TibiaMessageMap.java
                                whenever the client binary sha changes)
then writes DRIFT.md against the previous tree, MANIFEST.json and VERSION.json, and
copies graphics_resources.rcc (runtime art, never committed) into data/assets/.

A new client_sha256 without --msgmap is a hard error so protocol/ cannot silently
freeze across CIP releases. The same gate applies to --funcmap (TibiaFunctionBodies
JSONL): it refreshes upstream/cip_stamps/ (per-function // b1/<hash> headers).
Raw decompiles stay in gitignored tmp/re/; commit upstream/.

--check (CI) verifies MANIFEST.json against kind-B under upstream/ and kind-A
under QTCLIENT_CIPSOFT_DIR / apps/cipsoft-data (qml/shaders/strings/conf/proto),
plus pack override cip-base shas. cip_stamps/ are not required in the dump.
The host has no client/overrides/ (CipSoft parity — product forks live in the pack).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
_PARENT = HERE.parents[1]
# tools/tibia-sync may live under the monorepo or under apps/qtclient.
if (_PARENT / "apps" / "qtclient" / "client" / "tibia").is_dir():
    MONOREPO_ROOT = _PARENT
    QTCLIENT_ROOT = _PARENT / "apps" / "qtclient"
elif (_PARENT / "client" / "tibia").is_dir():
    QTCLIENT_ROOT = _PARENT
    _mono = _PARENT.parent.parent
    if (_mono / "apps" / "qtclient").resolve() == QTCLIENT_ROOT.resolve():
        MONOREPO_ROOT = _mono
    else:
        MONOREPO_ROOT = QTCLIENT_ROOT  # standalone public qtclient tree
else:
    raise SystemExit(f"cannot locate qtclient from {HERE}")

DEFAULT_OUT = QTCLIENT_ROOT / "client" / "upstream"
RE_TOOLS = HERE.parent / "re"
if not (RE_TOOLS / "qt_rcc.py").is_file():
    RE_TOOLS = MONOREPO_ROOT / "tools" / "re"
if not (RE_TOOLS / "qt_rcc.py").is_file():
    RE_TOOLS = QTCLIENT_ROOT / "tools" / "re"
CACHE = QTCLIENT_ROOT / "tmp/re/tibia-sync"

sys.path.insert(0, str(HERE))
sys.path.insert(0, str(MONOREPO_ROOT / "tools"))
sys.path.insert(0, str(HERE.parent))  # standalone: tools/tibia_paths.py next to tibia-sync/
import drift  # noqa: E402
from tibia_paths import resolve_tibia, usage_hint  # noqa: E402

GENERATED = ("DRIFT.md", "MANIFEST.json")
OPTION_ANCHOR = b"\0actionBarBottomLocked\0"
IDENTIFIER = re.compile(r"^[A-Za-z][A-Za-z0-9]*$")


def log(msg):
    print(f"[tibia-sync] {msg}", flush=True)


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def contents_dir(path: Path) -> Path:
    for c in (path, path / "Contents"):
        if (c / "MacOS/client").is_file():
            return c
    raise SystemExit(f"no MacOS/client under {path}")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def carve(binary: Path, out: Path):
    if not out.is_dir():
        run(sys.executable, RE_TOOLS / "qt_rcc.py", "--carve", binary, out)
    return out


def merge_qml(roots: Path, module_path: str, dest: Path) -> int:
    count = 0
    for root in sorted(roots.iterdir()):
        src = root / module_path
        if not src.is_dir():
            continue
        for f in src.rglob("*"):
            if f.is_file():
                target = dest / f.relative_to(src)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, target)
                count += 1
    return count


def legacycontrols_plugin(contents: Path) -> Path:
    """Install-tree QtQuick.LegacyControls plugin (QML baked into the binary)."""
    lc_dir = contents / "Resources/qml/QtQuick/LegacyControls"
    names = (
        "libQt5LegacyControls.dylib",
        "Qt5LegacyControls.dll",
        "libQt5LegacyControls.so",
        "libQt5LegacyControls.so.1",
    )
    for name in names:
        candidate = lc_dir / name
        if candidate.is_file():
            return candidate
    if lc_dir.is_dir():
        for candidate in sorted(lc_dir.iterdir()):
            if candidate.is_file() and candidate.suffix in {".dylib", ".dll", ".so"}:
                return candidate
            if candidate.is_file() and ".so." in candidate.name:
                return candidate
    raise SystemExit(
        f"no LegacyControls plugin under {lc_dir} "
        f"(expected libQt5LegacyControls.dylib / Qt5LegacyControls.dll)"
    )


def option_ids(binary: bytes) -> dict[str, str]:
    """Option id -> name. The names are one run of C strings, but not in id
    order: the x86_64 table builder (0x1009ca690) pairs them as
    `mov dword [rbp+d], id` + `lea rdx, [rip+name]`, so the ids come from those
    instruction pairs (file offsets of code and strings share the __TEXT delta)."""
    pos = binary.find(OPTION_ANCHOR)
    if pos < 0:
        raise SystemExit("option table anchor not found")
    pos += 1
    names = {}
    while True:
        end = binary.index(b"\0", pos)
        name = binary[pos:end].decode("latin1")
        if not IDENTIFIER.match(name):
            break
        names[pos] = name
        pos = end + 1
    pair = re.compile(rb"\xc7(?:\x85....|\x45.)(....)\x48\x8d\x15(....)", re.DOTALL)
    ids = {}
    for m in pair.finditer(binary):
        target = m.end() + int.from_bytes(m.group(2), "little", signed=True)
        if target in names:
            ids[int.from_bytes(m.group(1), "little")] = names[target]
    if sorted(ids) != list(range(len(names))) or sorted(ids.values()) != sorted(names.values()):
        raise SystemExit(f"option table builder not decoded: {len(ids)} ids for {len(names)} names")
    # Anchors for TibiaExternal 15.33.c68982 (testserver): effect opacities and several
    # gameWindow* ids were reordered vs live Tibia.app / prior vendor; stashSortOrder added.
    expected = {0x32: "lightAmbientLevel", 0x39: "ownEffectsOpacity", 0x3C: "gameWindowScaleOnlyByEvenMultiples",
                0x4E: "gameWindowShowTextualEffects", 0x5A: "frameRateLimit", 0x5E: "vSyncEnabled"}
    for i, name in expected.items():
        if ids.get(i) != name:
            raise SystemExit(f"option table moved: {hex(i)} is {ids.get(i)}, expected {name}")
    return {hex(i): ids[i] for i in sorted(ids)}


def qml_registrations(binary: bytes, moc: list, qml_root: Path) -> dict:
    """CIP registers enum-only scopes in module qmlenumvalues by short name
    (qmlRegisterUncreatableMetaObject). The names are a C-string run after the
    module name; each resolves to the moc class owning the enum keys its QML uses."""
    pos = binary.index(b"\0qmlenumvalues\0") + len("qmlenumvalues") + 2
    reason, names = None, []
    while True:
        end = binary.index(b"\0", pos)
        s = binary[pos:end].decode("latin1")
        pos = end + 1
        if reason is None and " " in s:
            reason = s
            continue
        if not IDENTIFIER.match(s):
            break
        names.append(s)
    qml = "\n".join(p.read_text(errors="replace") for p in qml_root.rglob("*.qml"))
    owners = [c for c in moc if c["enums"] and c["superclass"]]
    scopes = {}
    for name in names:
        used = set(re.findall(rf"\b{name}\.([A-Za-z_]\w*)", qml))

        def score(c):
            keys = {k for e in c["enums"] for k in e["values"]} | {e["name"] for e in c["enums"]}
            short = c["class"].split("::")[-1]
            return len(used & keys), short in (name, "T" + name), -abs(len(short) - len(name))

        best = max(owners, key=score)
        if score(best)[0] == 0 and not score(best)[1]:
            raise SystemExit(f"enum scope {name}: no moc class owns its keys {sorted(used)[:5]}")
        scopes[name] = best["class"]
    return {"enum_uri": "qmlenumvalues", "enum_reason": reason, "enum_scopes": scopes}


def cip_version(binary: bytes) -> str:
    found = sorted(set(re.findall(rb"\0(1[0-9]\.[0-9]{2})\0", binary)))
    return found[-1].decode() if found else "unknown"


def cip_version_int(dotted: str) -> int | None:
    """`15.33` → 1533 (login clientVersion / crystalserver CLIENT_VERSION)."""
    m = re.fullmatch(r"(\d+)\.(\d+)", (dotted or "").strip())
    if not m:
        return None
    return int(m.group(1)) * 100 + int(m.group(2))


def read_product_client_version() -> int | None:
    path = QTCLIENT_ROOT / "client/tibia/client/gameclientbase.h"
    if not path.is_file():
        return None
    m = re.search(r"static\s+constexpr\s+int\s+ClientVersion\s*=\s*(\d+)\s*;", path.read_text())
    return int(m.group(1)) if m else None


def read_server_client_version() -> int | None:
    path = MONOREPO_ROOT / "apps/crystalserver/src/core.hpp"
    if not path.is_file():
        return None
    m = re.search(r"CLIENT_VERSION\s*=\s*(\d+)\s*;", path.read_text())
    return int(m.group(1)) if m else None


def set_constexpr_int(path: Path, name: str, value: int) -> bool:
    """Rewrite `name = <int>;` (ClientVersion / CLIENT_VERSION). Returns True if changed."""
    if not path.is_file():
        return False
    text = path.read_text()
    pattern = re.compile(rf"(\b{re.escape(name)}\s*=\s*)(\d+)(\s*;)")
    new, n = pattern.subn(rf"\g<1>{value}\g<3>", text, count=1)
    if n and new != text:
        path.write_text(new)
        return True
    return False


def sync_protocol_versions(cip_dotted: str, *, apply: bool) -> list[str]:
    """Align product ClientVersion + crystalserver CLIENT_VERSION with cip_version.

    When apply=True, rewrite both headers. Always returns human drift lines.
    """
    want = cip_version_int(cip_dotted)
    if want is None:
        return [f"protocol: cannot parse cip_version `{cip_dotted}`"]
    lines: list[str] = []
    client_path = QTCLIENT_ROOT / "client/tibia/client/gameclientbase.h"
    server_path = MONOREPO_ROOT / "apps/crystalserver/src/core.hpp"
    have_client = read_product_client_version()
    have_server = read_server_client_version()
    if have_client is not None and have_client != want:
        lines.append(f"protocol bump client ClientVersion {have_client}→{want}")
        if apply:
            set_constexpr_int(client_path, "ClientVersion", want)
            log(f"protocol: set ClientVersion={want} in {client_path}")
    if have_server is not None and have_server != want:
        lines.append(f"protocol bump server CLIENT_VERSION {have_server}→{want}")
        if apply:
            set_constexpr_int(server_path, "CLIENT_VERSION", want)
            log(f"protocol: set CLIENT_VERSION={want} in {server_path}")
    if have_client == want and have_server == want:
        lines.append(f"protocol: ClientVersion/CLIENT_VERSION already {want} ({cip_dotted})")
    elif have_client is None:
        lines.append("protocol: gameclientbase.h ClientVersion not found")
    elif have_server is None:
        lines.append("protocol: crystalserver core.hpp CLIENT_VERSION not found (submodule?)")
    return lines


def qt_version(contents: Path) -> str:
    plist = contents / "Frameworks/QtCore.framework/Resources/Info.plist"
    if plist.is_file():
        r = subprocess.run(["/usr/libexec/PlistBuddy", "-c", "Print :CFBundleShortVersionString", plist],
                           capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.strip()
    return "unknown"


def stage(contents: Path, work: Path, msgmap: Path | None) -> Path:
    client = contents / "MacOS/client"
    data = client.read_bytes()
    stage_dir = work / "upstream"
    shutil.rmtree(stage_dir, ignore_errors=True)
    stage_dir.mkdir(parents=True)

    embedded = carve(client, work / "embedded")
    n = merge_qml(embedded, "qt/qml/qmlcomponents", stage_dir / "qml/qmlcomponents")
    log(f"qmlcomponents: {n} files")
    for root in embedded.iterdir():
        for qsb in (root / "shaders").glob("*.qsb") if (root / "shaders").is_dir() else []:
            (stage_dir / "shaders").mkdir(exist_ok=True)
            shutil.copy2(qsb, stage_dir / "shaders" / qsb.name)

    lc_plugin = legacycontrols_plugin(contents)
    lc_carved = carve(lc_plugin, work / "legacycontrols")
    lc_n = merge_qml(lc_carved, "qt/qml/QtQuick/LegacyControls",
                     stage_dir / "qml/QtQuick/LegacyControls")
    if lc_n == 0:
        raise SystemExit(f"LegacyControls carve from {lc_plugin} produced no QML")
    log(f"LegacyControls: {lc_n} files from {lc_plugin.name}")

    (stage_dir / "strings").mkdir()
    shutil.copy2(contents / "MacOS/client.en.qm", stage_dir / "strings/client.en.qm")
    (stage_dir / "conf").mkdir()
    shutil.copy2(contents / "Resources/conf/clientoptions.json", stage_dir / "conf/clientoptions.json")
    config_ini = [r / "conf/config.ini" for r in sorted(embedded.iterdir()) if (r / "conf/config.ini").is_file()]
    if len(config_ini) != 1:
        raise SystemExit(f"embedded conf/config.ini: expected 1 copy, carved {len(config_ini)}")
    shutil.copy2(config_ini[0], stage_dir / "conf/config.ini")

    (stage_dir / "proto").mkdir()
    import protodump  # protobuf; only the extract path needs it

    for name, fd in sorted(protodump.find_descriptors(data).items()):
        (stage_dir / "proto" / name).write_text(protodump.render(fd))
    log(f"proto: {len(list((stage_dir / 'proto').iterdir()))} schemas")

    contract = stage_dir / "contract"
    contract.mkdir()
    fixups = work / "fixups.txt"
    if not fixups.is_file():
        # dyld_info traps (exit 133) after printing the full table on this binary.
        with fixups.open("w") as f:
            subprocess.run(["dyld_info", "-fixups", str(client)], stdout=f)
        if fixups.stat().st_size < 1024:
            fixups.unlink()
            raise SystemExit("dyld_info -fixups produced no output")
    run(sys.executable, RE_TOOLS / "qt_moc.py", client, contract / "moc.json", "--fixups", fixups)
    run(sys.executable, RE_TOOLS / "qml_contract.py", contract / "moc.json", stage_dir / "qml/qmlcomponents/qml", contract)
    (contract / "option_ids.json").write_text(json.dumps(option_ids(data), indent=1) + "\n")
    regs = qml_registrations(data, json.loads((contract / "moc.json").read_text()), stage_dir / "qml")
    (contract / "qml_registrations.json").write_text(json.dumps(regs, indent=1) + "\n")
    log(f"qmlenumvalues: {len(regs['enum_scopes'])} enum scopes")

    if msgmap:
        (stage_dir / "protocol").mkdir()
        cov = work / "coverage.json"
        run(sys.executable, RE_TOOLS / "proto_coverage.py", "--msgmap", msgmap, "--binary", client,
            "--root", MONOREPO_ROOT, "--json", cov)
        messages = [{"dir": m["dir"], "opcode": m["opcode"], "tibia": m.get("tibia", [])}
                    for m in json.loads(cov.read_text()) if m.get("tibia")]
        (stage_dir / "protocol/messages.json").write_text(json.dumps(messages, indent=1) + "\n")

    (stage_dir / "VERSION.json").write_text(json.dumps({
        "cip_version": cip_version(data),
        "qt_version": qt_version(contents),
        "client_sha256": sha256(client),
    }, indent=1) + "\n")
    return stage_dir


def manifest(root: Path) -> dict[str, str]:
    return {p.relative_to(root).as_posix(): sha256(p) for p in sorted(root.rglob("*"))
            if p.is_file() and p.name not in GENERATED}


def collect_override_roots(extra: Path | None = None) -> list[Path]:
    """Pack overrides only (host client/overrides/ is forbidden)."""
    roots: list[Path] = []
    seen: set[Path] = set()

    def add(path: Path) -> None:
        if not path.is_dir():
            return
        real = path.resolve()
        if real in seen:
            return
        seen.add(real)
        roots.append(path)

    mods_dir = QTCLIENT_ROOT / "client" / "mods"
    if mods_dir.is_dir():
        for mod in sorted(mods_dir.iterdir()):
            if not mod.is_dir() or mod.name.startswith("_"):
                continue
            if not (mod / "mod.json").is_file():
                continue
            add(mod / "overrides")
    pack = os.environ.get("QTCLIENT_PACK", "").strip()
    if pack:
        add(Path(pack) / "client" / "overrides")
    add(MONOREPO_ROOT / "apps" / "idleot-client" / "client" / "overrides")
    if extra is not None:
        add(extra)
    return roots


def assert_no_host_overrides() -> list[str]:
    """Host stock forks are forbidden — CipSoft parity."""
    host = QTCLIENT_ROOT / "client" / "overrides"
    if not host.exists():
        return []
    qml = sorted(p.relative_to(host).as_posix() for p in host.rglob("*.qml"))
    if qml:
        return [f"host client/overrides/{rel}" for rel in qml]
    # Empty dir / .gitkeep still forbidden
    return ["host client/overrides/ (remove the directory; pack overrides only)"]


KIND_A_PREFIXES = ("qml/", "shaders/", "strings/", "conf/", "proto/")


def _is_kind_a(rel: str) -> bool:
    return any(rel.startswith(p) for p in KIND_A_PREFIXES)


def check_tree_manifest(out: Path) -> dict[str, str]:
    """Actual file digests for --check: kind-B from upstream/, kind-A from dump."""
    kind_a = drift.resolve_kind_a_dir(QTCLIENT_ROOT)
    actual: dict[str, str] = {}
    for rel, digest in manifest(out).items():
        if rel.startswith("cip_stamps/") or _is_kind_a(rel):
            # kind-A may linger locally as a gitignored cache; SoT is cipsoft-data.
            continue
        actual[rel] = digest
    expected_keys = json.loads((out / "MANIFEST.json").read_text()) if (out / "MANIFEST.json").is_file() else {}
    for rel in expected_keys:
        if rel.startswith("cip_stamps/") or not _is_kind_a(rel):
            continue
        path = (kind_a / "client" / rel) if kind_a is not None else (out / rel)
        if not path.is_file() and kind_a is not None:
            path = out / rel
        if path.is_file():
            actual[rel] = sha256(path)
    return actual


def check(out: Path, extra_overrides: Path | None = None) -> int:
    expected_raw = json.loads((out / "MANIFEST.json").read_text()) if (out / "MANIFEST.json").is_file() else None
    if expected_raw is None:
        log(f"no {out}/MANIFEST.json; run make tibia-sync")
        return 1
    # cip_stamps/ are generated for porting; not part of the kind-A dump gate.
    expected = {k: v for k, v in expected_raw.items() if not k.startswith("cip_stamps/")}
    added, removed, changed = drift.keyed_diff(expected, check_tree_manifest(out))
    host_bad = assert_no_host_overrides()
    stale, orphan = drift.overrides_report(out, collect_override_roots(extra_overrides))
    for label, items in (("hand-added", added), ("missing", removed), ("hand-edited", changed),
                         ("forbidden host override", host_bad),
                         ("override with stale cip-base", stale), ("override without CIP file", orphan)):
        for item in items:
            log(f"{label}: {item}")
    bad = added or removed or changed or host_bad or stale or orphan
    log("upstream/ is clean" if not bad else "upstream/ is out of sync: never hand-edit upstream/ (re-run make tibia-sync); "
        "rebase stale pack overrides onto the new CIP file and update their cip-base sha; "
        "host client/overrides/ must not exist")

    ver = load_json_version(out)
    want = cip_version_int(ver.get("cip_version", ""))
    have_c, have_s = read_product_client_version(), read_server_client_version()
    proto_bad = False
    if want is not None and (have_c != want or have_s != want):
        proto_bad = True
        log(f"protocol drift: cip_version={ver.get('cip_version')} → {want}, "
            f"ClientVersion={have_c}, CLIENT_VERSION={have_s} "
            "(re-run make tibia-sync to rewrite both)")

    import launcher_sync
    return 1 if bad or proto_bad or launcher_sync.check_tree() else 0


def load_json_version(out: Path) -> dict:
    path = out / "VERSION.json"
    if not path.is_file():
        return {}
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return {}


def previous_client_sha(out: Path) -> str | None:
    path = out / "VERSION.json"
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text()).get("client_sha256")
    except (json.JSONDecodeError, OSError):
        return None


def require_msgmap_for_new_client(out: Path, client_sha: str, msgmap: Path | None) -> None:
    """Fail when the binary changes but protocol/ would be left stale."""
    prev = previous_client_sha(out)
    if prev == client_sha or msgmap is not None:
        return
    prev_label = f"{prev[:12]}…" if prev else "none"
    raise SystemExit(
        f"client binary changed ({prev_label} → {client_sha[:12]}…); "
        "pass --msgmap=… (TibiaMessageMap.java JSONL) to refresh protocol/messages.json"
    )


def require_funcmap_for_new_client(out: Path, client_sha: str, funcmap: Path | None) -> None:
    """Fail when the binary changes but cip_stamps/ would be left stale.

    First-time trees with no cip_stamps/ yet may omit --funcmap (bootstrap).
    Once stamps exist, a new client_sha256 requires a fresh FUNCMAP.
    """
    prev = previous_client_sha(out)
    stamps = out / "cip_stamps"
    if prev == client_sha or funcmap is not None:
        return
    if not stamps.is_dir():
        return
    prev_label = f"{prev[:12]}…" if prev else "none"
    raise SystemExit(
        f"client binary changed ({prev_label} → {client_sha[:12]}…) and cip_stamps/ exists; "
        "pass --funcmap=… (TibiaFunctionBodies.java JSONL) to refresh function body stamps"
    )


def apply_funcmap(
    staged: Path,
    funcmap_path: Path | None,
    previous_out: Path,
    client_bin: Path | None = None,
) -> None:
    """Write staged/cip_stamps from FUNCMAP, or keep the previous tree."""
    import funcmap as fm

    if funcmap_path is not None:
        rows = fm.load_funcmap(funcmap_path)
        tip = QTCLIENT_ROOT / "client" / "tibia"
        if tip.is_dir():
            rows = fm.attach_names_from_tip(rows, [tip])
            if client_bin is not None and Path(client_bin).is_file():
                tip_rows = fm.rows_from_tip_addrs(Path(client_bin), [tip])
                rows = fm.merge_funcmap_and_tip(rows, tip_rows)
                log(f"cip_stamps/: merged {len(tip_rows)} tip /// CIP addr hashes")
        root, n = fm.write_stamp_headers(rows, staged, contract_dir=staged / "contract")
        log(f"cip_stamps/: {n} methods → {root}")
        return

    prev = previous_out / "cip_stamps"
    if prev.is_dir():
        shutil.copytree(prev, staged / "cip_stamps")
        log("cip_stamps/: kept the previous stamps (same client_sha256 or no --funcmap)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tibia", type=Path, help="Tibia.app (or its Contents)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--overrides", type=Path, default=None,
                    help="extra pack overrides/ root (in-tree mods + QTCLIENT_PACK + idleot-client are scanned)")
    ap.add_argument("--msgmap", type=Path, help="TibiaMessageMap.java output (JSONL) for protocol/messages.json "
                    "(required when client_sha256 changes)")
    ap.add_argument("--funcmap", type=Path, help="TibiaFunctionBodies.java output (JSONL) for cip_stamps/ "
                    "(required when client_sha256 changes once stamps exist)")
    ap.add_argument("--assets-dir", type=Path, help="assets dir for graphics_resources.rcc "
                    "(default apps/qtclient/data/assets)")
    ap.add_argument("--check", action="store_true", help="verify the vendored tree (CI)")
    args = ap.parse_args()

    if args.check:
        raise SystemExit(check(args.out, args.overrides))
    tibia_path = args.tibia
    if not tibia_path:
        try:
            tibia_path = resolve_tibia()
            log(f"using Tibia install: {tibia_path}")
        except FileNotFoundError:
            ap.error(usage_hint())

    contents = contents_dir(Path(tibia_path))
    client_sha = sha256(contents / "MacOS/client")
    require_msgmap_for_new_client(args.out, client_sha, args.msgmap)
    require_funcmap_for_new_client(args.out, client_sha, args.funcmap)
    work = CACHE / client_sha[:12]
    work.mkdir(parents=True, exist_ok=True)
    staged = stage(contents, work, args.msgmap)
    if not args.msgmap and (args.out / "protocol").is_dir():
        shutil.copytree(args.out / "protocol", staged / "protocol")
        log("protocol/: kept the previous messages.json (same client_sha256)")
    apply_funcmap(staged, args.funcmap, args.out, client_bin=contents / "MacOS/client")

    cip_ver = json.loads((staged / "VERSION.json").read_text()).get("cip_version", "")
    proto_lines = sync_protocol_versions(cip_ver, apply=True)

    text, changed = drift.report(args.out, staged, collect_override_roots(args.overrides))
    if proto_lines:
        # Prepend protocol section so DRIFT always names client+server bumps.
        text = text.replace(
            "# CIP drift\n\n",
            "# CIP drift\n\n## Protocol version\n\n"
            + "\n".join(f"- {line}" for line in proto_lines)
            + "\n\n",
            1,
        )
        changed = True
    (staged / "MANIFEST.json").write_text(json.dumps(manifest(staged), indent=1) + "\n")
    (staged / "DRIFT.md").write_text(text)

    with tempfile.TemporaryDirectory(dir=args.out.parent if args.out.parent.is_dir() else None) as tmp:
        if args.out.exists():
            args.out.rename(Path(tmp) / "old")
        shutil.copytree(staged, args.out)
    log(f"wrote {args.out} ({'changes: see DRIFT.md' if changed else 'no changes'})")

    assets = args.assets_dir or QTCLIENT_ROOT / "data/assets"
    rcc = contents / "MacOS/graphics_resources.rcc"
    if assets.is_dir() and rcc.is_file():
        # Engine art stays next to the assets dump for local/dev mounts; CDN
        # ships it in the client package, not assets-current.
        shutil.copy2(rcc, assets / "graphics_resources.rcc")
        log(f"art: {assets / 'graphics_resources.rcc'}")
    else:
        log(f"WARN: art not copied (assets dir {assets} missing; run make extract first)")

    proto = QTCLIENT_ROOT / "client/tibia/protocol"
    if (args.out / "protocol/messages.json").is_file() and proto.is_dir():
        run(
            sys.executable, proto / "gen_dispatch.py",
            "--messages", args.out / "protocol/messages.json",
            "--codes", QTCLIENT_ROOT / "client/tibia/protocol/protocolcodes.h",
            "--server", proto / "servermessages.cpp",
            "--client", proto / "clientmessages.cpp",
            "--map", proto / "handler_map.json",
            "--out", proto / "dispatch.gen.cpp",
        )
        log("protocol: regenerated client/tibia/protocol/dispatch.gen.cpp")

    import launcher_sync
    launcher = Path("/Applications/Tibia.app")
    launcher_sync.apply(launcher if launcher.is_dir() else None, contents)
    if launcher_sync.check_tree():
        log("WARN: hint images or the launcher fingerprint are incomplete; see launcher/upstream/DRIFT.md")


if __name__ == "__main__":
    main()
