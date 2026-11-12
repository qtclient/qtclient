# Thin CIP stubs (not ports)

Placeholders so a CIP RTTI name has a `class TFoo :` declaration while behavior is
still missing. The obligation ledger marks everything here as **`stub`**, never
`ported`.

When you decompile-back a class:

1. Move the real type into `client/tibia/<area>/`.
2. Wire it into a runtime path.
3. Delete the stub from the matching `*_ports.h` here.
4. Append `docs/reverse-engineering/ledgers/wired.json`.
5. Regenerate ledgers and lower `missing-ceiling.txt`.
