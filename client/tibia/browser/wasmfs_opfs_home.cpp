#if defined(__EMSCRIPTEN__)

#include "wasmfs_opfs_home.h"

#include <cerrno>
#include <sys/stat.h>

#include <emscripten.h>
#include <emscripten/console.h>
#include <emscripten/wasmfs.h>

namespace {

constexpr const char* kHome = "/home/web_user";

bool mountHome(backend_t backend, const char* kind)
{
    if (mkdir("/home", 0777) != 0 && errno != EEXIST) {
        emscripten_console_errorf("tibia.wasm: mkdir /home failed errno=%d", errno);
        return false;
    }

    const int err = wasmfs_create_directory(kHome, 0777, backend);
    if (err != 0) {
        emscripten_console_errorf(
            "tibia.wasm: wasmfs_create_directory(%s) => %d errno=%d",
            kHome,
            err,
            errno);
        return false;
    }

    EM_ASM({
        Module.qtclientWasmfsOpfs = 0;
        Module.qtclientPersistentFsMounted = 1;
    });
    emscripten_console_logf("tibia.wasm: %s mounted on WasmFS %s", kHome, kind);
    return true;
}

} // namespace

bool portalTipAlreadyOnHome()
{
    // Portal either hydrated CIP_HOME into Module.FS or installed Module.tipFs
    // (Qt file engine). Remounting /home/web_user would hide tip or race the
    // engine — skip when either flag / packages/ is present.
    return EM_ASM_INT({
        if (Module.qtclientPortalHydratedTip || Module.qtclientFs)
            return 1;
        try {
            if (typeof FS === 'undefined' || !FS.analyzePath)
                return 0;
            if (!FS.analyzePath('/home/web_user').exists)
                return 0;
            var pkgs = '/home/web_user/.local/share/IdleOT/IdleOT/packages';
            if (FS.analyzePath(pkgs).exists)
                return 1;
            pkgs = '/home/web_user/.local/share/Tibia/Tibia/packages';
            return FS.analyzePath(pkgs).exists ? 1 : 0;
        } catch (e) {
            return 0;
        }
    }) != 0;
}

bool tibiaMountWasmfsOpfsHome()
{
    if (portalTipAlreadyOnHome()) {
        emscripten_console_log(
            "tibia.wasm: portal tip already on /home/web_user — skip WasmFS remount");
        EM_ASM({
            Module.qtclientPersistentFsMounted = 1;
            Module.qtclientWasmfsOpfs = 0;
            Module.qtclientPortalHydratedTip = 1;
        });
        return true;
    }

    // Tip durability is Module.tipFs (JS OPFS Worker) + Qt file engine — not
    // Emscripten's wasmfs_create_opfs_backend (needs JSPI on the main thread;
    // we do not link -sJSPI). Memory-backed home is only a fallback scaffold.
    backend_t memory = wasmfs_create_memory_backend();
    if (!memory) {
        emscripten_console_error("tibia.wasm: wasmfs_create_memory_backend failed");
        return false;
    }
    return mountHome(memory, "memory (tip via Module.tipFs when present)");
}

#endif // __EMSCRIPTEN__
