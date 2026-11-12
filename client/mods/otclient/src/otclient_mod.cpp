#include <qtclient/abi/const.h>
#include <qtclient/abi/host_api_v1.h>
#include <qtclient/abi/mod_register.h>

#include <cstdio>
#include <cstring>

namespace tibia {
namespace mods {
struct ModContext;
}
}

extern "C" const QtClientHostApiV1* qtclient_host_api_v1(void);

namespace {

const QtClientHostApiV1* g_api = nullptr;

void enableOtcrWireFeatures()
{
    if (!g_api || !g_api->host_os || !g_api->set_custom_os || !g_api->enable_feature)
        return;

    char os[32] = {};
    g_api->host_os(os, sizeof(os));
    if (std::strcmp(os, "windows") == 0)
        g_api->set_custom_os(QTCLIENT_OS_OTCR_WINDOWS);
    else if (std::strcmp(os, "mac") == 0)
        g_api->set_custom_os(QTCLIENT_OS_OTCR_MAC);
    else
        g_api->set_custom_os(QTCLIENT_OS_OTCR_LINUX);

    g_api->enable_feature(QTCLIENT_FEATURE_ITEM_SHADER);
    g_api->enable_feature(QTCLIENT_FEATURE_CREATURE_SHADER);
    g_api->enable_feature(QTCLIENT_FEATURE_CREATURE_ATTACHED_EFFECT);
    g_api->enable_feature(QTCLIENT_FEATURE_WINGS_AURAS_EFFECTS_SHADER);
}

void earlyBootstrapOtclient(const QtClientModLoadCtx* ctx)
{
    (void)ctx;
    if (g_api && g_api->set_feature_extras_hook)
        g_api->set_feature_extras_hook(&enableOtcrWireFeatures);
    enableOtcrWireFeatures();
}

void registerOtclient(const QtClientHostApiV1* api)
{
    if (!api || !api->register_early_bootstrap) {
        std::fprintf(stderr, "otclient mod: host API v1 missing\n");
        return;
    }
    g_api = api;
    api->register_early_bootstrap(&earlyBootstrapOtclient);
}

} // namespace

#ifndef QTCLIENT_ABSORB_INTO_QTCLIENT
QTCLIENT_MOD_EXPORT void qtclient_mod_register(const QtClientHostApiV1* api,
                                               const QtClientModLoadCtx* ctx)
{
    (void)ctx;
    registerOtclient(api);
}
#endif

// Deprecated absorb-into-tip entry (GenerateLinkedMods). Prefer qtclient_mod_register.
void qtclient_mod_otclient_register(const tibia::mods::ModContext* earlyCtx)
{
    (void)earlyCtx;
    registerOtclient(qtclient_host_api_v1());
}
