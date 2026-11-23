# Local play tip (launcher-first)

CipSoft’s launcher always installs from a tip: `tibiametadata.json` lists
`packages[]` (live: **Tibia** + **Tibia External Test**). There is **no hard
client cap** — the tip is unbounded; the UI wraps at four tiles per row. Each
package has its own `directoryname` and `package.json`, and shares the
package/assets split (`assetsurl` / `assetsversionurl`).

Local play follows that contract:

```bash
make qtclient desktop build    # or make stock desktop build
make qtclient launcher build
make qtclient desktop run      # emit tip + launcher Update/Play
```

Same in the qtclient checkout: `make desktop build && make launcher build && make desktop run`.

## What the tip contains

| Source | Tip tile |
|--------|----------|
| Current checkout with a built client | `regular`, `installautomatically: true` |
| Sibling git worktrees with a built client | `test` / `beta` / `preview` art |
| Explicit `--pack` / `PLAY_PACK_FLAGS='--pack …'` | Extra tile reusing the primary engine + that pack’s FS chrome |
| Sibling pack-only worktrees (no client binary) | Opt-in: `QTCLIENT_PLAY_PACK_WORKTREES=1` |

All packages share one `assetsurl` (local `http://127.0.0.1:9080/launcher/assets-current/…`
when `make cdn` has assets; otherwise CipSoft `static.tibia.com`). Code- or
pack-only changes bump only `package.json.version`.

Emitter: [`tools/publish/emit_play_tip.py`](../../tools/publish/emit_play_tip.py).
List sources without writing: `python3 tools/publish/emit_play_tip.py --list`.

Flags:

```bash
make desktop run PLAY_PACK_FLAGS='--current-only'   # this checkout only
make desktop run PLAY_PACK_FLAGS='--cip-assets'     # force CipSoft assetsurl
make desktop run PLAY_PACK_FLAGS='--pack /path/to/pack/client'
```

## Convenience vs CipSoft parity

| Behaviour | CipSoft | Local play tip |
|-----------|---------|----------------|
| Launcher → Update → Play | yes | yes (`make desktop run`; no `QTCLIENT_DEV_JSON` bypass) |
| Product mods on Play | n/a (CIP has none) | `make qtclient desktop run` omits `--no-mods`; stock keeps it |
| Tip `packages[]` | yes (2 live) | yes (worktrees + packs) |
| Shared assets version URL | yes (per CDN host) | yes (one local or CIP assets-current) |
| Engine file transport | lzma | raw files + `"unpack": false` (same field CIP uses for sprites) for fast iteration |
| Direct client binary | no | `DIRECT=1 make desktop run` debug / F5 hatch |
| Window size | fixed ~800×600, no maximize | same (CipSoft `setFixedWidth` / no maximize hint) |

## Related

- Demo tip (single package + CipSoft assets): [`demo-tip.md`](demo-tip.md)
- Package layout: monorepo `docs/reverse-engineering/cip-package-layout.md`
