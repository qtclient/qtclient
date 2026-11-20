#include "runtime/host.h"

#include "platform/platform.h"

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
    return tibia::platform::osName();
}

}
