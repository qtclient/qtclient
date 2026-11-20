# Mod / WorldView performance oracle

Extensibility must not regress the CIP world hot path. Use the existing frame
timers (`provider->buildNs()` / `tilesNs()` / `lightNs()`); the client logs only
CipSoft's messages, so read them from the FPS / latency indicator (world map
build time) or a profiler, not from the log.

## Stock baseline (no packs)

```bash
cmake --build … --target client
./client --replay=records/smoke-world.cam --play-for=20
# or: --account=… --password=… --character=… --play-for=20
```

Record average `worldmapBuildUs` / FPS from the FPS / latency indicator.

## After WorldView / pack changes

1. Stock (`QTCLIENT_ENABLE_MODS=OFF` or empty `mods/`): within noise of baseline
   (investigate sustained >~2% frame-time increase).
2. qtclient + raycast on: within noise of the **previous** in-tree qtclient raycast
   on the same machine/replay.

Rules enforced in code:

- `ModLoader` only at startup
- Per-frame: cached `WorldLightingDriver*` + null check
- No `mod.json` / directory / `QLibrary` work inside `advance()`
