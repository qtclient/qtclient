// Host API v9: the action table (abi/action_api.h). Game actions by CipSoft's EGameAction
// names (the QMetaEnum --smoke-action reads, app/smokeactions.cpp) published through the
// game client's router and seen first through the router's pre-dispatch hook; context menu
// extensions and custom menus on CipSoft's TContextMenuController; CipSoft's crosshair
// target selection (input::startTargetSelection) and target cursor.
#include "mods/abi_internal.h"

#include "game/currentgamesession.h"
#include "game/gameactionrouter.h"
#include "game/playerdata.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"

#include <QJsonObject>
#include <QMetaEnum>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <vector>

/// A context menu being built (abi/action_api.h).
struct QtClientContextMenu
{
    QPointer<tibia::gamewindow::TContextMenuController> menu;
    struct TEntry
    {
        void (*closed)(void*) = nullptr;
        void* user = nullptr;
        bool done = false;
    };
    /// Shared with the menu's close connection: the `closed` of every entry.
    std::shared_ptr<std::vector<TEntry>> entries = std::make_shared<std::vector<TEntry>>();
};

namespace tibia::mods {
namespace {

using input::EGameAction;
using input::IGameActionPtr;

// --- names and payloads ---

bool actionOfName(const char* name, EGameAction& out)
{
    if (!name || !*name)
        return false;
    bool known = false;
    const int value = QMetaEnum::fromType<EGameAction>().keyToValue(QByteArray(name).trimmed().toUpper().constData(), &known);
    if (!known || value == int(EGameAction::UNKNOWN))
        return false;
    out = static_cast<EGameAction>(value);
    return true;
}

QByteArray nameOfAction(const EGameAction type)
{
    return QByteArray(QMetaEnum::fromType<EGameAction>().valueToKey(int(type)));
}

/// The CipSoft action of `type` with the documented payload keys; null when it has no
/// action class without fields and is not one of them.
IGameActionPtr makeAction(const EGameAction type, const QJsonObject& p)
{
    switch (type) {
    case EGameAction::OPEN_CYCLOPEDIA: {
        auto action = std::make_shared<input::TGameActionOpenCyclopedia>(p.value("tab").toInt());
        if (p.contains("raceId"))
            action->setRaceId(static_cast<uint16_t>(p.value("raceId").toInt()));
        return action;
    }
    case EGameAction::OPEN_TASKBOARD_DIALOG:
        return std::make_shared<input::gameactions::TGameActionOpenTaskboardDialog>(p.value("tab").toInt());
    case EGameAction::OPEN_SOCIAL_DIALOG:
        return std::make_shared<input::TGameActionOpenSocialDialog>(p.value("tab").toInt());
    case EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT:
        return std::make_shared<input::TGameActionFriendsDialogSearchAccount>(p.value("characterName").toString());
    case EGameAction::SET_STATUS_LINE:
        return std::make_shared<input::TGameActionSetStatusLine>(p.value("text").toString());
    case EGameAction::START_TALKING_TO_NPC:
        return input::makeCreatureAction(type, static_cast<uint32_t>(p.value("creatureId").toInteger()));
    case EGameAction::SEND_CHAT_MESSAGE_TO_NPC: {
        QString name = p.value("name").toString();
        int level = p.contains("level") ? p.value("level").toInt() : -1;
        if (const game::TPlayerData* data = game::currentPlayerData()) {
            if (name.isEmpty())
                name = QString::fromStdString(data->characterName());
            if (level < 0)
                level = static_cast<int>(data->level());
        }
        return std::make_shared<input::TGameActionSendChatMessageToNpc>(p.value("text").toString(), name,
                                                                         static_cast<uint16_t>(std::max(0, level)));
    }
    default:
        return input::makeGameAction(type);
    }
}

QJsonObject payloadOf(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case EGameAction::OPEN_CYCLOPEDIA:
        if (const auto* a = dynamic_cast<const input::TGameActionOpenCyclopedia*>(&action)) {
            QJsonObject o{ { "tab", a->tab() } };
            if (a->raceId() != 0)
                o.insert("raceId", a->raceId());
            return o;
        }
        break;
    case EGameAction::OPEN_TASKBOARD_DIALOG:
        if (const auto* a = dynamic_cast<const input::gameactions::TGameActionOpenTaskboardDialog*>(&action))
            return { { "tab", a->tab() } };
        break;
    case EGameAction::OPEN_SOCIAL_DIALOG:
        if (const auto* a = dynamic_cast<const input::TGameActionOpenSocialDialog*>(&action))
            return { { "tab", a->tab() } };
        break;
    case EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT:
        if (const auto* a = dynamic_cast<const input::TGameActionFriendsDialogSearchAccount*>(&action))
            return { { "characterName", a->characterName() } };
        break;
    case EGameAction::SET_STATUS_LINE:
        if (const auto* a = dynamic_cast<const input::TGameActionSetStatusLine*>(&action))
            return { { "text", a->text() } };
        break;
    case EGameAction::START_TALKING_TO_NPC:
        if (const auto* a = dynamic_cast<const input::TGameActionStartTalkingToNPC*>(&action))
            return { { "creatureId", static_cast<qint64>(a->creatureId()) } };
        if (const auto* a = dynamic_cast<const input::TGameActionCreature*>(&action))
            return { { "creatureId", static_cast<qint64>(a->creatureId()) } };
        break;
    case EGameAction::SEND_CHAT_MESSAGE_TO_NPC:
        if (const auto* a = dynamic_cast<const input::TGameActionSendChatMessageToNpc*>(&action))
            return { { "text", a->text() }, { "name", a->speakerName() }, { "level", a->speakerLevel() } };
        break;
    default:
        break;
    }
    return {};
}

// --- publish / subscribe ---

struct TActionWatch
{
    Subscribers<std::function<bool(EGameAction, const QByteArray&, const QByteArray&)>> listeners;
    bool hooked = false;
};

TActionWatch& actionWatch()
{
    static TActionWatch w;
    return w;
}

bool preDispatch(const IGameActionPtr& action)
{
    TActionWatch& w = actionWatch();
    if (w.listeners.empty())
        return false;
    const EGameAction type = action->gameActionType();
    const QByteArray name = nameOfAction(type);
    QByteArray payload;
    return w.listeners.callUntil(
        [&](auto& entry) {
            if (payload.isEmpty())
                payload = QJsonDocument(payloadOf(*action)).toJson(QJsonDocument::Compact);
            return entry.fn(type, name, payload);
        });
}

int actPublish(const char* name, const char* payload_json)
{
    EGameAction type{};
    if (!actionOfName(name, type))
        return -1;
    QJsonValue payload;
    if (!parseJsonValue(payload_json, payload) || !(payload.isObject() || payload.isNull()))
        return -1;
    const HostObjects* host = hostObjects();
    if (!host || !host->router)
        return -2;
    const IGameActionPtr action = makeAction(type, payload.toObject());
    if (!action)
        return -1;
    host->router->handleGameActionInternal(action);
    return 0;
}

int actSubscribe(const char* name, QtClientGameActionFn fn, void* user)
{
    if (!fn)
        return 0;
    EGameAction only{};
    const bool all = !name || !*name;
    if (!all && !actionOfName(name, only))
        return 0;
    TActionWatch& w = actionWatch();
    if (!w.hooked) {
        w.hooked = true;
        game::TInternalGameActionRouter::setPreDispatchHook(&preDispatch);
    }
    return w.listeners.add([all, only, fn, user](const EGameAction type, const QByteArray& n, const QByteArray& payload) {
        if (!all && type != only)
            return false;
        return fn(user, n.constData(), payload.constData()) != 0;
    });
}

void actUnsubscribe(int id)
{
    actionWatch().listeners.remove(id);
}

// --- object contexts ---

QtClientObjectContext contextOf(const input::TObjectGameActionContext& c)
{
    QtClientObjectContext out{};
    switch (c.locationType()) {
    case input::EGameActionContextType::LOCATION_INVENTORY:
        out.location = QTCLIENT_LOCATION_INVENTORY;
        break;
    case input::EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT:
        out.location = QTCLIENT_LOCATION_CONTAINER;
        break;
    case input::EGameActionContextType::LOCATION_WORLD_MAP:
    case input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE:
    case input::EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
        out.location = QTCLIENT_LOCATION_MAP;
        break;
    default:
        out.location = QTCLIENT_LOCATION_UNKNOWN;
        break;
    }
    const Position p = c.position();
    out.pos = QtClientMapPos{ p.x, p.y, p.z };
    out.stack_pos = c.stackPosition();
    out.type_id = c.objectClientId();
    out.creature_id = c.creatureId();
    return out;
}

// --- context menus ---

void addEntry(QtClientContextMenu* menu, const char* label, int flags, void (*triggered)(void*), void (*closed)(void*),
              void* user)
{
    if (!menu || !menu->menu)
        return;
    gamewindow::TContextMenuEntryItem* entry = menu->menu->addCustomEntry(QString::fromUtf8(label ? label : ""));
    entry->setCheckable((flags & QTCLIENT_MENU_ENTRY_CHECKABLE) != 0);
    entry->setChecked((flags & QTCLIENT_MENU_ENTRY_CHECKED) != 0);
    const bool disabled = (flags & QTCLIENT_MENU_ENTRY_DISABLED) != 0;
    if (triggered && !disabled)
        QObject::connect(entry, &gamewindow::TContextMenuEntryItem::triggered, entry, [triggered, user] { triggered(user); });
    if (!closed)
        return;
    auto entries = menu->entries;
    entries->push_back({ closed, user, false });
    const size_t index = entries->size() - 1;
    // The menu clears its entries when it is used again: an entry's `closed` runs at the
    // latest then.
    QObject::connect(entry, &QObject::destroyed, [entries, index] {
        auto& e = (*entries)[index];
        if (!e.done) {
            e.done = true;
            e.closed(e.user);
        }
    });
}

/// After the menu closed (queued: an entry's `triggered` runs first), every `closed`.
void connectClosed(QtClientContextMenu* menu)
{
    if (!menu || !menu->menu || menu->entries->empty())
        return;
    auto entries = menu->entries;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(
        menu->menu.data(), &cip::tibia::gamewindow::TContextMenuController::contextMenuClosed, menu->menu.data(),
        [entries, connection] {
            QObject::disconnect(*connection);
            for (auto& e : *entries) {
                if (!e.done) {
                    e.done = true;
                    e.closed(e.user);
                }
            }
        },
        Qt::QueuedConnection);
}

struct TMenuExtension
{
    QtClientContextMenuExtensionFn fn = nullptr;
    void* user = nullptr;
    /// Deleting it drops the extension from the controller (dead owners are skipped).
    std::unique_ptr<QObject> owner;
    QPointer<gamewindow::TContextMenuController> attachedTo;
};

std::map<int, TMenuExtension>& menuExtensions()
{
    static std::map<int, TMenuExtension> extensions;
    return extensions;
}

/// Adds the extension to the process-wide context menu (made with the game window; a new
/// one gets the extensions again).
void attach(TMenuExtension& extension)
{
    gamewindow::TContextMenuController* controller = gamewindow::TContextMenuController::instance();
    if (!controller || extension.attachedTo == controller)
        return;
    extension.owner = std::make_unique<QObject>();
    extension.attachedTo = controller;
    const QtClientContextMenuExtensionFn fn = extension.fn;
    void* user = extension.user;
    controller->addGameActionsExtension(
        extension.owner.get(), [fn, user](const input::TGameActionList& actions, int, gamewindow::TContextMenuController& menu) {
            std::vector<QtClientObjectContext> objects;
            objects.reserve(actions.size());
            for (const IGameActionPtr& action : actions) {
                if (!action)
                    continue;
                if (const std::optional<input::TObjectGameActionContext> c = input::objectContextOf(*action))
                    objects.push_back(contextOf(*c));
            }
            QtClientContextMenu handle;
            handle.menu = &menu;
            // CipSoft builds game-action menus on a right click.
            fn(user, objects.data(), objects.size(), static_cast<int>(Qt::RightButton), &handle);
            connectClosed(&handle);
        });
}

int actAddContextMenuExtension(QtClientContextMenuExtensionFn fn, void* user)
{
    if (!fn)
        return 0;
    static int nextId = 1;
    const int id = nextId++;
    TMenuExtension& extension = menuExtensions()[id];
    extension.fn = fn;
    extension.user = user;
    attach(extension);
    return id;
}

void actRemoveContextMenuExtension(int id)
{
    menuExtensions().erase(id);
}

QtClientContextMenu* actBeginCustomMenu()
{
    gamewindow::TContextMenuController* controller = gamewindow::TContextMenuController::instance();
    if (!controller || !hostObjects())
        return nullptr;
    controller->beginCustomMenu();
    auto* menu = new QtClientContextMenu;
    menu->menu = controller;
    return menu;
}

void actShowCustomMenu(QtClientContextMenu* menu)
{
    if (!menu)
        return;
    if (menu->menu) {
        connectClosed(menu);
        menu->menu->showCustomMenu();
    }
    delete menu;
}

void actMenuAddEntry(QtClientContextMenu* menu, const char* label_utf8, int flags, void (*triggered)(void* user),
                     void (*closed)(void* user), void* user)
{
    addEntry(menu, label_utf8, flags, triggered, closed, user);
}

void actMenuAddSeparator(QtClientContextMenu* menu)
{
    if (menu && menu->menu)
        menu->menu->addCustomSeparator();
}

// --- target selection ---

void setTargetCursor(const bool on)
{
    if (gamewindow::TMouseCursorController* cursor = gamewindow::TMouseCursorController::instance())
        cursor->setTargetSelection(on);
}

void actStartTargetSelection(QtClientTargetSelectedFn fn, void* user, int set_cursor)
{
    const bool cursor = set_cursor != 0;
    if (cursor)
        setTargetCursor(true);
    input::startTargetSelection([fn, user, cursor](const input::TTargetSelectedPtr& target) {
        if (cursor)
            setTargetCursor(false);
        if (!fn)
            return;
        if (!target) {
            fn(user, nullptr);
            return;
        }
        const QtClientObjectContext context = contextOf(target->context());
        fn(user, &context);
    });
}

void actCancelTargetSelection()
{
    input::finishTargetSelection(nullptr);
}

int actIsSelectingTarget()
{
    return input::isSelectingTarget() ? 1 : 0;
}

const QtClientActionApiV1 g_actionApi = {
    .struct_size = sizeof(QtClientActionApiV1),
    .publish = &actPublish,
    .subscribe = &actSubscribe,
    .unsubscribe = &actUnsubscribe,
    .add_context_menu_extension = &actAddContextMenuExtension,
    .remove_context_menu_extension = &actRemoveContextMenuExtension,
    .begin_custom_menu = &actBeginCustomMenu,
    .show_custom_menu = &actShowCustomMenu,
    .context_menu_add_entry = &actMenuAddEntry,
    .context_menu_add_separator = &actMenuAddSeparator,
    .start_target_selection = &actStartTargetSelection,
    .cancel_target_selection = &actCancelTargetSelection,
    .is_selecting_target = &actIsSelectingTarget,
};

} // namespace

void attachMenuExtensions()
{
    for (auto& [id, extension] : menuExtensions())
        attach(extension);
}

const QtClientActionApiV1* actionApiV1()
{
    return &g_actionApi;
}

} // namespace tibia::mods
