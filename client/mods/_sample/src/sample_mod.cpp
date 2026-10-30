#include "qtclient/mod_api_v1.h"

#include "mods/mod_entry.h"

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
    api->registerMod({
        .identity =
            {
                .id = "sample",
                .displayName = "Sample",
                .organization = "qtclient",
                .organizationDomain = "example.invalid",
                .bundleId = "com.qtclient.sample",
                .cdnBaseUrl = "",
                .priority = 50,
            },
        .bootstrap = &bootstrapSample,
    });
}

} // namespace

QTCLIENT_MOD_EXPORT void qtclient_mod_entry(const tibia::mods::ModContext* earlyCtx)
{
    Q_UNUSED(earlyCtx);
    // earlyCtx is null on first load — register only; bootstrapAll runs later.
    registerSample(qtclient_mod_host_api_v1());
}
