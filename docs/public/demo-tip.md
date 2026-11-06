# Demo tip (myAAC + CipSoft assets)

Login comes from the default tip pack [`mods/myaac`](../../client/mods/myaac/mod.json)
([myAAC](https://github.com/slawkens/myaac) at `http://127.0.0.1/login.php`).
Crystal wire features come from [`mods/otclient`](../../client/mods/otclient/mod.json).
Product tips add `mods/<id>` (portal urls) alongside `otclient`; multiple
autoload packs are supported (identity/urls: lower `priority` wins).

## Local demo

1. Run [myAAC](https://github.com/slawkens/myaac) so `http://127.0.0.1/login.php` answers CIP JSON (`type: login`).
2. `make stock demo-pack` (or CI demo artifacts).
3. Point the launcher tip at the emitted `tibiametadata.json`.
4. **Update** installs our client package + CipSoft `assets-current` (package includes default mods + launcher channel gates).
5. **Play** logs in at `http://127.0.0.1/login.php` with no env tweaks.

Stock CipSoft URLs:

```bash
./client --no-mods
# or: make stock client run ARGS='--no-mods'
```

Override login without rebuilding:

```bash
export QTCLIENT_LOGIN_BASE=http://127.0.0.1
export QTCLIENT_LOGIN_PATH=/login.php
```

Or edit/reinstall `mods/myaac/mod.json` `urls` — no host rebuild.

## Product tip

`make qtclient client install` merges product pack FS chrome (portal urls; often
without `myaac`). Run loads all autoload tip packs. Default launcher mods
(`clientmods`, `minimap`) gate optional CDN channels.

## Emitter

`emit_demo_tip.py` stages `mods/myaac`, `mods/otclient`, and launcher
`clientmods`/`minimap` next to the Play binary and writes a
CipSoft-shaped tip with CipSoft `assetsurl`.
