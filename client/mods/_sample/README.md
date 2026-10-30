# Sample client pack

Unified layout (desktop + WASM):

```
mods/sample/
  mod.json
  native/mod[.so|.dylib|.dll|.wasm]
  qml/sample/qmldir + SampleLabel.qml   # FS QML module
```

Uses `qtclient/mod_api_v1.h` only (no `QTCLIENT_MOD_INTERNAL`).

Build with `qtclient_add_mod`. Enable via `-DQTCLIENT_MODS=_sample` or copy to
`client/mods/sample` (auto-discovered).

At runtime the stock host loads the pack with
`ModLoader::loadFromDirectory` — same path on desktop and `/play` (OPFS).
