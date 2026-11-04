# Sample client pack

Unified layout (desktop + WASM):

```
mods/sample/
  mod.json
  qml/sample/qmldir + SampleLabel.qml   # FS QML module
```

Exports `qtclient_mod_sample_entry` (STATIC into tip via `qtclient_add_mod`).
Uses `qtclient/mod_api_v1.h` only (no `QTCLIENT_MOD_INTERNAL`).

Build with `qtclient_add_mod`. Enable via `-DQTCLIENT_MODS=_sample` or copy to
`client/mods/sample` (auto-discovered).

At runtime `ModLoader::loadAll` invokes linked entries then loads every autoload
FS pack — same path on desktop and `/play` (OPFS).
