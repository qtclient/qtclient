# Rust pack MODULE (host-pushed C ABI)

qtclient stays C++. Packs may be Rust `cdylib` modules that export the same
entry as C/C++ packs.

## Contract

1. Bindgen [`include/qtclient/abi/*.h`](../include/qtclient/abi/).
2. Export `qtclient_mod_register` matching
   [`host_api_v1.h`](../include/qtclient/abi/host_api_v1.h).
3. Use only the passed `QtClientHostApiV1*` — do not link tip symbols.
4. Stage `mods/<id>/native/mod.{dylib,so,dll,wasm}` + FS chrome.

## Skeleton

```toml
# Cargo.toml
[lib]
crate-type = ["cdylib"]

[build-dependencies]
bindgen = "0.69"
```

```rust
// src/lib.rs
use std::os::raw::{c_char, c_int, c_void};

#[repr(C)]
pub struct QtClientHostApiV1 { /* bindgen */ }

#[no_mangle]
pub unsafe extern "C" fn qtclient_mod_register(
    api: *const QtClientHostApiV1,
    _ctx: *const c_void,
) {
    // register hooks via (*api).register_mod / set_* 
}
```

Ship the cdylib as `mods/<id>/native/mod.dylib` (rename/copy per platform).
The tip ModLoader resolves `qtclient_mod_register` by name.

## Smoke crate

Working example (bindgen + no-op `set_feature_extras_hook`):

[`apps/idleot-client/rust-smoke/`](../../idleot-client/rust-smoke/)

```bash
cd apps/idleot-client/rust-smoke && cargo build --release
```
