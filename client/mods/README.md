# Mods

Bare Dist (CipSoft Host) is **not** under `mods/` (`upstream/` + `client/tibia/`).
A Client Mod is `mods/<name>/` + `mod.json` + `qml/<uri>/`, and customizes the Host
only through its `overrides/` (the Host has none).

## Default Mods (in-tree)

| Mod | Role |
|-----|------|
| `portal/` | [portal](https://github.com/qtclient/portal) login urls (`/clientservices/*.php`); staged by `make demo-pack` |

Product Mods live out-of-tree (`QTCLIENT_PACK` in the umbrella `.env`) and build
apart from the Host, against the Mod SDK (`find_package(QtClientSdk)`).
`make qtclient desktop install` installs the SDK, builds + installs the Mod, then
builds the Dist with `-DQTCLIENT_PACKS=<mod install prefix>`, which stages Dist
`mods/<id>` (desktop: incl. `native/mod.*`); product urls replace `portal`.
DEV builds load Mod `qml/` + `overrides/` from the Mod source tree
(`QTCLIENT_DEV_PACK_SOURCES`; F5 / file watch) — see [`include/qtclient/README.md`](../../include/qtclient/README.md).

## Inplace (gitignored)

Other experimental Mods under this directory are gitignored. An in-tree Mod is
FS-only (`mod.json`, `qml/`, `overrides/`); one with C++ builds against the SDK like any
out-of-tree Mod (a `CMakeLists.txt` here fails the Host configure). Prefer
`QTCLIENT_PACK` + `make qtclient desktop install` for product Dists.

Enable with default `QTCLIENT_ENABLE_MODS=ON` (auto-discovers non-`_*` Mods).
Bare Dist only: `-DQTCLIENT_ENABLE_MODS=OFF` or run with `--no-mods`.

QML must be a filesystem module: `qml/<qmlModule>/qmldir`.
Mod surface: the Host-pushed C ABI [`include/qtclient/abi/host_api_v1.h`](../../include/qtclient/abi/host_api_v1.h)
(SDK: [`include/qtclient/`](../../include/qtclient/README.md)).
