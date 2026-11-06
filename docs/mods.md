# Mods and WorldView customization

Frozen tip extension hooks (CIP-neutral only):
[pack-extension-contract.md](pack-extension-contract.md)
(`scripts/ci/check-cip-tip-purity.py`).

Architecture overview: [`docs/pack-architecture.md`](../../../docs/pack-architecture.md).

## Stock = CipSoft

- `client/upstream/` — vendored CIP QML / shaders / strings (`make sync`)
- `launcher/upstream/` — CIP launcher chrome
- `client/tibia/<area>/` — `tibia::<area>` filesystem
- `launcher/src/` — tip-protocol launcher
- `shared/src/` — `shared::` types

Do not invent an OTC-style `modules/` tree that re-hosts stock CIP.
Keep the name **mods** / **pack** (not “plugins” — that collides with Qt QML plugins).

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
| Default tip mods | `otclient` MODULE + `myaac` FS urls |
| Product (`-DQTCLIENT_LINKED_PACK=`) | stages pack; IdleOT still `ABSORB_INTO_TIP` until abi peel |

Local product path: `make qtclient client|wasm install`.

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

### Multi-pack tip + `urls`

Tip loads **all** autoload packs under `mods/<id>/` (priority descending so lower
`priority` numbers win override remap). Identity / urls still first-wins after
ascending priority sort.

| Mode | Packs |
|------|-------|
| `--no-mods` / empty tip | none → real CipSoft |
| tip default | all autoload `mods/<id>/` |
| `QTCLIENT_MODS_DIR` | packs under that root |
| `QTCLIENT_EXTRA_MODS_DIR` (DEV) | additional FS packs |

Optional `urls` on `mod.json` merge into `LauncherEnv`.

| Pack | Role |
|------|------|
| [`mods/myaac`](../client/mods/myaac/mod.json) | Default tip myAAC |
| [`mods/otclient`](../client/mods/otclient/mod.json) | Crystal `isOTCR` wire (runtime MODULE) |
| Product `mods/<id>/` | Author pack |

### WASM

1. Tip links with `MAIN_MODULE=2` only when runtime MODULE packs are enabled
   (`TibiaMods.cmake`) — packs are `SIDE_MODULE=2` with **zero tip imports**.
2. No tip export allowlist; capability surface is `QtClientHostApiV1`.
3. Optional `hostBuildId` on `mod.json` (strict by default).

## IdleOT transitional absorb

IdleOT still uses tip C++ façades (`session.h`, `worldview.h`, …). Until those
are peeled onto `abi/`, `qtclient_add_mod(idleot ABSORB_INTO_TIP …)` compiles
pack sources into `tibia`. New packs must use the runtime MODULE path.

## Filesystem QML (F5)

Unchanged: ModLoader adds `mods/<id>/qml`; F5 clears QML cache on desktop DEV.

## Verify

```bash
python3 scripts/ci/check-pack-abi-headers.py
make stock client build
# with QTCLIENT_MODS=otclient — expect mods/otclient/native/mod.*
```
