# Demo tip (portal + CipSoft assets)

Login comes from the default tip pack [`mods/portal`](../../client/mods/portal/mod.json)
([portal](https://github.com/qtclient/portal) at `http://127.0.0.1:5173`).
The client speaks the CIP wire (CIP OS ids), so Crystal keeps its CIP layouts.
Product tips add `mods/<id>` (portal urls); multiple
autoload packs are supported (identity/urls: lower `priority` wins).

## Local demo

1. Run [portal](https://github.com/qtclient/portal) so `http://127.0.0.1:5173/clientservices/loginservice.php` answers CIP JSON (`type: login`).
2. `make stock demo-pack` (or CI demo artifacts).
3. Point the launcher tip at the emitted `tibiametadata.json`.
4. **Update** installs our client package + CipSoft `assets-current` (package includes default mods + launcher channel gates).
5. **Play** logs in at `http://127.0.0.1/login.php` with no env tweaks.

Stock CipSoft URLs:

```bash
./client --no-mods
# or: DIRECT=1 make stock desktop run ARGS='--no-mods'
```

Override login without rebuilding:

```bash
export QTCLIENT_LOGIN_BASE=http://127.0.0.1
export QTCLIENT_LOGIN_PATH=/login.php
```

Or edit/reinstall `mods/portal/mod.json` `urls` — no host rebuild.

## Product tip

`make qtclient desktop install` merges product pack FS chrome (portal urls; often
without `portal`). Run loads all autoload tip packs. A pack's `launcher.json` (`"channels"`)
enables the optional CDN download channels of the launcher configuration.

## Emitter

`emit_demo_tip.py` stages `mods/portal` next to the Play binary and writes a
CipSoft-shaped tip with CipSoft `assetsurl`.
