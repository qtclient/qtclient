# Mods and packs

Product / third-party packs on the CipSoft qtclient host (host API v9). A pack
builds **apart from the host**, against the pack SDK only; the host takes the
installed pack as an input. Desktop ships the pack as a runtime MODULE
(`native/mod.*`); WASM links it as a static-ABI archive into `tibia.wasm`. Both
use the same pack sources and the same FS chrome.

Frozen tip extension hooks (CIP-neutral only):
[pack-extension-contract.md](pack-extension-contract.md)
(`scripts/ci/check-cip-tip-purity.py`). Pack SDK headers:
[`include/qtclient/abi/`](../include/qtclient/README.md); SDK package:
[`sdk/`](../sdk/CMakeLists.txt) (`find_package(QtClientSdk)`).

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
| Pack builds against `QtClientSdk` only | No host build dir, host sources or tip targets on the pack; the host takes the *installed* pack |
| Packs export only `qtclient_mod_register` (MODULE) / `qtclient_mod_<c_id>_register_abi` (static-ABI) | One entry, written once with `QTCLIENT_MOD_DEFINE_REGISTER(c_id)` |
| Zero tip imports | A pack resolving tip symbols (export lists) was the past pain; `check-pack-zero-tip-imports.sh` |
| Desktop = runtime MODULE linking the tip's (shared) Qt | One Qt per process; pack `QObject`s work with the host's `QQmlEngine` |
| WASM = static-ABI, not a SIDE_MODULE | The CIP wasm Qt kit is static: a Qt SIDE_MODULE would need a second Qt (second `QCoreApplication`, QML registry, resource tree) or a `MAIN_MODULE=1` tip; pthreads + dylink are experimental in Emscripten |
| FS QML + overrides | F5 / `PackQmlInterceptor`; not tip-baked `qt_add_qml_module` |
| No qrc paths that shadow tip / CIP resources | Qt resolves a qrc path to the first-registered root; a pack registers after the tip and `graphics_resources.rcc`. Reskin through FS `overrides/` QML pointing at pack images (`qrc:/qt/qml/<module>/images/…`) |

Removed paths: tip export lists / `host_exports.json` (→ host-pushed API
table), packs importing `qtclient_mod_host_api_v1()` (→ the
`qtclient_mod_register(api, ctx)` argument), desktop-only `QPluginLoader`,
`ABSORB_INTO_QTCLIENT` / `qtclient_link_mod_archive` / `QTCLIENT_LINKED_PACK_LIBS`
(pack C++ compiled into or archive-linked by the tip against tip headers),
`QTCLIENT_LINKED_PACK(S)` (the host building the pack), the non-abi
`include/qtclient/*.h` façades and the tip-closure client-sdk.

Non-goals: tip export allowlists for packs; packs importing tip symbols;
process-isolated sidecar mods (lighting / opcodes stay in-process).

## Artifacts

```text
pack SDK (sdk/, build/sdk.sh)    → QtClientSdk: include/qtclient/abi/ + lib/cmake/QtClientSdk/
                                   (QtClientMod.cmake; records ABI version, hostBuildId, Qt kit)
pack (pack CI / build/pack.sh)   → <prefix>/mods/<id>/{mod.json, native/mod.* (desktop), qml/, …}
                                   + wasm: <prefix>/lib/libqtclient_mod_<id>.a + lib/cmake/QtClientPack_<c_id>/
stock host (qtclient CI)         → tibia tip (+ optional default FS packs)
product tip (release/local)      → host built with -DQTCLIENT_PACKS=<pack prefix>[;…]
```

## Build (SDK → pack → host)

The three builds are separate CMake projects. Only the SDK comes from the host
tree, and it compiles nothing.

```bash
# 1. pack SDK for this qtclient commit + Qt kit (desktop, or --wasm)
bash apps/qtclient/scripts/lifecycle/build/sdk.sh            # → apps/qtclient/build/qtclient-sdk/desktop
# 2. the pack against the SDK alone (any directory; no host source path)
cmake -S apps/idleot-client -B build/pack -G Ninja \
  -DCMAKE_PREFIX_PATH=apps/qtclient/build/qtclient-sdk/desktop
cmake --build build/pack && cmake --install build/pack --prefix build/pack/install
# 3. the host, taking the installed pack
QTCLIENT_PACKS=$PWD/build/pack/install bash apps/qtclient/scripts/lifecycle/build/desktop.sh
```

`make qtclient desktop|browser install` runs exactly these three steps
(`QTCLIENT_PACK` from `.env`; the pack's `scripts/lifecycle/build/pack.sh` wraps
step 2). WASM: `sdk.sh --wasm`, the pack configured with the wasm Qt toolchain
(`-DCMAKE_TOOLCHAIN_FILE=<wasm Qt>/lib/cmake/Qt6/qt.toolchain.cmake`), then
`build/wasm.sh` with `QTCLIENT_PACKS`.

A pack's CMake:

```cmake
cmake_minimum_required(VERSION 3.22)
project(my-pack LANGUAGES CXX)
find_package(QtClientSdk 9 REQUIRED CONFIG NO_CMAKE_FIND_ROOT_PATH)  # major = host ABI
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)             # the SDK's kit
qtclient_add_mod(my-pack
  SOURCES src/modpack.cpp …
  QML_DIR qml OVERRIDES_DIR overrides RESOURCES_DIR resources MOD_JSON mod.json
  [GUI_PRIVATE])                         # Qt6::GuiPrivate/QuickPrivate (RHI nodes)
qt_add_resources(qtclient_mod_my-pack res FILES … OUTPUT_TARGETS _out)  # wasm: export these
qtclient_mod_install(my-pack EXTRA_TARGETS ${_out} [BRAND_FILES brand.cmake …])
```

| `QtClientSdk` gives | |
|---|---|
| `QtClientSdk::Abi` | the abi include dir (the only host headers) |
| `QTCLIENT_SDK_ABI_VERSION` / package major | host API version (`QTCLIENT_HOST_API_V1_VERSION`) |
| `QTCLIENT_SDK_HOST_BUILD_ID` | stamped into the staged `mod.json` `hostBuildId` (strict match at load) |
| `QTCLIENT_SDK_QT_VERSION` / `_QT_PREFIX` / `_QT_STATIC` | the host's Qt; `qtclient_add_mod` fails on another Qt version |
| `QTCLIENT_SDK_PLATFORM` / `_PACK_MODE` | `MODULE` (desktop) or `STATIC_ABI` (wasm); a wasm SDK refuses a desktop configure and vice versa |

`qtclient_add_mod` puts only the pack's own dirs and the abi include dir on the
pack (no tip PCH, defines `QT_NO_KEYWORDS`, `QTCLIENT_MOD_ID`, `QTCLIENT_MOD_C_ID`,
and `QTCLIENT_MOD_STATIC_ABI` in static-ABI mode), hidden visibility, `AUTOMOC`
/ `AUTORCC`. It refuses to run inside the host build (`TARGET tibia`).

Host inputs (`cmake/TibiaMods.cmake`):

| Variable | Role |
|---|---|
| `QTCLIENT_PACKS` | installed pack prefixes; desktop stages `mods/<id>` (incl. `native/`) next to the tip; wasm links `QtClientPack::<c_id>` and generates `static_abi_mods.cpp` (`qtclient_static_abi_mod_entry(id)` for ModLoader); `share/qtclient-pack/<id>/brand.cmake` sets the product identity / icon |
| `QTCLIENT_DEV_PACK_SOURCES` | desktop DEV only: pack `client/` source dirs for disk QML + F5 (no build) |
| `QTCLIENT_LINKED_PACK(S)`, `QTCLIENT_LINKED_PACK_LIBS` | removed — configure fails with the migration message |

CI gate: `apps/qtclient/scripts/ci/check-pack-sdk-build.sh --pack <dir> [--wasm]`
installs the SDK outside the tree, configures / builds / installs the pack with
`CMAKE_PREFIX_PATH=<sdk>` only, fails on a host path in the pack build graph,
and checks the installed tree (desktop `native/mod.*`, wasm export + archive)
with `check-pack-zero-tip-imports.sh`. Release (`build-pack.yml`) uses this gate
as the pack build. Umbrella: `scripts/ci/check-pack-sdk-build.sh` (`make ci`).

## Runtime MODULE (desktop)

Product native code is a **detached MODULE** next to the tip. The tip never
relinks when pack C++ changes (rebuild the pack, re-stage `mods/<id>`).

```text
<qtclient tip>/                      # macOS: <App>.app/Contents/MacOS/
  client
  mods/<id>/
    mod.json
    native/mod.{dylib,so,dll}        # qtclient_mod_register
    qml/…  overrides/…  resources/…
```

- Links Qt from the SDK's (shared) kit — the same one the tip bundles. RPATH:
  macOS `@loader_path/../../../../Frameworks` (+ the kit for build-tree runs;
  `package_desktop.py` drops it and runs `macdeployqt -executable=` on the
  module), Linux `$ORIGIN/../../../lib`, Windows by name.
- macOS signing is inside-out: `mod.dylib` is signed before the app
  (`package_desktop.py` ad-hoc; release Developer ID + notarization). Windows
  Trusted Signing signs `mod.dll`.
- `mod.json` `hostBuildId` must equal the host's (the SDK stamps it); compare
  `api->qt_version` with `QT_VERSION_STR` when using GuiPrivate.
- No C++ across the boundary: no exceptions out of `qtclient_mod_register` or
  any callback, no `std::any` / `std::function` / `QString` in the ABI.

| qtclient build | Result |
|------------|--------|
| Stock (`--no-mods`) | CIP tip; no packs |
| Default tip mods | `qtc-aac` FS urls |
| Product (`-DQTCLIENT_PACKS=<prefix>`) | stages the installed pack (desktop MODULE / wasm static-ABI) |

Local product path: `make qtclient desktop|browser install`.

### Load sequence

```text
ModLoader::loadAll
  → for each mods/<id>/mod.json (hostBuildId strict)
      → desktop: QLibrary native/mod.* → qtclient_mod_register(&hostApi, &loadCtx)
        wasm:    qtclient_static_abi_mod_entry(id) → qtclient_mod_<c_id>_register_abi(&hostApi, &loadCtx)
      → register FS qml/ overrides/ urls/identity
```

Packs never look up tip symbols. Extending capability means adding fields at
the tail of a table in [`include/qtclient/abi/`](../include/qtclient/abi/)
(`QTCLIENT_ABI_HAS`).

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
  native/mod.*          # desktop MODULE only (wasm: none — the C++ is in tibia.wasm)
  qml/<qmlModule>/
  overrides/…
  resources/…
```

### Host-pushed register

```c
#include <qtclient/abi/mod_register.h>

/* c_id = mod.json "id" as a C identifier ('-' / '.' → '_'). */
QTCLIENT_MOD_DEFINE_REGISTER(my_pack)(const QtClientHostApiV1* api, const QtClientModLoadCtx* ctx)
{
    api->register_mod(/* … */);
}
```

The macro expands to the exported `qtclient_mod_register` in a MODULE build and
to `extern "C" qtclient_mod_<c_id>_register_abi` when `QTCLIENT_MOD_STATIC_ABI`
is defined (wasm). Packs must **not** import tip symbols; use only `api`
function pointers. CI: `scripts/ci/check-pack-abi-headers.py` (raw `void*` only
as `user` / `self` / `ptr`; Qt objects as `QtClientQ*Ptr`).

### Notify payloads (`notify->arg_json`)

Field hooks get `const QtClientNotifyArgs*`. `arg_count` / `arg_kind`
(`QTCLIENT_NOTIFY_ARG_*`) describe each argument; scalars read with `arg_int` /
`arg_bool` / `arg_string`, structured ones with `arg_json` (UTF-8 JSON into a
caller buffer; returns the full length — retry with a larger buffer). Structs
are objects keyed by member name, vectors and tuples are arrays, maps are
objects keyed by the decimal key, `Position` is `{x,y,z}`. Field names keep the
host's spelling (including CipSoft's typos such as `diffculty`, `ocorrence`,
`asignedStatus`).

| Field | Arguments |
|-------|-----------|
| `onBossCooldown` | 0: `[{bossRaceId, cooldownTime}]` |
| `onUpdateBestiaryMonsterData` | 0: `{id, bestClass, currentLevel, AnimusMasteryBonus, AnimusMasteryPoints, killCounter, thirdDifficulty, secondUnlock, lastProgressKillCount, difficulty, ocorrence, loot: [{itemId, diffculty, specialEvent, name, amount}], charmValue, attackMode, maxHealth, experience, speed, armor, mitigation, combat: {<element>: value}, location, leaderKilled, castsSpells}` |
| `onUpdateBestiaryCharmsData` | 0: `{resetAllCharmsCost, availableCharmSlots, points, charms: [{id, name, description, unlockPrice, unlocked, asignedStatus, raceId, removeRuneCost, availableCharmSlots, tier}], finishedMonsters: [raceId]}` |
| `onParseCyclopediaTracker` | 0: tracker type (int: 0 monsters, 1 bosses); 1: `[[raceId, kills, firstUnlock, secondUnlock, lastUnlock, completed]]` |
| `onCyclopediaMapDiscoveryArea` | 0: area id; 1: flag (0/1); 2: target; 3: `[[x, y, z, state]]` |
| `onTeamFinderList` | 0: rate limited (bool); 1: `[{leaderGuid, leaderName, minLevel, maxLevel, vocationIDs, teamSlots, membersCount, timestamp, teamType, bossID, huntType, huntArea, questID, status}]` |
| `onTeamFinderLeader` | 0: `{reset, minLevel, maxLevel, vocationIDs, teamSlots, freeSlots, timestamp, teamType, bossID, huntType, huntArea, questID, members: [{guid, name, level, vocation, status}]}` |

Other vectors of ints / strings come out as JSON arrays; an argument the host
cannot serialize is `QTCLIENT_NOTIFY_ARG_OTHER`. The host serializer is
`client/tibia/mods/notify_abi.cpp` (`jsonOf`); a new structured payload adds a
`jsonOf` there and a row here.

Besides the field bus, `notify` has two cross-session subscriptions:
`subscribe_containers` (CipSoft `TContainerStorage` containerUpdated /
containerRemoved, `QTCLIENT_CONTAINER_*`) and, appended at the end of the v9
table, `subscribe_quick_loot_user_changed` (the current session's
`TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput`).

### Rust packs

qtclient stays C++, but a pack may be a Rust `cdylib`: bindgen
[`include/qtclient/abi/*.h`](../include/qtclient/abi/), export
`qtclient_mod_register` with the `host_api_v1.h` signature, use only the passed
`QtClientHostApiV1*` (never link tip symbols), and stage the library as
`mods/<id>/native/mod.{dylib,so,dll}` next to the FS chrome (a Qt-free pack is
the one kind that can also ship `native/mod.wasm` as a wasm SIDE_MODULE; the tip
then links `MAIN_MODULE=2`). Working
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
| Native C++ | Each pack’s entry via ModLoader (desktop `qtclient_mod_register`, wasm static-ABI table) |
| QML import paths | Append all `mods/<id>/qml` |
| Overrides | Later load wins (`PackQmlInterceptor` reverse scan) |
| Exclusive hooks | Lighting / HD / texture atlas hint: first setter wins; conflicts are logged |
| Additive hooks | Overlay factories append |

CMake: `-DQTCLIENT_PACKS=<prefix>` (one installed pack) or
`-DQTCLIENT_PACKS=<a>;<b>` (several), each built against the SDK.

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

1. Qt packs are **static-ABI**: the same sources built against the wasm SDK into
   `libqtclient_mod_<id>.a` (exported `QtClientPack::<c_id>`), which the host
   build links; `static_abi_mods.cpp` names `qtclient_mod_<c_id>_register_abi`.
   The tip stays a plain executable (no `MAIN_MODULE`), and relinks when pack C++
   changes. The archive is gated alone (`check-pack-zero-tip-imports.sh
   --archive`), since the final link would satisfy a tip import.
2. `/play` packaging skips `native/` (`dev_cdn.py`, `cip_package.py`
   `WASM_PACK_SKIP_DIRS`); desktop slices keep it.
3. Only a Qt-free pack may stay a `SIDE_MODULE=2` (`native/mod.wasm`); then the tip
   links `MAIN_MODULE=2` (`TibiaMods.cmake`).
4. No tip export allowlist; capability surface is `QtClientHostApiV1`.
5. Optional `hostBuildId` on `mod.json` (strict by default).

## Filesystem QML (F5)

ModLoader adds `mods/<id>/qml`; F5 clears the QML cache on desktop DEV. With
`QTCLIENT_DEV_PACK_SOURCES` the DEV tip reads the pack's source `qml/` and
`overrides/` directly (edit, F5), without rebuilding the pack.

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
python3 scripts/ci/check-idleot-abi-progress.py
bash scripts/ci/check-pack-sdk-build.sh            # pack against the installed SDK alone
make stock desktop build
```
