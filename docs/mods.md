# Mods and WorldView customization

## Stock = CipSoft

- `client/upstream/` — vendored CIP QML / shaders / strings (`make sync`)
- `launcher/upstream/` — CIP launcher chrome
- `client/tibia/<area>/` — `tibia::<area>` filesystem
- `launcher/src/` — tip-protocol launcher
- `shared/src/` — `shared::` types

Do not invent an OTC-style `modules/` tree that re-hosts stock CIP.
Keep the name **mods** / **pack** (not “plugins” — that collides with Qt QML plugins).

## Pack format (desktop + WASM — same tree)

```text
mods/<id>/
  mod.json              # id, QTCLIENT_MOD_API, autoload, priority, native, identity, qmlModule
  native/mod.*          # shared library / SIDE_MODULE (.so / .dylib / .dll / .wasm)
  qml/<qmlModule>/      # filesystem QML module (qmldir + *.qml)
  overrides/…           # optional CIP stock-path forks (runtime remap)
  resources/…           # optional assets
```

Build with one CMake API:

```cmake
find_package(QtClient REQUIRED)  # or include cmake/QtClientMod.cmake in-tree
qtclient_add_mod(my_pack
  SOURCES src/entry.cpp …
  QML_DIR qml
  QML_MODULE my_pack   # stages qml/<module>/ + generates qmldir
  OVERRIDES_DIR overrides
  MOD_JSON mod.json
)
```

Authors do **not** choose “plugin vs SIDE_MODULE”. The host loads packs with
`ModLoader::loadFromDirectory` (startup only) via `QLibrary` + `qtclient_mod_entry`.

```bash
# next to client binary, or tip OPFS mods/
mods/my_pack/mod.json
mods/my_pack/native/mod.so   # or .wasm on /play
mods/my_pack/qml/my_pack/qmldir
```

Stock CDN ships host only (`client` / `tibia.wasm`). Product packs are separate
artifacts installed into `mods/<id>/` before Play (`make idleot install` for local).

## Pack tiers

| Tier | What | When |
|------|------|------|
| Runtime pack | `native/mod.*` + `qml/<uri>/` + `resources/` + `mod.json` | Post-compile; CDN / `make idleot install` |
| Optional QML plugin | Shared `native/qmlplugin.*` for C++ types | Prefer over STATIC-into-`mod.*` |
| Override pack | CIP forks under `overrides/` | **Runtime** `PackQmlInterceptor` remap (host stays stock) |

### Filesystem QML modules (F5 hot reload)

Qt resolves `import <qmlModule>` as `$importPath/<qmlModule>/qmldir`.
ModLoader adds **`mods/<id>/qml`** as the import path (pack QML only — not
`overrides/`). Product C++ may keep `qrc:/qt/qml/<module>/…` URLs —
`PackQmlInterceptor` remaps them onto the FS module. Prefer
`makeModuleComponent(engine, "TypeName")` for `QQmlComponent` loads so sibling
types resolve after cache clears.

Do **not** bake STATIC `qt_add_qml_module` into the native MODULE — that fights
desktop F5 / disk hot reload.

| Build | QML source | Hot reload |
|-------|------------|------------|
| Umbrella `make qtclient client build` (`DEV=1` → `TIBIA_QML_DISK`) | Live pack `qml/` + override source trees via `DiskQmlInterceptor`; file watch + **F5** clears component cache | Yes (desktop) |
| Production `make -C apps/qtclient client build` | Staged `mods/<id>/qml/<uri>/` via `PackQmlInterceptor` | Edit → restage / rebuild pack tree |

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

`QTCLIENT_MOD_INTERNAL=1` is a **temporary, non-public** migration bridge for
in-tree IdleOT deep engine includes. New packs must use `mod_api_v1.h` only
(see [`client/mods/_sample`](../client/mods/_sample)). Do not rely on
`QTCLIENT_MOD_INTERNAL` in out-of-tree packs.

### Versioning

- `mod.json` → `QTCLIENT_MOD_API` must be in `[ModLoader::kMinApi, kSupportedApi]` (currently 1..1). Legacy key `modApi` is not read.
- Optional `hostBuildId` — warn (or reject if `QTCLIENT_MOD_STRICT_HOST=1`) when it differs from the host SHA baked into `mod_api_v1`.
- `qtclient_mod_entry(earlyCtx)` — **register-only** when `earlyCtx` is null (startup load); UI wiring runs in `earlyBootstrapAll` / `bootstrapAll`.

## Overrides

| Layer | Path | When applied |
|-------|------|----------------|
| Engine / Qt-only | `client/overrides/` (e.g. LegacyControls) | Configure-time merge into `qmlcomponents` qrc |
| Pack stock forks | `mods/<pack>/overrides/` (rel. to `client/upstream/qml/`) | **Runtime** remap via `PackQmlInterceptor` (same URI → pack FS) |
| Launcher | `launcher/overrides/` + `launcher/mods/<pack>/` | Launcher build |
| Dev F5 | pack `qml/` + `overrides/` on disk | `TIBIA_QML_DISK` / Pack interceptor; **F5** clears cache — no host rebuild |

Stock Soft hosts publish once (`QTCLIENT_ENABLE_MODS=OFF`). Product packs overlay
without baking forks into host qrc. `make tibia-sync --check` still scans pack
`overrides/` for `cip-base` drift.

## CMake discovery (dev)

- `QTCLIENT_ENABLE_MODS` (default ON)
- `QTCLIENT_MODS_DIR` (CMake + runtime) default `${TIBIA_ROOT}/client/mods`
- Empty `QTCLIENT_MODS` = all non-`_*` packs with `mod.json`
- Sample: [`client/mods/_sample`](../client/mods/_sample) — enable with `-DQTCLIENT_MODS=_sample`

Runtime loading does not require CMake discovery: drop a built pack next to the
binary and the host loads it at startup.

## WorldView customization

Stock renderer = CIP sprites + classic light map. Product packs plug in via
cached hooks (null-check only on the hot path):

| Extension | Role |
|-----------|------|
| `WorldLightingDriver` | Optional lighting (IdleOT raycast) |
| `WorldOverlayFactory` | QSGNode siblings after sprites |
| `WorldFrameHooks` | Request shared outdoor/roof gather |
| Sprite effect / HD hooks | `mods/registry` + `sprites/hdscale` |
| `ModContext.services` | Named `QObject*` for QML |

### Performance

- Load mods only at startup / pre-play.
- Per-frame: cached pointers + null checks; no `mod.json` I/O, no directory scans.
- Stock with no packs: CIP path with no product lighting linked.
- Oracle: see [development/mod-perf-oracle.md](development/mod-perf-oracle.md).

## Entry symbol

```c
extern "C" void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx);
```

Register mods, backends, WorldView drivers/overlays inside this function
(prefer `qtclient_mod_host_api_v1()`).

## Host + pack (FDW-style)

- **qtclient CI** publishes Soft host artifacts (`qtclient-host-<plat>:<sha>`, `qtclient-host-sdk:<sha>`).
- **idleot-client CI** builds MODULE/SIDE_MODULE against host-sdk; stamps `hostBuildId`.
- **idleot/release** waits for both, merges `mods/idleot` into the Soft tip, brands IdleOT, promotes CDN.
- Local: `make idleot client|wasm install` = Soft host (`STOCK_HOST=1`) → pack build → tip merge.

## Make CLI

```bash
make qtclient client build       # Soft host (stock or DEV disk QML)
make idleot client install       # Soft host → IdleOT MODULE → tip merge → CDN desktop
make idleot wasm install         # Soft MAIN_MODULE → SIDE_MODULE → tip merge → dist/cdn
make idleot client run           # install + Soft Play (loads mods/idleot at startup)
make -C apps/qtclient client build   # app-local Soft host
```
