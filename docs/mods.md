# Mods and packs

Product / third-party packs on the CipSoft qtclient host. Ship path is
**stock tip + runtime MODULE + FS chrome** for desktop and WASM.

Frozen tip extension hooks (CIP-neutral only):
[pack-extension-contract.md](pack-extension-contract.md)
(`scripts/ci/check-cip-tip-purity.py`). Pack SDK:
[`include/qtclient/`](../include/qtclient/README.md).

## Stock = CipSoft

- `client/upstream/` — vendored CIP QML / shaders / strings (`make sync`)
- `launcher/upstream/` — CIP launcher chrome
- `client/tibia/<area>/` — `tibia::<area>` filesystem
- `launcher/src/` — tip-protocol launcher
- `shared/src/` — `shared::` types

Do not invent a `modules/` tree that re-hosts stock CIP; stock is changed only
through pack `overrides/`.
Keep the name **mods** / **pack** (not “plugins” — that collides with Qt QML plugins).

## Decisions

| Choice | Why |
|--------|-----|
| Host-pushed C ABI (`include/qtclient/abi/`) | SDK headers define pack access; no tip export allowlist / `host_exports.json` |
| Packs export only `qtclient_mod_register` | Tip dlopens the MODULE and passes `QtClientHostApiV1*` |
| Zero tip imports in MODULE packs | Mods resolving tip/Qt symbols (export lists) was the past pain |
| FS QML + overrides | F5 / `PackQmlInterceptor`; not tip-baked `qt_add_qml_module` |
| Same pipeline desktop + WASM | `mods/<id>/native/mod.{dylib,so,dll,wasm}` |
| IdleOT `ABSORB_INTO_QTCLIENT` (deprecated, still used) | Deep tip façades still in use; peel onto `abi/`, then drop absorb |

Removed paths: tip export lists / `host_exports.json` (→ host-pushed API
table), packs importing `qtclient_mod_host_api_v1()` (→ the
`qtclient_mod_register(api, ctx)` argument), desktop-only `QPluginLoader`
(→ the same MODULE load on desktop and WASM).

Non-goals: tip export allowlists for packs; packs importing tip symbols;
process-isolated sidecar mods (lighting / opcodes stay in-process).

## Artifacts

```text
stock host (qtclient CI)     → tibia tip (+ optional default packs)
client-sdk (pack_sdk)        → include/qtclient/abi/ + cmake helpers
pack archive (pack CI)       → mods/<id>/native/mod.* + FS tree
product tip (release/local)  → host + N packs staged under tip/mods/
```

## Runtime MODULE (stock qtclient + product packs)

Product native code is a **detached MODULE** next to the tip. The tip never
relinks when pack C++ changes. Packs export a single symbol and receive the
host API table (SDK headers under `include/qtclient/abi/` define the surface).

```text
<qtclient tip>/
  tibia | tibia.js + tibia.wasm
  mods/<id>/
    mod.json
    native/mod.{dylib,so,dll,wasm}   # qtclient_mod_register
    qml/…  overrides/…  resources/…
```

| qtclient build | Result |
|------------|--------|
| Stock (`--no-mods`) | CIP tip; no packs |
| Default tip mods | `qtc-aac` FS urls |
| Product (`-DQTCLIENT_LINKED_PACK=`) | stages pack; IdleOT still `ABSORB_INTO_QTCLIENT` until abi peel |

Local product path: `make qtclient desktop|browser install`.

### Load sequence

```text
ModLoader::loadAll
  → for each mods/<id>/mod.json
      → dlopen native/mod.*
      → qtclient_mod_register(&hostApi, &loadCtx)
      → register FS qml/ overrides/ urls/identity
```

Packs never look up tip symbols. Extending capability means adding fields to
`QtClientHostApiV1` in [`include/qtclient/abi/host_api_v1.h`](../include/qtclient/abi/host_api_v1.h).

## Tip splash (login / launcher /play)

Canonical CDN path (shared by qtclient login, launcher chrome URL, and `/play`
transitions):

`{assetBase}/images/launcher/BackgroundImage.jpg`

Resolution: pack `mods/<id>/images/launcher/` (local F5) → tip CDN
(`{assetBase}/images/launcher/…`, local tip is `:9080` — not portal `/cdn`) →
RCC (`qrc:/images/title.jpg` / `:/images/launcher/…`). Product rebrand =
replace the tip file; stock / unchanged tip keeps CipSoft art. Do not put UI
chrome (`/images/background.png`, skin buttons) on the CDN — splash/marketing
only.

## Pack format (desktop + WASM — same tree)

```text
mods/<id>/
  mod.json
  native/mod.*          # optional; host-pushed C ABI MODULE
  qml/<qmlModule>/
  overrides/…
  resources/…
```

### Host-pushed register

```c
#include <qtclient/abi/mod_register.h>

QTCLIENT_MOD_EXPORT void qtclient_mod_register(
    const QtClientHostApiV1* api,
    const QtClientModLoadCtx* ctx)
{
    api->register_mod(/* … */);
}
```

Packs must **not** import tip symbols. Use only `api` function pointers.
CI: `scripts/ci/check-pack-abi-headers.py`.

### Rust packs

qtclient stays C++, but a pack may be a Rust `cdylib`: bindgen
[`include/qtclient/abi/*.h`](../include/qtclient/abi/), export
`qtclient_mod_register` with the `host_api_v1.h` signature, use only the passed
`QtClientHostApiV1*` (never link tip symbols), and stage the library as
`mods/<id>/native/mod.{dylib,so,dll,wasm}` next to the FS chrome. Working
example (bindgen + no-op hook):
[`apps/idleot-client/rust-smoke/`](../../idleot-client/rust-smoke/README.md).

### Multi-pack tip + `urls`

Tip loads **all** autoload packs under `mods/<id>/` (priority descending so lower
`priority` numbers win override remap). Identity / urls still first-wins after
ascending priority sort.

| Mode | Packs |
|------|-------|
| `--no-mods` / empty tip | none → real CipSoft |
| tip default | all autoload `mods/<id>/` |
| `QTCLIENT_MODS_DIR` | packs under that root (if none found → tip default) |
| `QTCLIENT_EXTRA_MODS_DIR` (DEV) | additional FS packs |

| Concern | Rule |
|---------|------|
| Native C++ | Each pack’s `qtclient_mod_register` via ModLoader |
| QML import paths | Append all `mods/<id>/qml` |
| Overrides | Later load wins (`PackQmlInterceptor` reverse scan) |
| Exclusive hooks | Lighting / HD / texture atlas hint: first setter wins; conflicts are logged |
| Additive hooks | Overlay factories append |

CMake: `-DQTCLIENT_LINKED_PACK=<pack>/client` (one source pack, IdleOT) or
`-DQTCLIENT_LINKED_PACKS=a/client;b/client` (several).

Local product Play may set `QTCLIENT_MODS_DIR` to a **launcher brand** directory
(`brand.json` only). The launcher strips that env when starting the game client
so tip `mods/<id>/mod.json` still supplies `urls.loginBase` and identity.

Optional `urls` on `mod.json` merge into `LauncherEnv`.

### Launcher config (`launcher.json`, `launchersettings`)

The launcher is CipSoft's; a pack configures it from its tip `mods/<id>/`
(staged from the pack's `launcher/` tree by `merge-launcher-brand.sh`):

| File / key | Role |
|------------|------|
| `brand.json` | identity + chrome (`productName`, `defaultCdn`, `images/launcher/`) |
| `launcher.json` `"channels": ["<name>", …]` | download channels of a build (`<name>url` / `<name>versionurl` in the launcher configuration) the launcher installs next to the assets; stock has none |
| `mod.json` / `launcher.json` `"launchersettings"` | `[GROUP] key` defaults (CipSoft's `launchersettings.ini` groups); higher `priority` number loses |

Host `[URLS]` keys beside CipSoft's: `clientRevision` (a JSON document whose
`revision` is handed to the client as `QTCLIENT_CLIENT_REVISION`; the default
`qtc-aac` pack sets `{cdn}/latest/release.json`), `contentStore`
(content-addressed file downloads, `<store>/<aa>/<packedhash>`; unset = CipSoft's
file urls), `systemSurvey`. `{cdn}` in a url is the tip CDN (`QTCLIENT_CDN_URL`,
else `QTCLIENT_ASSET_BASE`, else the brand's `defaultCdn`); without one the url
is unset.

| Pack | Role |
|------|------|
| [`mods/qtc-aac`](../client/mods/qtc-aac/mod.json) | Default tip [qtc-aac](https://github.com/qtclient/qtc-aac) |
| Product `mods/<id>/` | Author pack |

### WASM

1. Tip links with `MAIN_MODULE=2` only when runtime MODULE packs are enabled
   (`TibiaMods.cmake`) — packs are `SIDE_MODULE=2` with **zero tip imports**.
2. No tip export allowlist; capability surface is `QtClientHostApiV1`.
3. Optional `hostBuildId` on `mod.json` (strict by default).

## IdleOT transitional absorb

IdleOT still uses tip C++ façades (`session.h`, `worldview.h`, `dialog.h`,
`gameactions.h`, …). Until those are peeled onto `abi/`,
`qtclient_add_mod(idleot ABSORB_INTO_QTCLIENT …)` compiles pack sources into
`tibia` and the tip calls `qtclient_mod_<id>_register` instead of
`qtclient_mod_register`. New packs must use the runtime MODULE path.

## Filesystem QML (F5)

ModLoader adds `mods/<id>/qml`; F5 clears the QML cache on desktop DEV.

## Performance

Extensibility must not regress the CIP world hot path. Use the existing frame
timers (`provider->buildNs()` / `tilesNs()` / `lightNs()`); the client logs only
CipSoft's messages, so read them from the FPS / latency indicator
(`worldmapBuildUs`) or a profiler, not from the log.

- Baseline: stock (`QTCLIENT_ENABLE_MODS=OFF` or `--no-mods`) on a fixed replay,
  e.g. `--replay=records/smoke-world.cam --play-for=20`.
- After WorldView / pack changes: stock within noise of the baseline
  (investigate a sustained >~2% frame-time increase); qtclient + raycast within
  noise of the previous raycast build on the same machine and replay.
- `ModLoader` runs only at startup; per frame only a cached
  `WorldLightingDriver*` + null check; no `mod.json` / directory / `QLibrary`
  work inside `advance()`.

## Verify

```bash
python3 scripts/ci/check-pack-abi-headers.py
make stock desktop build
```
