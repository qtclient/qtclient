// Host API v9: the game host table (abi/gamehost_api.h) over the game client's
// runtime::TGameHost (its lifecycle signals and character switching) and the host's HD flag.
#include "mods/abi_internal.h"

#include "client/gameclient.h"
#include "runtime/gamehost.h"
#include "runtime/host.h"

#include <QJsonArray>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace tibia::mods {
namespace {

runtime::TGameHost* gameHost()
{
    const HostObjects* host = hostObjects();
    return host && host->gameClient ? host->gameClient->gameHost() : nullptr;
}

struct TGameHostWatch
{
    Subscribers<std::function<void(int, const QString&)>> listeners;
    QPointer<runtime::TGameHost> bound;
    std::vector<QMetaObject::Connection> connections;
};

TGameHostWatch& watch()
{
    static TGameHostWatch w;
    return w;
}

void publish(const int event, const QString& arg)
{
    watch().listeners.call(event, arg);
}

/// Connects the game host's signals once (the game client and its game host live as long
/// as the application).
void bindGameHost()
{
    TGameHostWatch& w = watch();
    runtime::TGameHost* host = gameHost();
    if (!host || w.bound == host)
        return;
    for (const QMetaObject::Connection& c : w.connections)
        QObject::disconnect(c);
    w.connections.clear();
    w.bound = host;
    using H = runtime::TGameHost;
    w.connections.push_back(QObject::connect(host, &H::mainWindowInitialized, host,
                                             [] { publish(QTCLIENT_GAMEHOST_MAIN_WINDOW_INITIALIZED, {}); }));
    w.connections.push_back(QObject::connect(host, &H::worldLeft, host, [] { publish(QTCLIENT_GAMEHOST_WORLD_LEFT, {}); }));
    w.connections.push_back(QObject::connect(host, &H::characterSelectionRequested, host,
                                             [] { publish(QTCLIENT_GAMEHOST_CHARACTER_SELECTION_REQUESTED, {}); }));
    w.connections.push_back(QObject::connect(host, &H::characterSelectionClosed, host,
                                             [] { publish(QTCLIENT_GAMEHOST_CHARACTER_SELECTION_CLOSED, {}); }));
    w.connections.push_back(QObject::connect(host, &H::characterSwitchRequested, host, [](const QString& name) {
        publish(QTCLIENT_GAMEHOST_CHARACTER_SWITCH_REQUESTED, name);
    }));
}

int ghSubscribe(int event, QtClientGameHostEventFn fn, void* user)
{
    if (!fn)
        return 0;
    bindGameHost();
    return watch().listeners.add([event, fn, user](const int e, const QString& arg) {
        if (event != 0 && event != e)
            return;
        const QByteArray utf8 = arg.toUtf8();
        fn(user, e, utf8.constData());
    });
}

void ghUnsubscribe(int id)
{
    watch().listeners.remove(id);
}

int ghCharacterNamesJson(char* buf, size_t buf_len)
{
    runtime::TGameHost* host = gameHost();
    if (!host)
        return writeSizedOut(QByteArray("[]"), buf, buf_len);
    return writeSizedOut(compactJson(QJsonArray::fromStringList(host->characterNames())), buf, buf_len);
}

int ghRequestCharacterSwitch(const char* name_utf8)
{
    runtime::TGameHost* host = gameHost();
    if (!host || !name_utf8 || !*name_utf8)
        return 0;
    return host->requestCharacterSwitch(QString::fromUtf8(name_utf8)) ? 1 : 0;
}

void ghToggleCharacterSelection()
{
    if (runtime::TGameHost* host = gameHost())
        host->toggleCharacterSelectionForSwitch();
}

int ghWorldViewActive()
{
    runtime::TGameHost* host = gameHost();
    return host && host->worldMapRenderProvider() ? 1 : 0;
}

int ghHdMode()
{
    return tibia::host::app().isHdMode() ? 1 : 0;
}

void ghSetHdMode(int on)
{
    tibia::host::app().setHdMode(on != 0);
}

const QtClientGameHostApiV1 g_gameHostApi = {
    .struct_size = sizeof(QtClientGameHostApiV1),
    .subscribe = &ghSubscribe,
    .unsubscribe = &ghUnsubscribe,
    .character_names_json = &ghCharacterNamesJson,
    .request_character_switch = &ghRequestCharacterSwitch,
    .toggle_character_selection_for_switch = &ghToggleCharacterSelection,
    .world_view_active = &ghWorldViewActive,
    .hd_mode = &ghHdMode,
    .set_hd_mode = &ghSetHdMode,
};

} // namespace

/// Called from bindUiWatchersToHost too: a game window means the client exists.
void bindGameHostWatcher()
{
    bindGameHost();
}

const QtClientGameHostApiV1* gameHostApiV1()
{
    return &g_gameHostApi;
}

} // namespace tibia::mods
