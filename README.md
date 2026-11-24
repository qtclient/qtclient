<p align="center">
  <strong>qtclient</strong> · <code>otqt</code><br/>
  CipSoft-compatible <strong>Qt/QML</strong> Tibia client — desktop &amp; browser WASM
</p>

<p align="center">
  <a href="https://github.com/qtclient/qtclient/actions"><img alt="CI" src="https://github.com/qtclient/qtclient/actions/workflows/ci.yml/badge.svg" /></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue.svg" /></a>
</p>

**qtclient** is a **CipSoft client port**: CipSoft's client and launcher classes
under CipSoft's names, written from CipSoft's binaries, on the official CIP QML UI
(vendored, never hand-edited) and the CIP tip launcher protocol. Not affiliated
with CipSoft GmbH.

**v1:** 100% parity with CipSoft's client 15.33.0aa90c and launcher (every
CipSoft function of the pinned builds ported or shown not to be CipSoft logic;
`python3 tools/re/port.py coverage` in the umbrella), with the OTClient-era code
removed. See [CHANGELOG.md](CHANGELOG.md).

Public repository: **[qtclient/qtclient](https://github.com/qtclient/qtclient)**.

## Why qtclient (vs OTClient)

qtclient is **not** an OTClient fork. Use it when you want CipSoft parity; use
OTClient when you want a Lua/OTUI community stack.

| | **qtclient** | **OTClient** |
|--|--|--|
| UI | CipSoft **QML** (official chrome) | Lua / OTUI |
| Upstream | Official CIP client (`make tibia-sync`) | Community protocol tree |
| Launcher | CIP tip protocol | Custom / third-party updaters |
| Browser | First-class **WASM** | Limited or separate ports |
| Extensibility | Runtime **mods/packs** on a stock CIP host | Usually fork the tree |

### Highlights

- **CIP QML host** — `client/upstream/` from sync; controllers under `client/tibia/`
- **CIP launcher** — tip metadata Update / Play
- **Desktop + WASM**
- **Stock vs packs** — optional `mods/<id>/` at runtime ([docs/mods.md](docs/mods.md))
- **Pack SDK** — host-pushed C ABI ([`include/qtclient/`](include/qtclient/README.md)), filesystem QML, F5 hot reload in dev
- **Session tools** — `--record` / `--replay`
- **Toolchain** — CMake + Ninja + vcpkg; CipSoft-patched Qt via `make qt`

## Layout

| Concern | Client | Launcher |
|---------|--------|----------|
| C++ | `client/tibia/` | `launcher/src/` |
| CIP vendor | `client/upstream/` | `launcher/upstream/` |
| Packs | `client/mods/` | `launcher/mods/` |

| Layer | Stock (this tree) | Product pack |
|-------|-------------------|--------------|
| CipSoft client / launcher | `client/tibia`, `launcher/src` (+ `upstream/` from sync) | — |
| `upstream/*` / CIP assets | `make sync` + `make extract` from a licensed install | Same |
| Pack SDK + default pack (`qtc-aac`) | Yes | `mod.json` + native MODULE / FS QML |
| Product packs | Not required to build stock | Drop in `mods/<id>/` or umbrella `make qtclient desktop install` (`QTCLIENT_PACK`) |

No pack / `--no-mods` → real CipSoft. Login URLs otherwise come from the tip pack.

## Use a release binary

1. Run [qtc-aac](https://github.com/qtclient/qtc-aac) + your game server (Crystal, etc.).
2. Download the qtclient demo Release for your OS.
3. Start `./launcher` (sidecar `tibiametadata.json` or set `QTCLIENT_URL`). **Update**
   pulls CipSoft assets; **Play** uses qtc-aac login.

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

See [docs/public/demo-tip.md](docs/public/demo-tip.md). Local monorepo play is
launcher-first: [docs/public/play-tip.md](docs/public/play-tip.md).

## Build from source

```bash
git clone https://github.com/qtclient/qtclient.git
cd qtclient
git submodule update --init third_party/Qt5LegacyControls
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make setup
make desktop build && make launcher build && make desktop run
```

`make setup` runs `make qt`, then `make sync` and `make extract` against the
default CipSoft package path (override with `TIBIA=` / `MSGMAP=`). BYO licensed
Tibia install — CipSoft blobs are never redistributed. More:
[docs/building.md](docs/building.md).

| Build | CMake |
|-------|--------|
| Stock CIP only | `-DQTCLIENT_ENABLE_MODS=OFF` (or run with `--no-mods`) |
| Auto-enable packs in `client/mods/` (skips `_`-prefixed templates) | default `QTCLIENT_ENABLE_MODS=ON` |

```bash
ln -s ~/my-mod client/mods/my_mod
# or drop a built mods/<id>/ tree next to the client binary (ModLoader)
```

### Env prefixes

| Prefix | Use |
|--------|-----|
| `QTCLIENT_*` | Stock qtclient / launcher process env and WASM hooks |
| pack-defined prefix | Product pack-only knobs (lighting, raycast / weather perf) |
| `TIBIA_*` | CipSoft-shaped CMake / target names (brand.cmake keys) — not process env |

## CI artifacts

- **CIP Qt prefixes:** `ghcr.io/qtclient/qt5:<sha12>-<kit>`
- **ccache** from stock (`QTCLIENT_ENABLE_MODS=OFF`) builds
- **launcher + client + qtclient-fetch-assets** tarballs (no CIP asset blobs)

C++ deps: **vcpkg**. Qt: [qtclient/Qt5](https://github.com/qtclient/Qt5) (fork of
CipSoft-Tibia/Qt5) kits via `make qt`. LegacyControls QML: `make sync` from the
install plugin; C++: [Qt5LegacyControls](https://github.com/CipSoft-Tibia/Qt5LegacyControls).

## Docs

| Doc | Topic |
|-----|--------|
| [docs/building.md](docs/building.md) | Building (desktop, WASM, Android) |
| [docs/mods.md](docs/mods.md) | Packs |
| [docs/pack-extension-contract.md](docs/pack-extension-contract.md) | Frozen tip hooks for packs |
| [docs/public/CONTRIBUTING.md](docs/public/CONTRIBUTING.md) | Contributing |
| [SECURITY.md](SECURITY.md) | Security |
| [AGENTS.md](AGENTS.md) | CIP-as-upstream rules |
