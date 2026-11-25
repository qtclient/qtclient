#pragma once

// Host-internal plumbing shared by the pack ABI tables (host API v9): the game window's
// CipSoft objects the tables reach, the sized-out string rule, subscriber lists and the
// table getters the root table (host_api_v1.cpp) hangs off QtClientHostApiV1.
// Nothing here is pack-facing: packs see only include/qtclient/abi/.

#include "qtclient/abi/host_api_v1.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QPointer>
#include <QString>

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class QObject;
class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace app {
struct LauncherEnv;
class TSessionBootstrap;
}
namespace client {
class TGameClient;
}
namespace game {
class TInternalGameActionRouter;
}
namespace gamewindow {
class TBlessingsDialogController;
class TBossDifficultySelectionDialogController;
class TChatController;
class TContainerStorageController;
class TCyclopediaDialogController;
class TDialog;
class TDialogStackController;
class TImbuingDialogController;
class TMapWindowController;
class TMinimapController;
class TNpcDialogController;
class TOptionsMenuDialogController;
class TQuestLogController;
class TSidebarManager;
class TSidebarPanelsManager;
class TSidebarWidgetsManager;
class TSocialDialogController;
class TStoreDialogController;
}
namespace mods {
struct ModContext;

/// The CipSoft objects of the game window the ABI tables use (filled by
/// TGameClient::createGameWindow, cleared when the game window goes). Every pointer may
/// be null; read them at call time.
struct HostObjects
{
    QQmlEngine* engine = nullptr;
    QQuickItem* rootItem = nullptr;
    QQuickItem* gameWindowItem = nullptr;
    QQuickItem* mapWindowPane = nullptr;
    QQuickItem* dialogPlaceholder = nullptr;
    /// Lives as long as the game window (the packs' long-lived objects' parent).
    QObject* lifetimeParent = nullptr;
    const app::LauncherEnv* launcherEnv = nullptr;

    client::TGameClient* gameClient = nullptr;
    game::TInternalGameActionRouter* router = nullptr;
    gamewindow::TDialogStackController* dialogStack = nullptr;
    app::TSessionBootstrap* sessionBootstrap = nullptr;

    gamewindow::TSidebarManager* sidebarManager = nullptr;
    gamewindow::TSidebarWidgetsManager* sidebarWidgets = nullptr;
    gamewindow::TSidebarPanelsManager* sidebarPanels = nullptr;
    gamewindow::TContainerStorageController* containers = nullptr;
    gamewindow::TMinimapController* minimapController = nullptr;
    gamewindow::TMapWindowController* mapWindowController = nullptr;
    gamewindow::TChatController* chat = nullptr;
    gamewindow::TNpcDialogController* npcDialog = nullptr;
    gamewindow::TBlessingsDialogController* blessingsDialog = nullptr;
    gamewindow::TImbuingDialogController* imbuingDialog = nullptr;
    gamewindow::TOptionsMenuDialogController* optionsDialog = nullptr;
    gamewindow::TStoreDialogController* storeDialog = nullptr;
    gamewindow::TQuestLogController* questLog = nullptr;
    gamewindow::TCyclopediaDialogController* cyclopediaDialog = nullptr;
    gamewindow::TSocialDialogController* socialDialog = nullptr;
    gamewindow::TBossDifficultySelectionDialogController* bossDifficultyDialog = nullptr;

    /// The host dialogs' TDialog by QTCLIENT_HOST_DIALOG_* kind (the create character
    /// controller can be replaced, so they are asked at call time).
    std::function<gamewindow::TDialog*(int kind)> hostDialog;
    /// The button bar's logout (CipSoft's confirmation), else the start screen.
    std::function<void()> requestLogoutViaButtonBar;
};

/// Sets / clears (null) the game window's objects; the ABI re-binds the backends and the
/// host dialog watchers to them.
void setHostObjects(const HostObjects* objects);
/// The current game window's objects; null without a game window.
const HostObjects* hostObjects();
/// Called by setHostObjects (backend_adapters.cpp / dialog_abi.cpp / ui_abi.cpp).
void bindBackendsToHost();
void bindDialogWatchersToHost();
void bindUiWatchersToHost();
void releaseUiOnHostTeardown();
void bindGameHostWatcher();
void attachMenuExtensions();
void bindChatWatchers();
void bindPackHostToHost();

/// The game window context of a bootstrap (context_abi.cpp). Valid until the next call.
const QtClientGameContextV1* gameContextFor(const ModContext& ctx);

/// The ABI tables (each in its <domain>_abi.cpp).
const QtClientSessionApiV1* sessionApiV1();
const QtClientNotifyApiV1* notifyApiV1();
const QtClientOptionsApiV1* optionsApiV1();
const QtClientPackHostApiV1* packHostAbiTable();
const QtClientGameHostApiV1* gameHostApiV1();
const QtClientOpcodeApiV1* opcodeApiV1();
const QtClientDialogApiV1* dialogApiV1();
const QtClientActionApiV1* actionApiV1();
const QtClientChatApiV1* chatApiV1();
const QtClientCooldownApiV1* cooldownApiV1();
const QtClientGameDataApiV1* gameDataApiV1();
const QtClientUiApiV1* uiApiV1();
const QtClientBackendApiV1* backendApiV1();
const QtClientCipObjectApiV1* cipObjectApiV1();

/// The sized-out rule (types.h): writes at most buf_len - 1 bytes plus a NUL, returns the
/// full length.
inline int writeSizedOut(const QByteArray& bytes, char* buf, size_t buf_len)
{
    if (buf && buf_len > 0) {
        const size_t n = std::min(static_cast<size_t>(bytes.size()), buf_len - 1);
        std::memcpy(buf, bytes.constData(), n);
        buf[n] = '\0';
    }
    return static_cast<int>(bytes.size());
}
inline int writeSizedOut(const QString& text, char* buf, size_t buf_len)
{
    return writeSizedOut(text.toUtf8(), buf, buf_len);
}
/// A JSON value as compact text (objects / arrays / scalars).
inline QByteArray compactJson(const QJsonValue& value)
{
    if (value.isObject())
        return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    if (value.isArray())
        return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
    // A scalar: as the single element of an array, without the brackets.
    QByteArray text = QJsonDocument(QJsonArray{ value }).toJson(QJsonDocument::Compact);
    return text.mid(1, text.size() - 2);
}
/// Parses a JSON value (object, array or scalar); false for invalid text. Null / "" text
/// reads as an empty object when `emptyIsObject`.
inline bool parseJsonValue(const char* text, QJsonValue& out, const bool emptyIsObject = true)
{
    const QByteArray bytes = text ? QByteArray(text).trimmed() : QByteArray();
    if (bytes.isEmpty()) {
        out = emptyIsObject ? QJsonValue(QJsonObject()) : QJsonValue();
        return true;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray("[") + bytes + "]", &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size() != 1)
        return false;
    out = doc.array().at(0);
    return true;
}
/// "v1..v8" bytes-written rule: writes as much as fits and returns the bytes written.
inline int writeBytesWritten(const QByteArray& bytes, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    const size_t n = std::min(static_cast<size_t>(bytes.size()), buf_len - 1);
    std::memcpy(buf, bytes.constData(), n);
    buf[n] = '\0';
    return static_cast<int>(n);
}
inline void copyUtf8(char* out, size_t len, const QString& text)
{
    if (!out || len == 0)
        return;
    const QByteArray utf8 = text.toUtf8();
    const size_t n = std::min(static_cast<size_t>(utf8.size()), len - 1);
    std::memcpy(out, utf8.constData(), n);
    out[n] = '\0';
}

/// Subscriber list with ids > 0. Unsubscribing (also inside a callback) only marks the
/// entry; a callback never runs after its unsubscribe returned.
template <class Fn>
class Subscribers
{
public:
    int add(Fn fn)
    {
        const int id = m_nextId++;
        m_entries.push_back(std::make_shared<Entry>(Entry{ id, std::move(fn), true }));
        return id;
    }
    bool remove(int id)
    {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if ((*it)->id == id) {
                (*it)->alive = false;
                m_entries.erase(it);
                return true;
            }
        }
        return false;
    }
    bool empty() const { return m_entries.empty(); }
    template <class... Args>
    void call(Args&&... args)
    {
        const auto snapshot = m_entries;
        for (const auto& e : snapshot) {
            if (e->alive)
                e->fn(args...);
        }
    }
    /// Runs entries until one returns true (the action router's take-over rule).
    template <class Pred, class... Args>
    bool callUntil(Pred&& accept, Args&&... args)
    {
        const auto snapshot = m_entries;
        for (const auto& e : snapshot) {
            if (e->alive && accept(*e, args...))
                return true;
        }
        return false;
    }

    struct Entry
    {
        int id;
        Fn fn;
        bool alive;
    };

private:
    int m_nextId = 1;
    std::vector<std::shared_ptr<Entry>> m_entries;
};

} // namespace mods
} // namespace tibia
