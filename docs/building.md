# Building tibia / qtclient

## First-time (path-free)

Install the official CipSoft Tibia client, then from this tree (or the public
[qtclient](https://github.com/qtclient/qtclient) clone):

```bash
git submodule update --init third_party/Qt5LegacyControls
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make setup                 # qt + sync + extract (default Tibia path)
make client-build && make client-start
```

`make sync` / `make extract` probe OS defaults (macOS CipSoft package,
Windows `%LOCALAPPDATA%\Tibia\…`, Linux XDG). Override with `TIBIA=…` when
needed. New CIP `client_sha256` still requires `MSGMAP=…` for protocol regen.

Binary users: download a release tarball, run `./launcher` with a demo tip or `./qtclient-fetch-assets`, then
`./client` — see the root README.

## Toolchain

**Prerequisites:** `VCPKG_ROOT`, CMake, Ninja, and Qt from `make qt` (add
`WASM=1` for the browser OS). That builds CipSoft’s public
[Qt5](https://github.com/CipSoft-Tibia/Qt5) tree (`third_party/qt5-cip`) or
pulls a warm prefix from `ghcr.io/qtclient/qt`
(needs [oras](https://oras.land) for pulls). First local build without an
artifact compiles Qt (large/slow).

C++ libraries come from the vcpkg manifest (`vcpkg.json`, Crystal-aligned
baseline). Qt is never a vcpkg port. LegacyControls is the
[Qt5LegacyControls](https://github.com/CipSoft-Tibia/Qt5LegacyControls)
submodule (LGPL). Details:
[`docs/development/build-tooling.md`](development/build-tooling.md).

Presets live in `CMakePresets.json` (`macos-dev`, `linux-release`,
`windows-release`, `wasm-tibia`, …). All of them use the vcpkg toolchain;
wasm chainloads Qt’s `qt.toolchain.cmake` with the `wasm32-emscripten` triplet.

Umbrella repo: same targets via top-level `make extract` / `make sync`
(optional `TIBIA=`).
