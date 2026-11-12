#include "gamelink.h"

#include <cstring>
#include <string>

namespace gamelink {

namespace {
Hooks g_hooks{};
bool g_installed = false;
}

void install(const Hooks& hooks)
{
    g_hooks = hooks;
    g_installed = true;
}

bool installed() { return g_installed; }

void connect(Protocol* proto, const std::string_view host, const uint16_t port, const bool gameWorld)
{
    if (!g_installed || !g_hooks.connect)
        return;
    const std::string copy(host);
    g_hooks.connect(proto, copy.c_str(), port, gameWorld);
}

void disconnect(Protocol* proto)
{
    if (g_installed && g_hooks.disconnect)
        g_hooks.disconnect(proto);
}

bool isConnected(Protocol* proto)
{
    return g_installed && g_hooks.isConnected && g_hooks.isConnected(proto);
}

bool isConnecting(Protocol* proto)
{
    return g_installed && g_hooks.isConnecting && g_hooks.isConnecting(proto);
}

void write(Protocol* proto, const uint8_t* data, const uint16_t size, const bool raw)
{
    if (g_installed && g_hooks.write)
        g_hooks.write(proto, data, size, raw);
}

void generateXtea(Protocol* proto, uint32_t key[4])
{
    if (g_installed && g_hooks.generateXtea)
        g_hooks.generateXtea(proto, key);
}

void setXtea(Protocol* proto, const uint32_t key[4])
{
    if (g_installed && g_hooks.setXtea)
        g_hooks.setXtea(proto, key);
}

void enableXtea(Protocol* proto)
{
    if (g_installed && g_hooks.enableXtea)
        g_hooks.enableXtea(proto);
}

void enableChecksum(Protocol* proto)
{
    if (g_installed && g_hooks.enableChecksum)
        g_hooks.enableChecksum(proto);
}

void enableSequenced(Protocol* proto)
{
    if (g_installed && g_hooks.enableSequenced)
        g_hooks.enableSequenced(proto);
}

void setRecordFile(Protocol* proto, const std::string_view file)
{
    if (!g_installed || !g_hooks.setRecordFile)
        return;
    const std::string copy(file);
    g_hooks.setRecordFile(proto, copy.c_str());
}

int64_t millisSinceRead(Protocol* proto)
{
    if (!g_installed || !g_hooks.millisSinceRead)
        return -1;
    return g_hooks.millisSinceRead(proto);
}

}
