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

## Linked tip (stock qtclient + product packs)

Product C++ **STATIC-links into qtclient** at tip build time. Runtime FS chrome stays a
drop-in tree next to the binary (F5 QML). There is **no** `native/mod.*`,
`MAIN_MODULE`, `SIDE_MODULE`, or `host_exports.json`.

```text
<linked qtclient tip>/                 # desktop binary dir, or tip with tibia.wasm
  tibia | tibia.js + tibia.wasm    # qtclient + N pack C++ in one binary
  mods/<id>/                       # one or more packs
    mod.json                       # id, QTCLIENT_MOD_API, urls?, identity?, qmlModule?
    qml/…  overrides/…  resources/…
```

| qtclient build | Result |
|------------|--------|
| Stock (`STOCK_HOST=1` / no linked packs) | CIP tip; FS urls/identity/QML packs only |
| Product (`-DQTCLIENT_LINKED_PACK=` / `LINKED_PACKS=` / `LINKED_PACK_LIBS=`) | qtclient⊃N pack C++ + staged `mods/<id>/` |

Local product path: `make qtclient client|wasm install` (qtclient tip with
`QTCLIENT_LINKED_PACK`, then FS sync onto `dist/cdn`). Portal tip is
`IDLEOT_ASSET_BASE` (local nginx `:9080`).

## Pack format (desktop + WASM — same tree)

```text
mods/<id>/
  mod.json              # id, QTCLIENT_MOD_API, autoload, priority, identity?, urls?, qmlModule?
  qml/<qmlModule>/      # filesystem QML module (qmldir + *.qml)
  overrides/…           # optional CIP stock-path forks (runtime remap)
  resources/…           # optional assets
```

### Multi-pack tip + `urls`

Tip loads **all** autoload packs under `mods/<id>/` (priority descending so lower
`priority` numbers win override remap). Identity / urls still first-wins after
ascending priority sort.

| Mode | Packs |
|------|-------|
| `--no-mods` / empty tip | none → real CipSoft (`https://www.tibia.com`, `https://static.tibia.com`) |
| `--default-mod` | force tip `mods/default/` only |
| tip default | all autoload `mods/<id>/` |
| `QTCLIENT_MODS_DIR` | all autoload packs under that root (or one dir with `mod.json`) |
| `QTCLIENT_EXTRA_MODS_DIR` (DEV) | additional FS-only packs appended after tip packs |

**Product local path:** set `QTCLIENT_PACK` in the umbrella `.env`, then
`make qtclient client|wasm install` builds qtclient with `QTCLIENT_LINKED_PACK` and
stages FS under tip `mods/<id>/`. Multi source: `-DQTCLIENT_LINKED_PACKS=a;b`.
`QTCLIENT_MODS_DIR` remains a qtclient escape hatch for out-of-tree experiments.

Optional `urls` on `mod.json` merge into `LauncherEnv` (env > mod > `release_env.lua` > CipSoft).

```json
"urls": {
  "loginBase": "http://127.0.0.1",
  "loginPath": "/login.php",
  "clientServicesPath": "/login.php",
  "createAccountPath": "/login.php",
  "pages": { "QTCLIENT_URL_CREATE_ACCOUNT": "/?subtopic=createaccount" }
}
```

| Pack | Role |
|------|------|
| [`mods/default`](../client/mods/default/mod.json) | Demo tip myAAC (`/login.php`); `make demo-pack` |
| Product `mods/<id>/` | Author pack (linked C++ + QML + optional `brand.json`); tip **replaces** `default` |

Urls-only packs (stock qtclient) omit linked C++. Product packs rebuild qtclient with the pack.

## Tip mods channel (CDN / launcher)

qtclient is a **generic CipSoft host**. Product chrome is a tip install channel next to
engine (`url`/`versionurl`) and assets (`assetsurl`):

```text
packages/<directoryname>/
  Contents/…                 # CIP engine (unchanged)
  mods/<id>/                 # one or more product packs (from mods.json)
    brand.json               # optional launcher chrome (at least one pack)
    mod.json
    qml/…  overrides/…  resources/…  images/launcher/…
  mods.json                  # files[] across all packs + packs[] metadata
  mods.json.sha256           # stamp written by PackageClient::installMods
```

`tibiametadata.json` `packages[].builds[]` may include:

```json
"modsurl": "…/mods-current/mods.json",
"modsversionurl": "…/mods-current/mods.json.sha256"
```

Empty / omitted = stock CIP (no tip pack). `mods.json` mirrors `assets.json`
(`files[]` with `localfile` under `mods/<id>/…`; optional `packs[]` lists each
id/priority). Launcher Play requires engine + assets + **mods** stamps to match
when the channel is present. Emit with
`apps/release/scripts/emit-mods-stamp.py <tip/mods> <tip>`.

### Atomic ModLoader

When the tip was built with `QTCLIENT_HAS_LINKED_PACK`:

1. Call `qtclient_invoke_linked_mods` **once** (every `qtclient_mod_<id>_entry`)
2. Then register identity / urls / QML / overrides from **each** FS `mods/<id>/`

Stock qtclient (no linked pack): FS urls/identity/QML only; a leftover `native/` tree fails closed.

### Author path (any product)

1. Pin qtclient SHA / client-sdk
2. Define `qtclient_mod_<id>_register` (C++) + `qtclient_add_mod(<id> …)` under pack `client/` (tip generates `extern "C"` entry wrappers)
3. Build tip with `-DQTCLIENT_LINKED_PACK=<pack>/client` (or `LINKED_PACKS` / `LINKED_PACK_LIBS`)
4. Publish tip + `mods.json` lzma blobs on the CDN
5. Point `tibiametadata` `modsurl` / `modsversionurl` at that tip

Build with one CMake API:

```cmake
include(cmake/QtClientMod.cmake)  # qtclient tip already has target tibia
qtclient_add_mod(my_pack
  SOURCES src/entry.cpp …   # must define qtclient_mod_my_pack_register
  QML_DIR qml
  QML_MODULE my_pack   # stages qml/<module>/ + generates qmldir
  OVERRIDES_DIR overrides
  MOD_JSON mod.json
)
```

Authors do **not** choose “plugin vs SIDE_MODULE”. qtclient owns the tip link;
`ModLoader::loadAll` invokes the linked entry table then FS chrome for every pack.

### WASM linked tip

1. qtclient tip is a normal Emscripten executable (no `MAIN_MODULE` / pack `SIDE_MODULE`).
2. Product packs STATIC-link via `QTCLIENT_LINKED_PACK(S)` (keep each
   tip-generated `qtclient_mod_<id>_entry` + `qtclient_invoke_linked_mods` with `LINKER:--undefined=` on WASM).
3. Gates:
   - qtclient CI: `_sample` STATIC linked canary + no `native/mod.wasm` under tip
   - Tip presence: `check-tip-pack-presence.sh` (`mods/<id>/mod.json` + content-store)
4. Optional `hostBuildId` on `mod.json` (strict by default on desktop + WASM;
   set `QTCLIENT_MOD_STRICT_HOST=0` to warn-only).

```bash
# next to client binary, or tip OPFS mods/
mods/my_pack/mod.json
mods/my_pack/qml/my_pack/qmldir
```

Stock qtclient CDN ships host only. Product tips rebuild qtclient with packs linked, then
stage FS chrome. Release refuses a tip whose tree still contains `native/`.

## Pack tiers

| Tier | What | When |
|------|------|------|
| Linked tip | qtclient⊃N pack C++ + FS `qml/` + `resources/` + `mod.json` | Tip build; CDN / `make qtclient install` |
| Prebuilt archive | `libqtclient_mod_<id>.a` via `QTCLIENT_LINKED_PACK_LIBS` | Pack CI → tip link (`archive-mod.sh`) |
| Override / FS-only pack | CIP forks + QML under `mods/<id>/` | **Runtime** remap (stock host OK) |

### Pack archive

```bash
# After a linked tip build:
bash apps/qtclient/scripts/lifecycle/package/archive-mod.sh \
  --id idleot --build-dir apps/qtclient/build/<preset> \
  --out /tmp/archive-idleot

# Tip link without recompiling pack sources:
cmake … -DQTCLIENT_LINKED_PACK_LIBS="idleot=/tmp/archive-idleot/lib/libqtclient_mod_idleot.a"
# FS chrome still staged from archive mods/idleot/ (or QTCLIENT_LINKED_PACK sources).
```

Curated SDK-only packs (`_sample`): build with `-DQTCLIENT_PACK_SDK_ONLY=ON` and
`QTCLIENT_HOST_INCLUDE_DIR` from `pack_sdk` (`client-sdk` artifact). Gate:
`scripts/ci/check-pack-sdk-only.sh`.

### Filesystem QML modules (F5 hot reload)

Qt resolves `import <qmlModule>` as `$importPath/<qmlModule>/qmldir`.
ModLoader adds **`mods/<id>/qml`** as the import path (pack QML only — not
`overrides/`). Product C++ may keep `qrc:/qt/qml/<module>/…` URLs —
`PackQmlInterceptor` remaps them onto the FS module. Prefer
`makeModuleComponent(engine, "TypeName")` for `QQmlComponent` loads so sibling
types resolve after cache clears.

Do **not** bake STATIC `qt_add_qml_module` into the tip — that fights desktop F5.

| Build | QML source | Hot reload |
|-------|------------|------------|
| Umbrella `make qtclient client build` (`DEV=1` → `TIBIA_QML_DISK`) | Live pack `qml/` + override source trees via `DiskQmlInterceptor`; file watch + **F5** clears component cache | Yes (desktop) |
| Umbrella `make stock client build` | qtclient stock host (no pack) | — |
| Production `make -C apps/qtclient client build` | Staged `mods/<id>/qml/<uri>/` via `PackQmlInterceptor` | Edit → restage / rebuild tip |

## Mod API v1

Public header: [`include/qtclient/mod_api_v1.h`](../include/qtclient/mod_api_v1.h).

Resolve with `qtclient_mod_host_api_v1()` (exported from the host). Surface:

| Hook | Role |
|------|------|
| `registerMod` / `registerEarlyBootstrap` | Identity + bootstrap callbacks |
| `setEffectTintHook` / variant / apply | Sprite effect tints |
| `setMissionBackend` / `setAttentionBackend` / `setInstanceBackend` | Product backends |
| `setLoginSyncEnqueueHook` | Login-thread work queue |
| `setWorldLightingDriver` / `addWorldOverlayFactory` / `setWorldFrameHooks` | WorldView |
| `setWeatherDriver` | Weather overlay + sound |
| `setHdScaler` | HD sprite scaler |

Brand identity comes from `mod.json` (`identity` + `priority`) via ModLoader.
Packs register feature bootstraps only (do not re-register a second brand id).

`QTCLIENT_MOD_INTERNAL=1` is a **temporary, non-public** `#ifdef` bridge for
engine-deep hooks during migration — it is **not** the tip defs gate. Tip
`INCLUDE_DIRECTORIES` and tip `COMPILE_DEFINITIONS`/`OPTIONS` (notably
`QT_NO_KEYWORDS`) always flow via `QtClientMod.cmake` so packs that still
compile tip headers get matching keyword/defs. New packs must use `mod_api_v1.h`
only (see [`client/mods/_sample`](../client/mods/_sample)). Do not rely on
`QTCLIENT_MOD_INTERNAL` in out-of-tree packs. IdleOT **INTERNAL unset Done**
(`QTCLIENT_MOD_INTERNAL=0`). Pack includes are named / domain façades under
`include/qtclient/` named / domain façades only (no `qtclient/tip/…` tree).
`QTCLIENT_PACK_SDK_ONLY=ON` + tip closure is the pack include contract.
`QTCLIENT_MOD_GUI_PRIVATE` remains the RHI opt-in.

`QTCLIENT_MOD_GUI_PRIVATE=1` is a **narrow** opt-in that links `Qt6::GuiPrivate`
(and `QuickPrivate` when present) for RHI headers (`rhi/qrhi.h`, `rhi/qshader.h`)
without requiring deep tip includes. IdleOT keeps GuiPrivate after
`MOD_INTERNAL` unset.

Pack SDK: [`include/qtclient/`](../include/qtclient/README.md) (copied by
`pack_sdk` + tip closure). Prefer
[`include/qtclient/session.h`](../include/qtclient/session.h)
(`localPlayer`, `isOnline`, `cipFeature`) over deep tip includes
unless the TU needs wire helpers (`useMapObject`, `talk`, …).

### Versioning

- `mod.json` → `QTCLIENT_MOD_API` must be in `[ModLoader::kMinApi, kSupportedApi]` (currently 1..1). Legacy key `modApi` is not read.
- Optional `hostBuildId` — reject on mismatch by default (desktop + WASM). Set
  `QTCLIENT_MOD_STRICT_HOST=0` to warn and continue (DEV).
- Pack `qtclient_mod_<id>_register` + tip `qtclient_mod_<id>_entry` / `qtclient_invoke_linked_mods` — **register-only** when `earlyCtx` is null (startup load); UI wiring runs in `earlyBootstrapAll` / `bootstrapAll`.

## Overrides

| Layer | Path | When applied |
|-------|------|----------------|
| Engine / Qt-only | `client/overrides/` (e.g. LegacyControls) | Configure-time merge into `qmlcomponents` qrc |
| Pack stock forks | `mods/<pack>/overrides/` (rel. to `client/upstream/qml/`) | **Runtime** remap via `PackQmlInterceptor` (same URI → pack FS) |
| Launcher | `launcher/overrides/` + `launcher/mods/<pack>/` | Launcher build |
| Dev F5 | pack `qml/` + `overrides/` on disk | `TIBIA_QML_DISK` / Pack interceptor; **F5** clears cache — no client rebuild |

Stock qtclient publishes once. Product tips rebuild qtclient with `QTCLIENT_LINKED_PACK`.
`make tibia-sync --check` still scans pack `overrides/` for `cip-base` drift.

## CMake discovery (dev)

- `QTCLIENT_ENABLE_MODS` (default ON)
- `QTCLIENT_MODS_DIR` (CMake + runtime) default `${TIBIA_ROOT}/client/mods`
- `QTCLIENT_LINKED_PACK` — singular out-of-tree pack `client/` STATIC into tip
- `QTCLIENT_LINKED_PACKS` — semicolon list of out-of-tree pack `client/` dirs
- `QTCLIENT_LINKED_PACK_LIBS` — `id=/path/to/libqtclient_mod_<id>.a` prebuilt archives
- `QTCLIENT_PACK_SDK_ONLY` — pack compile against client-sdk includes only
- Empty `QTCLIENT_MODS` = all non-`_*` packs with `mod.json`
- Sample: [`client/mods/_sample`](../client/mods/_sample) — enable with `-DQTCLIENT_MODS=_sample`

## Protocol vs CIP spine

Stock game frames follow CipSoft order: Net → TR → `TProtocolMessageQueue` →
handlers (`TClassicProtocolBridgeHandler` → classic `parse*` today). Product
extensions stay on **ExtendedOpcode** (tip-local); they must not hang new
handlers on the CIP queue. Crystal remains classic binary wire.

## WorldView customization

Stock renderer = CIP sprites + classic light map. Product packs plug in via
cached hooks (null-check only on the hot path):

| Extension | Role |
|-----------|------|
| `WorldLightingDriver` | Optional lighting (pack raycast / custom). **POD-only** like HD scaler: tip peels `ThingType`/`Thing`/`TCreature`/`TWorldMapField`/`Position`/`Point` into `LightingMapPos` / `LightingThingContext` / `LightingFrameDesc` (`mods/lighting_types.h`). Packs call `beginContext` → `addSprites`/`addFrame` → `endContext` (or `ContextScope`); no tip hub types on the driver API |
| `WorldOverlayFactory` | QSGNode siblings after sprites |
| `WorldFrameHooks` | Request shared outdoor/roof gather |
| Sprite effect / HD hooks | `mods/registry` + `sprites/hdscale` + `sprites/spritesession` (`QImage` peel) |
| `ModContext.services` | Named `QObject*` for QML |

### Performance

- Load mods only at startup / pre-play (all tip packs once).
- Per-frame: cached pointers + null checks; no `mod.json` I/O, no directory scans.
- Stock with no packs: CIP path with no product lighting linked.
- Oracle: see [development/mod-perf-oracle.md](development/mod-perf-oracle.md).

## Entry symbols

```c
// each pack (C++, absorbed into tibia)
void qtclient_mod_<id>_register(const tibia::mods::ModContext* earlyCtx);
// tip-generated linked_mods.cpp
extern "C" void qtclient_mod_<id>_entry(const tibia::mods::ModContext* earlyCtx);
extern "C" void qtclient_invoke_linked_mods(const tibia::mods::ModContext* earlyCtx);
```

Register mods, backends, WorldView drivers/overlays inside the pack entry
(prefer `qtclient_mod_host_api_v1()`). Linked tips keep entries past DCE.

## Client + pack

- **`Client: ci`** (public) publishes qtclient (`client-<plat>:<sha>`, `client-sdk:<sha>`). Desktop and wasm both absorb `_sample` into `tibia` as a pack canary (same tip `/FI` pch + tip-generated `extern "C"` wrappers) on every kit — including MSVC — before release `pack-windows`.
- Product release rebuilds qtclient with `QTCLIENT_LINKED_PACK`, stages FS chrome, brands the tip, promotes CDN. When pack or host compile surface changed, dispatch `idleot/release` Build client and wait for **pack-windows** green before cutting a tag.
- Local: `make qtclient client|wasm install` builds qtclient with the pack linked and stages `mods/<id>`. Run loads tip `mods/<id>` only. `make stock client run` / `make stock launcher run` pass `--no-mods`.

## Make CLI

```bash
make stock client build          # CIP host (or DEV disk QML)
make stock client run            # --no-mods (stock client)
make stock launcher run          # --no-mods (stock launcher + CIP chrome)
make qtclient client install     # qtclient tip + QTCLIENT_LINKED_PACK + FS chrome
make qtclient launcher install   # qtclient launcher + tip brand
make qtclient wasm install       # qtclient wasm tip + QTCLIENT_LINKED_PACK + FS chrome
make qtclient client run         # start the linked tip; warns if tip mods/<id> missing
make stock demo-pack && make stock demo-run  # tip with mods/default + CipSoft assetsurl
make stock client run ARGS='--default-mod'   # force myAAC tip pack
make stock client run ARGS='--no-mods'       # real CipSoft URLs
make -C apps/qtclient client build           # app-local qtclient stock
```
