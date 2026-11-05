# Mods

Stock CipSoft is **not** under `mods/` (`upstream/` + `client/tibia/`).

| OTC | This client |
|-----|-------------|
| `modules/` (stock) | CIP stock |
| Customizing stock | pack `overrides/` (host has none) |
| `mods/<name>/` | `mods/<name>/` + `mod.json` + `qml/<uri>/` |

## Default packs (in-tree)

| Pack | Role |
|------|------|
| `myaac/` | myAAC login urls (`/login.php`); staged by `make demo-pack` |
| `otclient/` | Crystal `isOTCR` wire (OS + shader / aura / attached-effect features) |

Product pack lives in [`apps/idleot-client`](../../../idleot-client/) (`QTCLIENT_PACK` in
root `.env`). `make qtclient client install` STATIC-links via `QTCLIENT_LINKED_PACK`
and stages tip `mods/idleot` alongside defaults (`otclient`); product urls replace
`myaac`. DEV builds load pack `qml/` + `overrides/` from the pack source tree
(F5 / file watch) — see [`docs/mods.md`](../../docs/mods.md).

## Inplace (gitignored)

Other experimental packs under this directory are gitignored. Drop a pack dir
here or point `QTCLIENT_LINKED_PACK` at an out-of-tree `client/`. Prefer
`QTCLIENT_PACK` + `make qtclient client install` for IdleOT.

Enable with default `QTCLIENT_ENABLE_MODS=ON` (auto-discovers non-`_*` packs).
Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF` or run with `--no-mods`.

QML must be a filesystem module: `qml/<qmlModule>/qmldir` (see [`docs/mods.md`](../../docs/mods.md)).
Public C++ surface: [`include/qtclient/mod_api_v1.h`](../../include/qtclient/mod_api_v1.h).

See [`docs/mods.md`](../../docs/mods.md).
