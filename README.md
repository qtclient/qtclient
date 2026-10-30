# tibia / qtclient

Qt/QML **CipSoft-compatible** open Tibia client (desktop and browser WASM).

Public repository: **[qtclient/qtclient](https://github.com/qtclient/qtclient)** (`otqt`).

- **Stock** = CIP (`client/upstream/` via `make tibia-sync`, never hand-edit) + CIP class FS under `client/tibia/`.
- **Overrides** (`client/overrides/`) customize stock CIP QML.
- **Mods** (`client/mods/`) are optional packs with a unified `mods/<id>/` layout loaded at runtime (`ModLoader`). Public tree ships `_sample`; drop private packs inplace.

BYO licensed `Tibia.app` for sync/extract — CIP blobs are not redistributed.

```bash
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make qt
make sync TIBIA=/path/to/Tibia.app MSGMAP=…
make extract TIBIA=/path/to/Tibia.app
make client build && make client run
# Dev disk QML (F5): make client build DEV=1  (umbrella `make qtclient client build` sets this)
```

## Stock vs mods

| Build | CMake |
|-------|--------|
| Stock CIP only | `-DQTCLIENT_ENABLE_MODS=OFF` |
| Auto-enable packs in `client/mods/` (except `_sample`) | default `QTCLIENT_ENABLE_MODS=ON` |
| Explicit list | `-DQTCLIENT_MODS=my_pack;other` |
| Sample SDK pack | `-DQTCLIENT_MODS=_sample` |

### Add a private mod inplace (not committed)

`client/mods/*` is gitignored except `_sample` and `README.md`. Any of:

```bash
cp -R ~/my-mod client/mods/my_mod
ln -s ~/src/my-mod client/mods/my_mod
```

Build with `qtclient_add_mod` and drop the staged `mods/<id>/` tree next to `client` (or let ModLoader find `QTCLIENT_MODS_DIR`). See [`client/mods/_sample`](client/mods/_sample) and [`docs/mods.md`](docs/mods.md).

## Docs

| Doc | Topic |
|-----|--------|
| [docs/open-source.md](docs/open-source.md) | Public vs private, export, CI reuse |
| [docs/mods.md](docs/mods.md) | Pack format, ModLoader, WorldView |
| [docs/public/CONTRIBUTING.md](docs/public/CONTRIBUTING.md) | Public contributor + ccache/Qt reuse |
| [SECURITY.md](SECURITY.md) | Vulnerability reporting |
| [AGENTS.md](AGENTS.md) | CIP-as-upstream rules |

C++ deps: **vcpkg**. Qt: CipSoft-Tibia/Qt5 (`third_party/qt5-cip`) via `make qt` — warm prefixes `ghcr.io/qtclient/qt-cip`. LegacyControls: `third_party/Qt5LegacyControls`.
