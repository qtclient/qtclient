#pragma once

#if defined(__EMSCRIPTEN__)
/// Ensure `/home/web_user` exists on WasmFS (memory scaffold).
/// Tip I/O uses Module.tipFs + Qt file engine when portal installs it; this
/// mount is skipped when portal already prepared CIP_HOME / tipFs.
/// Call before any qtclient QFile / QStandardPaths AppData use.
bool tibiaMountWasmfsOpfsHome();
#endif
