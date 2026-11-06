# qtclient pack SDK interface

**Runtime MODULE packs** must use the host-pushed C ABI only:

| Header | Role |
|--------|------|
| `abi/host_api_v1.h` | `QtClientHostApiV1` — capability surface |
| `abi/mod_register.h` | `qtclient_mod_register` export |
| `abi/types.h` / `lighting.h` (lighting driver + world overlay vtables) / `registry.h` / `session.h` / `const.h` | POD + hooks |

Extending pack access means adding fields to `QtClientHostApiV1` (bump
`api_version` when breaking). Packs never import tip symbols.

**Transitional C++ façades** (IdleOT `ABSORB_INTO_TIP` only) still live beside
this tree (`session.h`, `worldview.h`, `pack_host.h`, …). New packs must not
depend on them — peel onto `abi/` instead.

Rust: see [`docs/rust-pack-module.md`](../docs/rust-pack-module.md).
