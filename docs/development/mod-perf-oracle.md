# Mod / WorldView performance oracle

Extensibility must not regress the CIP world hot path. Use existing frame timers
(`QTCLIENT_PERF` / `QTCLIENT_PERF`, `provider->buildNs()` / `tilesNs()` / `lightNs()`).

## Stock baseline (no packs)

```bash
cmake --build … --target client
QTCLIENT_PERF=1 ./client --replay=records/smoke-world.cam --play-for=20
# or: --account=… --password=… --character=… --play-for=20
```

Record average `worldmapBuildUs` / FPS from the perf log line.

## After WorldView / pack changes

1. Stock (`QTCLIENT_ENABLE_MODS=OFF` or empty `mods/`): within noise of baseline
   (investigate sustained >~2% frame-time increase).
2. qtclient + raycast on: within noise of the **previous** in-tree qtclient raycast
   on the same machine/replay.

Rules enforced in code:

- `ModLoader` only at startup
- Per-frame: cached `WorldLightingDriver*` + null check
- No `mod.json` / directory / `QLibrary` work inside `advance()`
