# qtclient (otqt)

CipSoft-compatible **Qt/QML Tibia client** (desktop + browser WASM) + **CIP launcher**.

## Layout (concern-higher)

| Concern | Client | Launcher |
|---------|--------|----------|
| C++ | `client/tibia/` | `launcher/src/` |
| Product UI | `client/qml/` | `launcher/qml/` |
| Stock forks | pack `overrides/` only | `launcher/overrides/` |
| Packs | `client/mods/` | `launcher/mods/` |
| CIP vendor | `client/upstream/` | `launcher/upstream/` |

BYO licensed Tibia install for offline assets — CIP blobs are never redistributed.
Preferred path: **demo tip** (our binary + CipSoft `assetsurl` + `mods/default`).
No pack / `--no-mods` → real CipSoft. Login URLs otherwise from the single tip pack.

## Use a release binary

1. Run [qtc-aac](https://github.com/qtclient/qtc-aac) + your game server (Crystal, etc.).
2. Download the qtclient demo Release for your OS.
3. Start `./launcher` (sidecar `tibiametadata.json` or set `QTCLIENT_URL`). **Update** pulls CipSoft assets; **Play** uses qtc-aac login.

```bash
# optional explicit tip
export QTCLIENT_URL="https://github.com/qtclient/qtclient/releases/latest/download/tibiametadata.json"
./launcher
```

4. **Offline BYO:** install official Tibia, then:

```bash
./qtclient-fetch-assets
# or: ./qtclient-fetch-assets --tibia /path/to/Tibia.app
./client
```

See [demo-tip.md](demo-tip.md). Local monorepo play is launcher-first:
[play-tip.md](play-tip.md) (`make desktop run` — CipSoft `packages[]`, worktrees + packs).

## Build from source

```bash
git clone https://github.com/qtclient/qtclient.git
cd qtclient
git submodule update --init third_party/Qt5LegacyControls
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make setup
make desktop build && make launcher build && make desktop run
# launcher: make launcher build && make launcher run
```

`make setup` runs `make qt`, then `make sync` and `make extract` against the
default CipSoft package path. Override with `TIBIA=` / `MSGMAP=` when needed.

## Stock vs mods

| Build | CMake |
|-------|--------|
| Stock CIP only | `-DQTCLIENT_ENABLE_MODS=OFF` |
| Auto-enable packs in `client/mods/` (skips `_`-prefixed templates) | default `QTCLIENT_ENABLE_MODS=ON` |

```bash
ln -s ~/my-mod client/mods/my_mod
# or drop a built mods/<id>/ tree next to the client binary (ModLoader)
```

## CI artifacts

- **CIP Qt prefixes:** `ghcr.io/qtclient/qt5:<sha12>-<kit>`
- **ccache** from stock (`QTCLIENT_ENABLE_MODS=OFF`) builds
- **launcher + client + qtclient-fetch-assets** tarballs (no CIP asset blobs)

## Docs

| Doc | Topic |
|-----|--------|
| [demo-tip.md](demo-tip.md) | CIP assetsurl + our package |
| [../open-source.md](../open-source.md) | Public vs product packs |
| [../mods.md](../mods.md) | Overrides + mods |
| [AGENTS.md](AGENTS.md) | CIP-as-upstream rules |

C++ deps: **vcpkg**. Qt: [qtclient/Qt5](https://github.com/qtclient/Qt5) (fork of CipSoft-Tibia/Qt5) kits via `make qt`.
LegacyControls QML: `make sync` from the install plugin. C++:
[Qt5LegacyControls](https://github.com/CipSoft-Tibia/Qt5LegacyControls).
