# Changelog

All notable changes to the public qtclient tree are documented here.
Format inspired by [Keep a Changelog](https://keepachangelog.com/).
Versioning follows semver for public tags (`vX.Y.Z`).

## [Unreleased]

## [1.0.0] — 2026-11-24

qtclient v1: a complete port of CipSoft's client and launcher.

### Added

- **CipSoft 15.33.0aa90c parity.** Every CipSoft function of the pinned client
  (15.33.0aa90c) and launcher builds is ported from its decompiled unit or shown
  not to be CipSoft logic: 100% in `port.py coverage` for both (todo 0,
  untriaged 0), tracked function by function in `client/tibia/ports.lock` and
  `launcher/ports.lock`.
- **CipSoft-like secondary game connection.** `TGameserverDualConnection` opens a
  backup connection to the world's unprotected port from the login data
  (`externalportunprotected`; browser `websocketportunprotected`) when
  *connection stability optimisation* is on, sends each XTEA packet on both, and
  dedupes the server's copies. CrystalServer listens on the extra game port and
  accepts growing sequence numbers with gaps, as CipSoft's server does.
- CipSoft's session recorder for `--record` (writes a CipSoft `dmp` file that
  `--replay` plays through the session-dump player).
- Pack SDK: host-pushed C ABI (`include/qtclient/abi/`): runtime MODULE packs
  export only `qtclient_mod_register(api, ctx)` and import no tip symbols, the
  same way on desktop and WASM; filesystem QML modules (`qml/<uri>/qmldir`) +
  `PackQmlInterceptor`; WorldView hooks (lighting driver, overlays, frame
  hooks). See `docs/mods.md` and `docs/pack-extension-contract.md`.
- SECURITY.md and clarified NOTICE (non-affiliation, BYO CipSoft materials).

### Removed

- The OTClient-era code CipSoft parity does not need: the session facade
  (`TGameSessionState` and the `gamesession*` forwarders; the session is
  CipSoft's `TGameSessionBase`), `cip_ports/` and the `CipTyped` shells, the
  OTC `client/` remnants and `ThingType` (appearances are CipSoft's
  `TAppearanceType`), the legacy game-server connection
  (`gameserverconnection`), `runtimegameconfig`, the OTC map state, wire and
  asset enums, and every client-side workaround for server differences
  (CrystalServer now behaves like CipSoft's server instead).
- The `otclient` example pack and the sample SDK pack `client/mods/_sample`;
  the default tip pack is `mods/portal`.
- Tip export lists / `host_exports.json` and packs importing
  `qtclient_mod_host_api_v1()` (replaced by the `qtclient_mod_register` argument).

### Changed

- Default tip pack `mods/myaac` → `mods/portal`: login and account pages point
  at [qtc-aac](https://github.com/qtclient/portal)
  (`http://127.0.0.1:5173/clientservices/*.php`). MyAAC servers can still use
  `package_desktop.py --login-profile myaac`.
- Stock core is CIP classic lighting only; product raycast / weather live in
  packs via the WorldView API.
- Product identity and launcher brand resolve from `mod.json` at runtime.
- Process env / WASM hooks: `QTCLIENT_*` in stock, pack-defined knobs in product
  packs; `TIBIA_*` kept for CipSoft-shaped CMake / target names only.
- ModLoader: API min/max check, optional `hostBuildId`, import path is pack `qml/` only.

## [0.1.0] — 2026-03-28

### Added

- CipSoft-compatible Qt/QML client spun out as qtclient (otqt): CIP QML +
  `client/tibia/`, mods discovery, BYO assets.
- Session replay: `--record` / `--replay`; fixtures under `testdata/sessiondumps/`.
- First-run: `make setup` and release helper `qtclient-fetch-assets`.
