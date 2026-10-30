# AGENTS.md

## AGENTS.md Loading Budget

- Keep this root file limited to repository-wide invariants and precise routing gates; place detailed workflows in versioned documentation or skills.
- Codex applies a combined instruction budget (32 KiB by default) across global, root, and nested guidance. Keep mandatory gates first and preserve headroom for narrower scopes.
- Do not raise `project_doc_max_bytes` as the first response to oversized guidance; remove duplication and route conditional detail first.

## Tibia app: CIP client as upstream (Mandatory)

Name the company **CipSoft** (or **CIP**). Never abbreviate it as **Soft** —
that collides with soft lighting, soft-death/despawn, Soft Boots, etc.

`client/tibia/` is the C++/QML rewrite of the official (CIP) Tibia client and the
**product desktop client**: [`docs/client-rewrite-plan.md`](../../docs/client-rewrite-plan.md).
Desktop release is `tibia-desktop`. Browser `/play` is the tibia wasm build
(`make tibia-wasm`); the GHCR image name stays `otclient-wasm` (artifact =
`tibia.js` / `tibia.wasm`). Lua/OTUI product UI and Dawn are retired. Do not
add product gameplay to Lua. The engine is `client/tibia/` (protocol, world,
assets, sockets, sound). Qt owns the process loop. There is no `gamecore`
target and no opentibiabr/otclient upstream. Check behavior against CIP.

- **`client/upstream/` is vendored, never hand-edited.** QML, moc contract, protos, msgmap,
  strings, shaders and `clientoptions.json` come from `make tibia-sync TIBIA=…`
  (umbrella `tools/tibia-sync`), which also writes `upstream/DRIFT.md`. `--check`
  (in `make ci`) fails on any hand edit. CIP art (`graphics_resources.rcc`) is
  never committed: sync copies it into `data/assets/` (or legacy `data/things/<ver>/`) and the app mounts
  it at runtime. Skill: `.cursor/skills/tibia-sync`.
- **C++ API is generated** by `tools/cip/gen_cpp.py` (via `cmake/CipCodegen.cmake`)
  as `cip::<CIP namespace>::<CIP class>` bases plus `cip::registerQmlTypes()`.
  Handwritten classes in `client/tibia/` subclass them under CIP's own names
  (`tibia::gamewindow::TCharacterSelectionController`). Do not rename CIP API.
- **Product QML** lives in the IdleOT pack (`apps/idleot-client/client/qml`, module
  `idleot`; monorepo symlink `client/mods/idleot`), reusing CIP components.
  Put new product UI there. Prefer C++ controllers / models that satisfy stock CIP
  QML (`model.length`, roles, signals) over forking a CIP `.qml` file.
- **Keep CIP QML intact.** Never hand-edit `client/upstream/`. Do not add
  `client/overrides/…` for product features — those belong in the IdleOT pack
  `qml/` / `overrides/`. Engine/Qt-only forks stay in `client/overrides/`
  (e.g. LegacyControls). Product stock-path forks go in the pack's `overrides/`
  (rel. to `client/upstream/qml/`) with a first line
  `// cip-base: <sha256 of the cip file>`. Engine overrides still merge at
  configure-time into `qmlcomponents` qrc; **pack** overrides remap at runtime
  via `PackQmlInterceptor` so Soft hosts stay stock (F5 / live reload). A CIP
  release that changes the base fails `tibia-sync --check` until the override is
  re-merged. Drop an override once C++ or the pack can carry the behavior instead.
  **Overrides are global CipSoft forks** (classic + HUD both mount them — not
  HudChat-only). `cip-base` does not prove CipSoft gestures/shortcuts still work;
  before merge, run the CipSoft behavioral checklist in
  [`docs/reverse-engineering/tibia-ui-parity.md`](../../docs/reverse-engineering/tibia-ui-parity.md#cipsoft-behavioral-obligations-on-qmloverrides)
  (visible select/drag, CipSoft Copy paths without focused TextEdit, hit-testing).
- **Engine:** protocol, world, asset, net, and sound code lives under
  `client/tibia/` and is checked against CIP (`upstream/protocol/messages.json`, a
  decompile, or a session dump). Legacy `src/framework` paths are gone.
  There is no `gamecore` library. Message buffers, image decode, RSA,
  and `Position` live under `client/tibia/engine`. HD scaling uses the public
  `sprites/hdscale` hook; The product pack registers xBRZ under `qtclient/scale/` when linked. Qt drives frames from
  `renderer/nextframeupdater.cpp`. Scheduled work uses Qt queued calls and
  `QTimer`. Resource paths use `QFile` and `--data`. See
  [`docs/open-source.md`](docs/open-source.md).
- **Toolchain:** CMake + Ninja + presets; C++ deps via **vcpkg** (`vcpkg.json`,
  including wasm as an OS triplet). Qt is **CipSoft-Tibia/Qt5** at
  `third_party/qt5-cip` (patched 6.10.3; `update = none` — init via `make qt`),
  plus `third_party/Qt5LegacyControls`. `make qt [WASM=1]` builds or pulls
  GHCR `ghcr.io/qtclient/qt-cip:<sha>-<kit>` (never vcpkg Qt; emsdk 4.0.7 for
  wasm). Needs `VCPKG_ROOT`. Docs:
  [`docs/development/build-tooling.md`](docs/development/build-tooling.md).
  Umbrella `make qtclient client build` is **dev** (`TIBIA_QML_DISK`:
  `qmlcomponents` stays on qrc; live reload for pack `qml/<uri>/` + override
  roots; F5 clears cache). Production: `make -C apps/qtclient client build`.
  Run: `make qtclient client run ARGS=…`, `make qtclient wasm build`. Flags:
  `--preview`, `--check-qml`, `--screenshot FILE`, `--renderer …`, `--data DIR`.
- **Login smoke test** (portal + Crystal running): `make qtclient client run
  ARGS='--account=EMAIL --password=PW --character=NAME --play-for=20'` logs in,
  plays 20 s, logs out; exit 0 only if still online. Without `--character` it
  shows CIP's character list. Auth is CipSoft launcher + CIP login /
  `/api/login` only (no product play-bootstrap SSO). With `--character`,
  `--screenshot=tmp/shots/x.png` shoots the world 3 s after login;
  `QT_LOGGING_RULES=tibia.worldmap.debug=true` logs sprite counts and sync cost.
- **CIP full clone.** Gap ledgers and status:
  [`docs/reverse-engineering/ledgers/`](../../docs/reverse-engineering/ledgers/),
  [`docs/reverse-engineering/cip-clone-status.md`](../../docs/reverse-engineering/cip-clone-status.md).
  Default world path is CIP every-frame emit. Transitional field-slide is opt-in
  (`TIBIA_FIELD_SLIDE=1` / `TIBIA_FIELD_SLIDE=1`); `TIBIA_CIP_WORLD=1` forces full emit for oracle.
- **World rendering** is CIP's. `renderer/TWorldMapRenderProvider` builds one
  frame per tick on the GUI thread: sprite records, the 1-pixel-per-field light
  image, and creature HUD anchors.
  - `WorldMap`, `LightMap` and `worldmap/TCreatureHUDOverlayController`
    (`CreatureHUD.qml` per creature) consume that frame. Highlighted sprites
    (mouse, attack and follow target) use CIP's `worldmap-highlight` shader.
  - Input is CIP's: `worldmap/TWorldMapPicker` →
    `TWorldMapGameActionProvider` (candidate `input::TGameAction`s) →
    `input::TGameActionMapper` (control scheme rules) → `input::publish`.
    `renderer/TWorldView` runs it for hover (cursor + highlight) and
    `mapWindowClicked`. Decompile CIP's code before changing these; do not
    infer.
  - OTClient's `MapView`/`Tile::draw`/draw pools/`LightView` are not used by the
    app.
  - Draw-order and light changes go in the provider; engine classes only get
    accessors.
  - **Raycast lighting** (product pack): height stamps + polar occlusion on top
    of CIP’s world draw. Agents/devs:
    [`docs/raycast-lighting.md`](../../docs/raycast-lighting.md). Design story:
    [`docs/2d-height-shadows.md`](../../docs/2d-height-shadows.md). Classic CIP
    light map remains the default; do not invent normal-map lighting for CIP art.
- **No Lua in the tibia app.** Logic that lived in Lua modules (features,
  things loading, product effects, opcode handlers) has a C++ home under
  `client/tibia/` (`qtclient/`, `network/ExtendedOpcodeRouter`, and the engine
  replacement). Encrypted Lua/OTUI shipping does not apply to this client
  (QML is compiled in). Game model code must not assume graphics: the `tibia`
  host creates no draw pools or GL context.
- **Qt 6.10.3 bug:** a `default property alias X: root.data` resolves to the next
  alias declared after it. CIP's build is patched; ours is not, hence the
  `BasicTableView` override. Keep default aliases last in any QML we own.

## Fork / upstream (Mandatory routing)

- **CIP is the client upstream.** `make tibia-sync` vendors `client/upstream/`. Protocol,
  map, asset, net, and sound behavior is written under `client/tibia/` and checked
  against that tree. There is no opentibiabr/otclient remote.
  See [`docs/engine-forks.md`](../../docs/engine-forks.md#otclient-engine-is-cip-owned).
- **Product UI:** IdleOT pack under `apps/idleot-client/` (runtime MODULE via ModLoader).
- CrystalOTC / NextGen / koliseu donor remotes and pins are retired — implement
  features directly in `client/tibia/` and QML.

## CIP assets

Runtime resolution matches CipSoft: one Resources tree
(`applicationDirPath()/../Resources`, AppData tip
`packages/<TIBIA_CLIENT_PACKAGE_DIR>/Contents/Resources`, or monorepo
`data/`), then short names `assets/` and sibling `sounds/`. Install localfiles
are OS-shaped (`Contents/Resources/…` on macOS). Dev extract (`make extract`)
copies CIP assets + sounds + `graphics_resources.rcc` into `data/assets/`;
spell strips, `minimap.otmm`, and portal outfit/item PNGs are opt-in
(`EXTRACT_FLAGS`). There is no `/data/assets` or `data/things/<version>/`
runtime fallback and no Lua asset downloader.

`ClientVersion` (e.g. 1533) is the **protocol** constant — not an assets folder
name. `make tibia-sync` keeps it aligned with `upstream/VERSION.json` `cip_version`.

## Launcher pack / Play (Mandatory)

CIP tip protocol: `tibiametadata.json` → engine `package.json` → `assets.json`
(stamp `assets.json.sha256`). See
[`docs/reverse-engineering/tibia-launcher-packaging.md`](../../docs/reverse-engineering/tibia-launcher-packaging.md).

- **Desktop:** `installPackage` then `installAssets` before Play.
- **Browser (WASM):** tip layout is CipSoft
  `packages/<dir>/Contents/Resources/{assets,sounds,minimap}` under **OPFS**
  (same paths as desktop AppData disk) on **all** platforms. `/play` is always
  portal progress + one game-only Module (never Soft Widgets). Portal installs
  essentials + sprite `/packs` via normal FS writes before `callMain`. Warm
  boot verifies the OPFS stamp; on miss installs in place. No IDBFS tip tree,
  Cache-as-packs VFS, or bulk MEMFS unzip. Do not download sprites after Play
  is live.
- Do not ship a `/play` path that downloads sprites after Play.
- **Variants** (CipSoft tip enum): `regular` / `test` / `beta` / `preview`.
  Stock tip directoryname is `Tibia.app`; the qtclient pack overrides to
  `IdleOT.app` (see `launcher/mods/idleot/brand.cmake` → `apps/idleot-client/launcher`). Assemble with
  `TIBIA_TIP_VARIANT` / `TIBIA_TIP_VARIANT`; launcher selects with
  `TIBIA_VARIANT` / `TIBIA_VARIANT` (default `regular`). **Local** skips the
  launcher: `make qtclient client run` links `Contents/Resources` → `data/`
  (`assets/` + `sounds/` siblings).
