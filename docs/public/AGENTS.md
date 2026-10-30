# AGENTS — qtclient

Public CipSoft-compatible client + launcher. Product packs live under
`client/mods/` or `launcher/mods/` in *your* checkout and are not part of this
repository’s default tree.

## Layout

Product-higher: sibling `client/` and `launcher/` trees (plus `shared/` for
`shared::`), each with the same work-tree axes:

`src/` · `qml/` · `overrides/` · `mods/` · `upstream/`

## CIP as upstream

- `client/upstream/` — vendored from official `Tibia.app` (`make sync`). Never hand-edit; not committed in the public tree.
- `launcher/upstream/` — CIP launcher chrome (`make` launcher sync). Never hand-edit.
- `client/overrides/` — engine/Qt-only stock forks (e.g. LegacyControls).
- `client/mods/<pack>/overrides/` — product stock-path forks.
- `launcher/overrides/` — stock chrome forks.
- `client/tibia/<area>/` — `tibia::<area>` C++/QML controllers and engine.
- `launcher/src/` — CIP tip protocol launcher.
- `shared/src/` — `shared::` types shared by both binaries.

## Mods

- `QTCLIENT_ENABLE_MODS` (default ON). Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF`.
- Client packs: `client/mods/<name>/` with `mod.json` + optional `src|qml|overrides`.
- Launcher packs: `launcher/mods/<name>/` (art overlays, etc.).

## Toolchain

- CMake + Ninja + vcpkg (`VCPKG_ROOT`).
- Qt: `third_party/qt5-cip` (`update = none`) — `make qt` / `make qt WASM=1`. Warm prefixes: `ghcr.io/qtclient/qt`.
- LegacyControls: `third_party/Qt5LegacyControls`.

## Do not

- Commit `upstream/`, CIP assets, or private mod packs.
- Hand-edit generated CIP contract/QML under `upstream/`.
- Host CipSoft game blobs on GitHub Releases (use demo tip or BYO extract).
