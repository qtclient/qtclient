#include <qtclient/const.h>
#include <qtclient/host.h>
#include <qtclient/mod_api_v1.h>
#include <qtclient/mod_entry.h>
#include <qtclient/session.h>

#include <QDebug>

namespace {

void enableOtcrWireFeatures()
{
    // Crystal only appends item/creature shader + wings tails when isOTCR
    // (CLIENTOS_OTCR_*). Stock tip stays on CipSoft NEW_* OS + CIP wire;
    // this default pack opts into OTCR after qtclient enableFeatures(tip).
    using namespace tibia::client;
    using qtclient::session::enableFeature;
    using qtclient::session::setCustomOs;
    const std::string hostOs = tibia::host::app().getOs();
    if (hostOs == "windows")
        setCustomOs(CLIENTOS_OTCR_WINDOWS);
    else if (hostOs == "mac")
        setCustomOs(CLIENTOS_OTCR_MAC);
    else
        setCustomOs(CLIENTOS_OTCR_LINUX);

    enableFeature(GameItemShader);
    enableFeature(GameCreatureShader);
    enableFeature(GameCreatureAttachedEffect);
    enableFeature(GameWingsAurasEffectsShader);
}

void earlyBootstrapOtclient(const tibia::mods::ModContext& ctx)
{
    Q_UNUSED(ctx);
    if (const auto* api = qtclient_mod_host_api_v1(); api && api->setFeatureExtrasHook)
        api->setFeatureExtrasHook(&enableOtcrWireFeatures);
    enableOtcrWireFeatures();
}

void registerOtclient(const qtclient::ModApiV1* api)
{
    if (!api || !api->registerEarlyBootstrap) {
        qWarning("otclient mod: host API v1 missing");
        return;
    }
    api->registerEarlyBootstrap(&earlyBootstrapOtclient);
}

} // namespace

void qtclient_mod_otclient_register(const tibia::mods::ModContext* earlyCtx)
{
    Q_UNUSED(earlyCtx);
    registerOtclient(qtclient_mod_host_api_v1());
}
