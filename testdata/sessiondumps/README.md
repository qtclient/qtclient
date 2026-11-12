# Session-dump fixtures

`--replay` / `TSessiondumpGameSession` play hex packet logs (same line format
the client writes under `records/` via `--record`). CIP `dmpd` protobuf dumps
are partially mapped (login/light/scalars + MoveCreature `0x6D` + synthetic
FullMap); prefer `.cam` for smoke oracles. Lines are:

```
< <millis> <hex-body>
> <millis> <hex-body>
```

`<` is server→client (body only, no 2-byte size). `>` is client→server (ignored
on replay except logout `0x14`). Prefer absolute paths or paths under this
directory. Runtime recordings land in cwd `records/` (gitignored).

## Fixture preference (`make tibia-dump-smoke`)

`scripts/ci/check-session-dump-smoke.sh` picks the first available dump:

1. `TIBIA_DUMP_FIXTURE` (or alias `TIBIA_SESSION_DUMP`)
2. `play.cam` in this directory (richer **modern-protocol** dump when present)
3. `smoke.cam` (tiny synthetic oracle)

Smoke still **fails** on crash, missing world entry / PNG, or a tiny PNG.

## Checked-in synthetic dump

`smoke.cam` is a tiny synthetic dump for CI / `make tibia-dump-smoke`:

1. `0x0A` PendingGame
2. `0x0F` EnterGame → `onGameStart` / world entered
3. `0x05` SessionDumpMarker `"smoke"` at 5s (keeps the player open past the shot)

It does **not** include a map description; the world view is empty. That is
enough to fail on crash, missing world entry, or a zero-size PNG.

## Richer dump (`play.cam`)

`play.cam` is a **protocol 1530** live recording (Crystal login → world entry with
map). Prefer it over `smoke.cam` when present. Override with:

```bash
TIBIA_DUMP_FIXTURE=/path/to/dump.cam make tibia-dump-smoke
```

Software-renderer smoke needs image nodes textured before parenting (see
`TWorldMapQuickItem::updatePaintNode`); empty `smoke.cam` never hit that path.

## Recording a richer dump (pixel oracle)

With Crystal + portal up and a things pack installed:

```bash
make client-start ARGS='--account=EMAIL --password=PW --character=NAME --record=smoke-world.cam --play-for=15'
```

The recorder writes under cwd `apps/qtclient/` (e.g. `smoke-world.cam`). Copy to
this directory as `play.cam`, or set `TIBIA_DUMP_FIXTURE`.

## CIP tutorial `dmpd`

Official tutorial assets use a binary `dmpd` container (CIP
`TSessiondumpAsyncLoader`). Layout: magic `dmpd` + u16be version + `@sessionKey|`
metadata + `'p'` packet headers + `'m'` length-prefixed protobuf
`GameserverMessage` chunks.

`--replay` detects magic `dmpd` and ingests via `sessiondumpdmpd` → maps known
opcodes (PendingGame / EnterGame / SessionDump* / Ping) into classic bodies on
the `g_game` packet path; unknown protobuf types are skipped with a log. Map /
creature protobuf payloads are **not** converted yet (no field schemas).

Fixture resolution:

1. `TIBIA_TUTORIAL_SESSIONDUMP=/path/to/tutorial-sessiondump.dmp`
2. `testdata/sessiondumps/tutorial-sessiondump.dmp` (copied from RE extract)
3. `tmp/re/tibia/*/tutorial/tutorial-sessiondump.dmp`

```bash
make client-start ARGS='--replay=testdata/sessiondumps/tutorial-sessiondump.dmp'
# or
TIBIA_TUTORIAL_SESSIONDUMP=tmp/re/tibia/ui-art/tutorial/tutorial-sessiondump.dmp \
  make client-start ARGS="--replay=$TIBIA_TUTORIAL_SESSIONDUMP"
```

This is **not** the OTC `< millis hex` format — do not rename `.dmp` to `.cam`.

### Login handoff (`character.tutorial=true`)

When portal login returns `tutorial: true` for a character, `TGameClient` resolves
the fixture via `resolveTutorialSessiondumpPath()`, replays it with
`TSessiondumpGameSession`, then on dump end / `FINISHED_TUTORIAL_FOR_CHARACTER`
(or empty `SESSIONDUMP_SKIP_TO`) connects to the live Newhaven world with the
pending credentials. Missing fixture → log and enter live Newhaven immediately.

`dmpd` now also maps protobuf `TutorialHint` (0xDC) → classic `0xDC` + hint id
when the nested varint field is present.

### Hints sidecar

When a sibling `*.hints` JSON exists next to the dump, `TSessiondumpHintsProcessor`
loads it and fires timestamped actions during replay (`SHOW_TUTORIAL_HINT`,
`TUTORIAL_PROGRESS`, condition overlays as progress text). Copy the CIP extract:

```bash
cp tmp/re/tibia/ui-art/tutorial/tutorial-sessiondump.hints \
   apps/qtclient/testdata/sessiondumps/
```

### Remaining gaps

- Condition overlays (mouse-trail / GUI markers) still log + progress text;
  `TTutorialHelperQmlService` / `TibiaTutorialMarker` are wired for QML lookup.
- CIP protobuf tile/creature blobs are not field-converted yet; the first
  `FullMap` becomes a **synthetic** classic map (ground + local player at the
  dump coordinate) so the cinematic camera is not blank. LoginSuccess, world
  light, and simple creature scalar updates (health/speed/light/…) are mapped.
- Live Newhaven vocation pick uses `sendTutorial(2)` →
  `TVocationSelectionDialogController` (independent of the cinematic).

Smoke oracle (play.cam): `play-baseline.oracle` pins IHDR + 8×8 average-hash
(`max_hamming=24` for animation phase). Override with `TIBIA_SCREENSHOT_ORACLE`.
Optional extra IHDR check: `TIBIA_SCREENSHOT_BASELINE=/path.png`.
