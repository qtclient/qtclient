#include <qtclient/mod_api_v1.h>
#include <qtclient/mod_entry.h>
// MSVC absorb canary: session → game hubs need pack_msvc_ns_anchor (C3083).
#include <qtclient/session.h>

#include <QDebug>

namespace {

void bootstrapSample(const tibia::mods::ModContext& ctx)
{
    Q_UNUSED(ctx);
    qInfo("sample mod: bootstrap ok (mod_api_v1)");
}

void registerSample(const qtclient::ModApiV1* api)
{
    if (!api || !api->registerMod) {
        qWarning("sample mod: host API v1 missing");
        return;
    }
    tibia::mods::ModEntry entry;
    entry.identity.id = "sample";
    entry.identity.displayName = "Sample";
    entry.identity.organization = "qtclient";
    entry.identity.organizationDomain = "example.invalid";
    entry.identity.bundleId = "com.qtclient.sample";
    entry.identity.cdnBaseUrl = "";
    entry.identity.priority = 50;
    entry.bootstrap = &bootstrapSample;
    api->registerMod(entry);
}

} // namespace

void qtclient_mod_sample_register(const tibia::mods::ModContext* earlyCtx)
{
    Q_UNUSED(earlyCtx);
    // earlyCtx is null on first load — register only; bootstrapAll runs later.
    registerSample(qtclient_mod_host_api_v1());
}
