# Building tibia / qtclient

## First-time (path-free)

Install the official CipSoft Tibia client, then from this tree (or the public
[qtclient](https://github.com/qtclient/qtclient) clone):

```bash
git submodule update --init third_party/Qt5LegacyControls
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
make setup                 # qt + sync + extract (default Tibia path)
make desktop build && make desktop run
```

`make sync` / `make extract` probe OS defaults (macOS CipSoft package,
Windows `%LOCALAPPDATA%\Tibia\…`, Linux XDG). Override with `TIBIA=…` when
needed. New CIP `client_sha256` still requires `MSGMAP=…` for protocol regen.

Binary users: download a release tarball, run `./launcher` with a demo tip or `./qtclient-fetch-assets`, then
`./client` — see the root README.

## Toolchain

**Prerequisites:** `VCPKG_ROOT`, CMake, Ninja, and Qt from `make qt` (add
`WASM=1` for the browser OS). That pulls the prebuilt kits of
[qtclient/Qt5](https://github.com/qtclient/Qt5) (our fork of CipSoft’s
[Qt5](https://github.com/CipSoft-Tibia/Qt5), `third_party/qt5-cip`) from
`ghcr.io/qtclient/qt5` (needs [oras](https://oras.land)) or its releases. Qt is
never compiled here; qtclient/Qt5 builds a kit set for each of its commits.

C++ libraries come from the vcpkg manifest (`vcpkg.json`, Crystal-aligned
baseline). Qt is never a vcpkg port. LegacyControls QML is carved by
`make tibia-sync`; C++ is the LGPL
[Qt5LegacyControls](https://github.com/CipSoft-Tibia/Qt5LegacyControls)
submodule on branch `main` (not hash-pinned). Details:
[`docs/development/build-tooling.md`](development/build-tooling.md).

Presets live in `CMakePresets.json` (`macos-dev`, `linux-release`,
`windows-release`, `wasm-qtclient`, …). All of them use the vcpkg toolchain;
wasm chainloads Qt’s `qt.toolchain.cmake` with the `wasm32-emscripten` triplet.

Umbrella repo: same targets via top-level `make extract` / `make sync`
(optional `TIBIA=`).

## Android / iOS (experimental)

Not shipping platforms. Desktop and WASM are the supported targets. The
`android/` tree and `scripts/lifecycle/build/{android,ios}.sh` are unsupported
scaffolds (`make qt ANDROID=1` for the Android Qt kit if you experiment).

## Layout notes

| Path | Role |
|------|------|
| `shared/` | Shared C++ lib linked by **launcher + tibia** |
| `client/tibia/shared/` | CIP client-only helpers (not the top-level lib) |
| `include/qtclient/abi/` | Pack C ABI (extensibility surface) |
| `data/qml/` | Not committed — comes from `make qt` |

## Animated UI assets

Qt's PNG handler reads **static** PNG only (no `acTL` / APNG). Animated CIP UI
art ships as **animated WebP** and loads through `QImageReader` (`imageCount`,
`nextImageDelay`, `loopCount`). When upstream ships new APNG PNGs under `data/`:

```bash
python3 tools/extract/apng_to_webp.py
python3 tools/extract/apng_to_webp.py --check   # CI: fail if any acTL .png remains
```

CipSoft's QML asks for the `.webp` path; data lookups without an extension try
`.webp` (`guessFilePath`). WebP is preferred over GIF (lossless, smaller).
