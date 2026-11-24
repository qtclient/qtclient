# Tip-internal IdleOT island

Product / tip-only infrastructure that **has no CipSoft contract twin**.

Rules:

- Do **not** invent fake `T*` CipSoft names for types here.
- Prefer migrating *callers* onto CipSoft ports when a twin exists; leave
  this island for IdleOT-only geometry / lighting / product helpers.

Current residents:

| Path | Role |
|------|------|
| `heightstamp.*` | Art → 3D height stamps for sun shadows + raycast lighting |
| `appearanceshape.*` | `ThingShape` of a CipSoft `TAppearanceType` (stamps + weather splashes) |
| `stats.*` | `g_stats` / `AutoStat` profiler (no CIP twin) |
