# Session recordings

`--record=<name.cam>` writes a hex packet log under this directory (cwd when
the client starts). Replay with:

```bash
make client-start ARGS='--replay=records/your.cam'
# or a checked-in fixture:
make tibia-dump-smoke
# → testdata/sessiondumps/play.cam (preferred) or smoke.cam
```

Do **not** commit recordings here — they are gitignored. Checked-in smoke
fixtures live in [`testdata/sessiondumps/`](../testdata/sessiondumps/README.md).

CIP tutorial `dmpd` dumps are also supported via `--replay` (see that README).
