# Tip-internal IdleOT island

Product / tip-only infrastructure that **has no CipSoft contract twin**.

Rules:

- Do **not** invent fake `T*` CipSoft names for types here.
- Keep this tree off `world/` + `engine/` keep-alive scan roots
  (`check-cip-keep-alive-allowlist.py`).
- `tools/re/cip_function_inventory.py` skips `idleot/` (`SKIP_TIP_DIR`).
- Prefer migrating *callers* onto CipSoft ports when a twin exists; leave
  this island for IdleOT-only geometry / lighting / product helpers.

Current residents:

| Path | Role |
|------|------|
| `heightstamp.*` | Art → 3D height stamps for sun shadows + raycast lighting |
| `stats.*` | OTC/TFS-style `g_stats` / `AutoStat` profiler (no CIP twin) |
