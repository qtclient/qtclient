# Mods

Stock CipSoft is **not** under `mods/` (`upstream/` + `client/tibia/`).

| OTC | This client |
|-----|-------------|
| `modules/` (stock) | CIP stock |
| Customizing stock | `client/overrides/` |
| `mods/<name>/` | `mods/<name>/` + `mod.json` + `qml/<uri>/` |

## Inplace (gitignored)

Only `_sample` and this README are tracked. Drop private packs here:

```bash
ln -s ~/my-pack mods/my_pack
# or product: make idleot install   # links + builds qtclient host/MODULE
```

Enable with default `QTCLIENT_ENABLE_MODS=ON` (auto-discovers non-`_*` packs).
Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF`.

QML must be a filesystem module: `qml/<qmlModule>/qmldir` (see [`docs/mods.md`](../../docs/mods.md)).
Public C++ surface: [`include/qtclient/mod_api_v1.h`](../../include/qtclient/mod_api_v1.h).

See [`docs/mods.md`](../../docs/mods.md) and [`_sample/`](_sample/).
