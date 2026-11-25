# Mods

Stock CipSoft is **not** under `mods/` (`upstream/` + `client/tibia/`).
A pack is `mods/<name>/` + `mod.json` + `qml/<uri>/`, and customizes stock only
through its `overrides/` (the host has none).

## Default packs (in-tree)

| Pack | Role |
|------|------|
| `qtc-aac/` | [qtc-aac](https://github.com/qtclient/qtc-aac) login urls (`/clientservices/*.php`); staged by `make demo-pack` |

Product packs live out-of-tree (`QTCLIENT_PACK` in the umbrella `.env`) and build
apart from the host, against the pack SDK (`find_package(QtClientSdk)`).
`make qtclient desktop install` installs the SDK, builds + installs the pack, then
builds the tip with `-DQTCLIENT_PACKS=<pack install prefix>`, which stages tip
`mods/<id>` (desktop: incl. `native/mod.*`); product urls replace `qtc-aac`.
DEV builds load pack `qml/` + `overrides/` from the pack source tree
(`QTCLIENT_DEV_PACK_SOURCES`; F5 / file watch) — see [`docs/mods.md`](../../docs/mods.md).

## Inplace (gitignored)

Other experimental packs under this directory are gitignored. An in-tree pack is
FS-only (`mod.json`, `qml/`, `overrides/`); one with C++ builds against the SDK like any
out-of-tree pack (a `CMakeLists.txt` here fails the host configure). Prefer
`QTCLIENT_PACK` + `make qtclient desktop install` for product tips.

Enable with default `QTCLIENT_ENABLE_MODS=ON` (auto-discovers non-`_*` packs).
Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF` or run with `--no-mods`.

QML must be a filesystem module: `qml/<qmlModule>/qmldir` (see [`docs/mods.md`](../../docs/mods.md)).
Pack surface: the host-pushed C ABI [`include/qtclient/abi/host_api_v1.h`](../../include/qtclient/abi/host_api_v1.h)
(SDK: [`include/qtclient/`](../../include/qtclient/README.md)).

See [`docs/mods.md`](../../docs/mods.md).
