#include "runtime/host.h"

#include <QtGlobal>

namespace tibia::host {

namespace {
App g_host;
}

App& app()
{
    return g_host;
}

std::string App::getOs() const
{
#if defined(Q_OS_WIN)
    return "windows";
#elif defined(Q_OS_MACOS)
    return "mac";
#elif defined(Q_OS_ANDROID)
    return "android";
#elif defined(Q_OS_WASM)
    return "browser";
#else
    return "linux";
#endif
}

}
