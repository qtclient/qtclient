// Host API v9: the game window context a pack bootstrap gets (abi/context.h), and the
// host-internal store of the game window's CipSoft objects the ABI tables read.
#include "mods/abi_internal.h"
#include "mods/registry.h"

#include "app/launcherenv.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QQmlEngine>
#include <QQuickItem>

namespace tibia::mods {

namespace {

HostObjects g_objects;
bool g_hasObjects = false;

struct ContextStore
{
    QtClientGameContextV1 ctx{};
    QByteArray loginBase;
    QByteArray assetBase;
    QByteArray paymentsBase;
    QByteArray revision;
};

ContextStore& contextStore()
{
    static ContextStore store;
    return store;
}

} // namespace

void setHostObjects(const HostObjects* objects)
{
    if (!objects)
        releaseUiOnHostTeardown();
    g_hasObjects = objects != nullptr;
    g_objects = objects ? *objects : HostObjects{};
    bindBackendsToHost();
    bindDialogWatchersToHost();
    bindUiWatchersToHost();
}

const HostObjects* hostObjects()
{
    return g_hasObjects ? &g_objects : nullptr;
}

const QtClientGameContextV1* gameContextFor(const ModContext& mod)
{
    ContextStore& store = contextStore();
    const auto* env = static_cast<const app::LauncherEnv*>(mod.launcherEnv);
    store.loginBase = env ? env->loginBase.toString().toUtf8() : QByteArray();
    store.assetBase = env ? env->assetBase.toString().toUtf8() : QByteArray();
    store.paymentsBase = env ? env->paymentsBase.toString().toUtf8() : QByteArray();
    store.revision = env ? env->revision.toUtf8() : QByteArray();
    QtClientGameContextV1& ctx = store.ctx;
    ctx = QtClientGameContextV1{};
    ctx.struct_size = sizeof(QtClientGameContextV1);
    ctx.qml_engine = mod.engine;
    ctx.root_item = mod.rootItem;
    ctx.game_window_item = mod.gameWindowItem;
    ctx.map_window_pane = mod.mapWindowPane;
    ctx.lifetime_parent = mod.host;
    ctx.login_base = store.loginBase.constData();
    ctx.asset_base = store.assetBase.constData();
    ctx.payments_base = store.paymentsBase.constData();
    ctx.client_revision = store.revision.constData();
    ctx.from_launcher = env && env->fromLauncher ? 1 : 0;
    return &ctx;
}

} // namespace tibia::mods
