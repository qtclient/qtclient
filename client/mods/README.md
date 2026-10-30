# Mods

Stock CipSoft is **not** under `mods/` (`upstream/` + `client/tibia/`).

| OTC | This client |
|-----|-------------|
| `modules/` (stock) | CIP stock |
| Customizing stock | `client/overrides/` |
| `mods/<name>/` | `mods/<name>/` + `mod.json` + `qml/<uri>/` |

## Tracked packs

| Pack | Role |
|------|------|
| `_sample/` | SDK example |
| `default/` | Demo tip myAAC urls (`/login.php`); staged by `make demo-pack` |

IdleOT lives in `apps/idleot-client` — use `QTCLIENT_MODS_DIR` (no required symlink).
Tip install replaces `mods/default` with `mods/idleot` (single slot).

## Inplace (gitignored)

Other packs under this directory are gitignored. Drop private packs here:

```bash
ln -s ~/my-pack mods/my_pack
# or product: make idleot install   # links + builds stock qtclient/MODULE
```

Enable with default `QTCLIENT_ENABLE_MODS=ON` (auto-discovers non-`_*` packs).
Stock-only: `-DQTCLIENT_ENABLE_MODS=OFF`.

QML must be a filesystem module: `qml/<qmlModule>/qmldir` (see [`docs/mods.md`](../../docs/mods.md)).
Public C++ surface: [`include/qtclient/mod_api_v1.h`](../../include/qtclient/mod_api_v1.h).

See [`docs/mods.md`](../../docs/mods.md) and [`_sample/`](_sample/).
