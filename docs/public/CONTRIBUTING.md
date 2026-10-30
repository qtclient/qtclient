# Contributing to qtclient

## Layout

| Path | Role |
|------|------|
| `client/upstream/` | CIP sync output (local only; gitignored in public) |
| `client/overrides/` | Stock QML overrides |
| `client/mods/_sample/` | Example mod pack (unified `mods/<id>/` layout) |
| `client/mods/<your>/` | Your pack (gitignored; copy/symlink/submodule locally) |
| `launcher/mods/<your>/` | Launcher brand / art pack |
| `third_party/qt5-cip` | CipSoft Qt sources (submodule, `update = none`) |
| `third_party/Qt5LegacyControls` | Controls 1 port (submodule) |

## Overrides vs mods

- **Overrides** change stock CIP UX → `client/overrides/`. Prefer small, upstreamable diffs.
- **Mods** add product features → `client/mods/<name>/` with `mod.json` + `qtclient_add_mod`.
- Customize world rendering via **WorldView** hooks (`docs/mods.md`) — do not fork CIP lighting into stock.

## First build

```bash
git submodule update --init third_party/Qt5LegacyControls
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make setup    # qt + sync + extract (default Tibia path; TIBIA= / MSGMAP= overrides)
make client build && make client run
```

Binary smoke: download a stock CI client tarball, `./qtclient-fetch-assets`, `./client`.

## Building with public CI caches

qtclient Actions publish shared warm state for downstream (e.g. private product) builds:

1. **Qt:** `tools/qt/ci-setup-qt.sh` pulls `ghcr.io/qtclient/qt:<sha>-<kit>` (override with `QT_GHCR_IMAGE` if needed).
2. **ccache:** download the `ccache-<os>-<sha>` artifact from the latest successful `Client: ci` workflow on this repo for the base commit you branched from; unpack into `CCACHE_DIR` before `cmake --build`.

Helper for consumers (from a checkout that vendors this tree):

```bash
# After checking out the qtclient base SHA:
bash scripts/ci/fetch-qtclient-ccache.sh --sha <qtclient_sha> --os linux
export CCACHE_DIR="$PWD/.ccache"
cmake --build …
```

## PRs

- Keep PRs free of `client/upstream/` blobs.
- Product features belong in `client/mods/<id>/` (or out of tree), not stock `client/tibia`.
- Run stock configure: `cmake --preset … -DQTCLIENT_ENABLE_MODS=OFF`.
- Do not bump `third_party/qt5-cip` without a matching Qt rebuild intent (GHCR publisher workflow).
