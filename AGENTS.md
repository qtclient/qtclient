# AGENTS.md — qtclient

qtclient is a **CipSoft client port** (Qt/QML). CipSoft / CIP is the only client
upstream. Do not treat other open clients as upstream or as a behavioral source.

## Naming (mandatory)

- This host is **qtclient** (umbrella: `make stock` / `make qtclient`). Never call it
  Soft, Soft Soft, CIP Soft, or any Soft-* product nickname.
- Name the company **CipSoft** (full) or **CIP** (short). Never abbreviate CipSoft
  as Soft.
- Prefer **CIP** / **CipSoft** / **qtclient** in docs, comments, CI, and tickets —
  not Soft.

## CIP as upstream (mandatory)

- `client/tibia/` — C++/QML rewrite of the official client (desktop + WASM).
- **`client/upstream/` is vendored, never hand-edited.** From `make tibia-sync` / `make sync`.
- Product UI belongs in **runtime packs** (`client/mods/<id>/`), not stock `client/tibia/`.
- Packs export `qtclient_mod_<id>_entry` (STATIC tip); see [`docs/pack-architecture.md`](../../docs/pack-architecture.md).
- **No Lua gameplay host** — QML + C++ only.

## Do not

- Commit `upstream/` or CipSoft asset blobs.
- Hand-edit generated CIP QML under `upstream/`.
- Add a second client upstream or copy behavior from other open clients.
