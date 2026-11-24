#include "platform/platform.h"
#include <optional>
#include "gameactionhandlers.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "appearances/appearanceinstance.h"
#include "container/containerciptypes.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <creatures/creaturestorage.h>
#include "protocol/protocolcodes.h"
#include <tibia/appearances/appearancetype.h>
#include <client/outfit.h>

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QProcess>
#include <QQuickWindow>
#include <QWindow>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"

namespace tibia::input {

namespace {

using A = EGameAction;

TargetHandler& targetHandler()
{
    static TargetHandler handler;
    return handler;
}

std::function<void(bool)>& targetSelectionListener()
{
    static std::function<void(bool)> listener;
    return listener;
}

/// CIP stack count for moves: map field appearance count, else inventory/container slot.
int storageSlotCount(const Position& position, const int stackPosition)
{
    if (position.x != UINT16_MAX) {
        if (worldmap::TWorldMapStorage* storage = game::currentWorldMapStorage()) {
            if (worldmap::TWorldMapField* field = storage->fieldAt(position)) {
                if (const appearances::TAppearanceInstancePtr appearance = field->appearanceAt(stackPosition)) {
                    if (const auto* object =
                            dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get())) {
                        if (TAppearanceType* type = object->thingType(); type && type->isStackable())
                            return std::max<int>(1, object->count());
                    }
                }
            }
        }
        return 1;
    }
    if (position.y & 0x40) {
        container::TContainerStorage* storage = game::currentContainerStorage();
        if (!storage)
            return 1;
        const container::TStoredContainer* row = storage->containerAt(position.y & 0x3F);
        if (!row)
            return 1;
        const container::TInventorySlot slot =
            storage->slotAt(row->id(), static_cast<int>(row->firstIndex()) + std::max(0, stackPosition));
        return slot.stackable ? std::max<int>(1, slot.count) : 1;
    }
    container::TInventoryContainer* inventory = game::currentInventoryContainer();
    if (!inventory)
        return 1;
    const container::TInventorySlot& slot = inventory->slotAt(position.y);
    return slot.stackable ? std::max<int>(1, slot.count) : 1;
}

}

int moveObjectCount(const TGameActionMoveObject& move)
{
    const TObjectGameActionContext& action = move.context();
    if (creatureOfObject(action))
        return 1;
    if (move.amount() > 0)
        return move.amount();
    if (action.objectClientId() && action.position().isValid())
        return storageSlotCount(action.position(), action.stackPosition());
    return 1;
}

namespace {

/// Focused QML/text field helpers (CIP REQUEST_SELECT_ALL_TEXT / COPY / CONFIRM_COPY).
QObject* focusedTextField()
{
    return QGuiApplication::focusObject();
}

}

TUnportedGameActionHandler::TUnportedGameActionHandler(QObject* parent)
    : QObject(parent)
{
}

void TUnportedGameActionHandler::handleGameAction(const IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    const EGameAction type = pGameAction->gameActionType();
    // Clipboard COPY_* / REQUEST_COPY_* are owned by TClipboardGameActionHandler.
    // A few actions must work before login (sound helper, mute, HUD toggle, relaunch,
    // character-selection shell, focused-text clipboard helpers).
    static constexpr A OfflineActions[] = {
        A::SOUND_TRIGGER, A::TOGGLE_SOUND, A::TOGGLE_CREATURE_HUDS_VISIBLE,
        A::TOGGLE_FULLSCREEN, A::QUIT_GAME, A::OPEN_OPTIONS_MENU_DIALOG, A::REQUEST_SELECT_ALL_TEXT,
        A::COPY_TEXT_TO_CLIPBOARD, A::COPY_TRUSTED_TEXT_TO_CLIPBOARD, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
        A::COPY_CREATURE_NAME_TO_CLIPBOARD, A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD,
        A::SHOW_CREATE_CHARACTER_DIALOG, A::CHARACER_SELECTION_AUTO_CLOSE, A::LENSHELP_SHOW, A::LENSHELP_HIDE,
        A::LENSHELP_ACTIVATE,
        // Login-mask Manage Account / logos / Create Account → PortalUrls handlers.
        A::OPEN_URL, A::OPEN_URL_GET_PREMIUM, A::OPEN_URL_GET_COINS, A::OPEN_URL_TIBIA_PAGE,
        A::OPEN_URL_CREATE_ACCOUNT, A::OPEN_URL_ACCESS_ACCOUNT, A::OPEN_URL_CIPSOFT, A::OPEN_URL_MANUAL,
        A::OPEN_URL_FAQ, A::OPEN_URL_SURVEY, A::OPEN_URL_LOST_ACCOUNT, A::OPEN_URL_PREMIUM_FEATURES,
        A::OPEN_URL_TWITCH_TIBIA, A::OPEN_URL_YOUTUBE_TIBIA,
        // Tutorial / sessiondump cinematic (may fire before gamesession isOnline).
        A::SHOW_TUTORIAL_HINT, A::CONFIRM_TUTORIAL_HINT, A::ADD_GUI_TUTORIAL_MARKER,
        A::REMOVE_ALL_TUTORIAL_MARKERS, A::ADD_HINT_AT_TUTORIAL_MARKER,
        A::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER, A::FINISHED_TUTORIAL_FOR_CHARACTER, A::TUTORIAL_PROGRESS,
        A::CHANGE_CONTEXT_MENU_ENABLED, A::SESSIONDUMP_START, A::SESSIONDUMP_PAUSE, A::SESSIONDUMP_RESUME,
        A::SESSIONDUMP_SKIP_TO,
    };
    const bool offlineOk = std::find(std::begin(OfflineActions), std::end(OfflineActions), type)
        != std::end(OfflineActions);
    const game::TGameSessionBase* session = game::currentGameSession();
    if (!(session && session->isOnline()) && !offlineOk)
        return;
    switch (type) {
    case A::ALLOW_ALL_TO_INSPECT_PLAYER:
        app::ClientOptions::store(QStringLiteral("inspectPlayerAllowAllEnabled"), true);
        break;
    case A::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER:
        app::ClientOptions::store(QStringLiteral("inspectPlayerAllowAllEnabled"), false);
        break;
    case A::CANCEL_USE_WITH:
    case A::CANCEL_CROSSHAIR_SPELL:
        finishTargetSelection(nullptr);
        break;
    case A::SHOW_MESSAGE_YOU_SEE_NOTHING: {
        Q_EMIT publishGameAction(std::make_shared<TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE,
                                                                             qtTrId("you_see_nothing"), QColor(Qt::green)));
        break;
    }
    case A::REQUEST_SELECT_ALL_TEXT:
        // The chat controller handles its own input and output (it listens too).
        if (QObject* field = focusedTextField())
            QMetaObject::invokeMethod(field, "selectAll", Qt::DirectConnection);
        break;
    case A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
        // qtclient: the focused text field's selection (CIP's text input handler is not ported).
        if (QObject* field = focusedTextField()) {
            const QString text = field->property("selectedText").toString();
            if (!text.isEmpty())
                QGuiApplication::clipboard()->setText(text);
        }
        break;
    default:
        break;
    }
}

void startTargetSelection(TargetHandler handler)
{
    finishTargetSelection(nullptr);
    targetHandler() = std::move(handler);
    if (targetHandler() && targetSelectionListener())
        targetSelectionListener()(true);
}

bool isSelectingTarget()
{
    return static_cast<bool>(targetHandler());
}

void finishTargetSelection(const TTargetSelectedPtr& target)
{
    if (TargetHandler handler = std::exchange(targetHandler(), {})) {
        if (targetSelectionListener())
            targetSelectionListener()(false);
        handler(target);
    }
}

void setTargetSelectionListener(std::function<void(bool)> listener)
{
    targetSelectionListener() = std::move(listener);
}

}
