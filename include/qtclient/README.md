# qtclient pack SDK interface

**Packs** use the host-pushed C ABI only (host API **v9**, `include/qtclient/abi/`). The
host passes one `QtClientHostApiV1` table into the pack's entry; nested tables cover each
domain. Packs never import tip symbols and never see a tip class.

| Header | Table / role | Host implementation |
|--------|--------------|---------------------|
| `abi/host_api_v1.h` | `QtClientHostApiV1`: root table (version, build id, Qt version, hooks, nested tables) | `client/tibia/mods/host_api_v1.cpp` |
| `abi/mod_register.h` | the pack entry: `QTCLIENT_MOD_DEFINE_REGISTER(c_id)` = exported `qtclient_mod_register` (desktop MODULE) or `qtclient_mod_<c_id>_register_abi` (WASM static-ABI, `QTCLIENT_MOD_STATIC_ABI`) | `mods/modloader.cpp` |
| `abi/types.h` | conventions (threading, strings, lists, subscriptions, Qt pointers), `QtClientMapPos`, `QtClientModLoadCtx` | — |
| `abi/context.h` | `QtClientGameContextV1`: QML engine, game window items, launcher urls (`ctx->game` in bootstraps) | `mods/context_abi.cpp` |
| `abi/gamehost_api.h` | `game_host`: lifecycle events, character switch, HD mode | `mods/gamehost_abi.cpp` |
| `abi/opcode_api.h` | `opcode`: extended opcodes in / out (raw and JSON) | `mods/opcode_abi.cpp` |
| `abi/dialog_api.h` | `dialog`: pack dialogs on CipSoft's dialog stack, message boxes, host dialogs | `mods/dialog_abi.cpp` |
| `abi/action_api.h` | `action`: game actions by CipSoft name + JSON, context menu extensions, target selection | `mods/action_abi.cpp` |
| `abi/chat_api.h` | `chat`: NPC channel lines, chat mode, external NPC conversations | `mods/chat_abi.cpp` |
| `abi/gamedata_api.h` | `cooldown`, `game_data`: cooldowns, spells, charms, palettes | `mods/gamedata_abi.cpp` |
| `abi/ui_api.h` | `ui`: sidebars and sidebar widgets, minimap, Cyclopedia map renderer, store images, sprites | `mods/ui_abi.cpp` |
| `abi/backends.h` | `backends`: mission / attention / instance backends as C vtables | `mods/backend_adapters.cpp` |
| `abi/cip_object_api.h` | `cip_object`: CipSoft model / controller objects for CipSoft QML (social, team finder) | `mods/cip_object_abi.cpp` |
| `abi/session_api.h` / `session.h` | `session`: the current game session's peels | `mods/host_api_v1.cpp` (session part) |
| `abi/notify_api.h` | `notify`: CipSoft field hooks, typed args (`arg_json`), session / container changes | `mods/host_api_v1.cpp` (notify part) |
| `abi/pack_host_api.h` | `pack_host`: options page, chrome panels, map overlay input | `mods/pack_host_abi.cpp` |
| `abi/options_api.h`, `sound_api.h`, `lighting.h`, `registry.h`, `const.h` | options store, ambience hold, lighting driver / world overlays / frame hooks, hook types, constants | as named in each header |

Extending pack access means adding fields at a table's tail (v9 tables carry
`struct_size`; check with `QTCLIENT_ABI_HAS(table, Type, field)`) or bumping
`api_version` when breaking. Packs never import tip symbols.

**Pointers.** A raw `void*` in the ABI is only ever the pack's own `user` / `self`
context. Qt objects travel as `QtClientQObjectPtr`, `QtClientQQuickItemPtr`,
`QtClientQQmlEnginePtr`, `QtClientQQuickWindowPtr` (`types.h`): the pointer is an instance of
that Qt class and the pack uses only Qt's API on it (properties, signals, invokables by name).
Host objects are reached through typed tables, host-opaque handles (`QtClientHandle`,
incomplete structs such as `QtClientDialog`) or ids — never cast.

**Build modes.** Desktop: the pack is a runtime MODULE (`mods/<id>/native/mod.*`) linking the
tip's shared Qt kit. WASM: the same sources, compiled against `abi/` only, are a static
library linked into `tibia.wasm` (the Qt WASM kit is static, so a Qt pack cannot be a
`dlopen`ed SIDE_MODULE), registered through the same entry signature.

**SDK package.** This tree is all a pack sees of the host. It ships as `QtClientSdk`
([`sdk/`](../../sdk/CMakeLists.txt), `scripts/lifecycle/build/sdk.sh`): `include/qtclient/abi/`,
`lib/cmake/QtClientSdk/` (`QtClientMod.cmake`; the config records the ABI version,
`hostBuildId` and the host's Qt kit) and `share/qtclient-sdk/sdk.json`. A pack builds with
`find_package(QtClientSdk)` + Qt only — no host build, host sources or tip targets — and the
host takes the installed pack (`-DQTCLIENT_PACKS`). See
[`docs/mods.md` → Build](../../docs/mods.md#build-sdk--pack--host).

**Non-abi façades removed (v9).** The transitional C++ façades that used to sit beside `abi/`
(`worldview.h`, `pack_host.h`, `dialog.h`, `gameactions.h`, `session.h`, `cooldown.h`,
`gamewindow/…`, …) each forwarded to a tip header and only served the old
`ABSORB_INTO_QTCLIENT` build. They are gone with it; `include/qtclient/` holds `abi/` and this
README only (`scripts/ci/check-cip-sdk-interface.py`). `pack_host.h` became host-internal
(`client/tibia/mods/`).

## Game session objects

Each login, replay or tutorial is a new game session with new storages (CipSoft
`TGameSessionBase`). A pack never keeps a pointer to one, and never calls a
per-session class's `T::instance()`:

- Every `session->`, `cooldown->`, `chat->` and `ui->` read resolves the current session at
  call time (0 / empty between sessions).
- `notify->subscribe_session_changed` (v5+) runs after each change of the current
  session: rebind there. `subscribe_session_end` runs when the current session disconnects,
  while it is still current. Cooldown, container and store-image subscriptions follow the
  session by themselves.

Rust: see [`docs/mods.md` → Rust packs](../../docs/mods.md#rust-packs).

## Logging

The host owns CipSoft's process log (`shared/src/logging.cpp`): one spdlog
logger writing every Qt message as `[ yyyy-mm-dd hh:mm:ss,zzz ] message` to
`log/client.log` (1 MiB, one backup) and stdout. Qt's message handler is
process-wide, so a pack's `qDebug` / `qInfo` / `qWarning` / `qCritical` land in
the same file with no extra API. Packs log the way CipSoft's client does:

- Plain default-category calls; no product prefixes (`[Tag]`, `mypack:`), no
  perf or frame-time lines.
- QML load failures: `qCritical() << "Component creation error for " << url.url() << "\n" << component.errors();`
- File / JSON failures: `qWarning() << "Failed to open" << path;`,
  `qWarning() << "Failed to parse JSON:" << error.errorString();`
- Chatty subsystems log with `qCDebug` on their own category, off by default
  like CipSoft's `sound.*` (`Q_LOGGING_CATEGORY(lcWeather, "weather", QtFatalMsg)`);
  `QT_LOGGING_RULES=weather.debug=true` turns one on.
- Never install a message handler or call `QLoggingCategory::setFilterRules`.
