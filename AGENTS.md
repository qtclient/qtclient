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

## Porting CipSoft code (mandatory)

- Every class with a CipSoft counterpart is written from its port unit (the
  decompiled class), never from memory, OTClient or invention. Recipe: skill
  `cip-port` in the umbrella (`.cursor/skills/cip-port/SKILL.md`).
- `client/tibia/ports.lock` and `launcher/ports.lock` record which CipSoft
  functions are ported, tested or skipped, each pointing at the qtclient
  symbol. Edit them only with the umbrella's `tools/re/port.py`; never lock a
  function you did not compare with its unit.
- No addresses, `sub_…`/`FUN_…` ids or build shas in comments or names.
- Ported code stays platform-neutral, like CipSoft's: no `#if __EMSCRIPTEN__`
  workarounds for the web (thread pools, `waitForDone`, file I/O on the GUI
  thread). Browser and Safari compatibility belongs below it: the Qt fork
  (`qtclient/Qt5`) or the browser runtime (`client/tibia/browser/`,
  `browser/qtclient-fs.js`; e.g. `main_thread_wait.cpp` keeps main-thread
  waits Safari-safe). Reproduce browser claims first with `tools/safari-poc`.
- The browser is a platform like Windows, macOS and Linux. Every behaviour
  that differs per platform goes behind `client/tibia/platform/platform.h`
  (compile-time capabilities in `platform/capabilities.h`), implemented once per
  platform in `platform_{browser,desktop,mac,windows,linux}.cpp`. Shared code
  calls those functions instead of adding `Q_OS_*` / `__EMSCRIPTEN__` / `_WIN32`
  `#if`s. Remaining exceptions: the game-server transport classes in
  `network/` (TCP vs WebSocket), `framework/pch.h`, the
  WebGL shader preamble (`renderer/wasnglsl.h`) and the browser runtime glue
  in `browser/`.

## Smoke runs (host options, no CipSoft counterpart)

Never drive the desktop with synthetic mouse or keyboard input. Script the run
instead; every step starts once the world is entered (`--character` login or
`--replay`):

- `--say <text>` (repeatable), `--walk <dirs>`, `--play-for <s>`,
  `--screenshot <file>` with `--screenshot-delay <ms>`.
- `--smoke-action <EGameAction name>[:<ms>]` (repeatable, run in order): publishes
  the game action through the client's action router, `<ms>` (default 1000)
  after the previous step. The action carries only its type, so use actions
  without a payload (e.g. `TOGGLE_FULLSCREEN`). `CONFIRM_TUTORIAL_HINT` confirms
  the tutorial hint shown last.
- `--smoke-confirm-hint`: confirms every tutorial hint 500 ms after it shows
  (not hint 2, the vocation selection).

Example (tutorial replay):
`tibia --renderer software --replay t.cam --smoke-confirm-hint --smoke-action CONFIRM_TUTORIAL_HINT:3000 --screenshot shot.png --screenshot-delay 8000`.

## Do not

- Commit `upstream/` or CipSoft asset blobs.
- Hand-edit generated CIP QML under `upstream/`.
- Add a second client upstream or copy behavior from other open clients.
