# Legacy `data/things/<ver>/`

**Unsupported for new work.** Official CIP assets land in flat
`data/assets/` via `make extract` (catalog, appearances, map, minimap,
`graphics_resources.rcc`, `spells.json`, `game-tables/` for the pipeline).

Do not add new code that reads `data/things/<ver>/`. Older bootstrap trees
may still exist locally; remove them after a successful extract into
`data/assets/`.
