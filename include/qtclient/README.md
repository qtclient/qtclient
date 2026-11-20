# qtclient pack SDK interface

**Runtime MODULE packs** must use the host-pushed C ABI only:

| Header | Role |
|--------|------|
| `abi/host_api_v1.h` | `QtClientHostApiV1` — capability surface |
| `abi/mod_register.h` | `qtclient_mod_register` export |
| `abi/types.h` / `lighting.h` (lighting driver + world overlay vtables) / `registry.h` / `session.h` / `const.h` | POD + hooks |

Extending pack access means adding fields to `QtClientHostApiV1` (bump
`api_version` when breaking). Packs never import tip symbols.

**Transitional C++ façades** (IdleOT `ABSORB_INTO_QTCLIENT` only) still live beside
this tree (`session.h`, `worldview.h`, `pack_host.h`, …). New packs must not
depend on them — peel onto `abi/` instead.

Rust: see [`docs/rust-pack-module.md`](../docs/rust-pack-module.md).

## Logging

The host owns CipSoft's process log (`shared/src/logging.cpp`): one spdlog
logger writing every Qt message as `[ yyyy-mm-dd hh:mm:ss,zzz ] message` to
`log/client.log` (1 MiB, one backup) and stdout. Qt's message handler is
process-wide, so a pack's `qDebug` / `qInfo` / `qWarning` / `qCritical` land in
the same file with no extra API. Packs log the way CipSoft's client does:

- Plain default-category calls; no product prefixes (`[Tag]`, `idleot:`), no
  perf or frame-time lines.
- QML load failures: `qCritical() << "Component creation error for " << url.url() << "\n" << component.errors();`
- File / JSON failures: `qWarning() << "Failed to open" << path;`,
  `qWarning() << "Failed to parse JSON:" << error.errorString();`
- Chatty subsystems log with `qCDebug` on their own category, off by default
  like CipSoft's `sound.*` (`Q_LOGGING_CATEGORY(lcWeather, "weather", QtFatalMsg)`);
  `QT_LOGGING_RULES=weather.debug=true` turns one on.
- Never install a message handler or call `QLoggingCategory::setFilterRules`.
