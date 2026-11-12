#include "gameactionhandlers.h"

#include "app/clientoptions.h"
#include "appearances/appearanceinstance.h"
#include "container/containerciptypes.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "renderer/spritetextureprovider.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <creatures/creaturestorage.h>
#include <client/item.h>
#include "protocol/protocolcodes.h"
#include <tibia/assets/thingtype.h>
#include <client/outfit.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
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
#include <tibia/game/gamesession.h>

Q_LOGGING_CATEGORY(lcGameActions, "tibia.gameactions")

namespace tibia::input {

namespace {

using A = EGameAction;

std::function<void(const TGameAction&)>& minimapActionHandler()
{
    static std::function<void(const TGameAction&)> handler;
    return handler;
}

std::unordered_map<EGameAction, ActionHandler>& actionHandlers()
{
    static std::unordered_map<EGameAction, ActionHandler> handlers;
    return handlers;
}

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

std::map<int, std::pair<EGameAction, ActionListener>>& actionListeners()
{
    static std::map<int, std::pair<EGameAction, ActionListener>> listeners;
    return listeners;
}

bool notifyListeners(const TGameAction& action)
{
    std::vector<ActionListener> matching;
    for (const auto& [id, listener] : actionListeners())
        if (listener.first == action.type)
            matching.push_back(listener.second);
    for (const ActionListener& listener : matching)
        if (listener(action))
            return true;
    return false;
}

/// CIP stack count for moves: map field appearance count, else inventory/container slot.
int storageSlotCount(const Position& position, const int stackPosition)
{
    if (position.x != UINT16_MAX) {
        if (worldmap::TWorldMapStorage* storage = worldmap::TWorldMapStorage::instance()) {
            if (worldmap::TWorldMapField* field = storage->fieldAt(position)) {
                if (const appearances::TAppearanceInstancePtr appearance = field->appearanceAt(stackPosition)) {
                    if (const auto* object =
                            dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get())) {
                        if (ThingType* type = object->thingType(); type && type->isStackable())
                            return std::max<int>(1, object->count());
                    }
                }
            }
        }
        return 1;
    }
    if (position.y & 0x40) {
        container::TContainerStorage* storage = container::TContainerStorage::instance();
        if (!storage)
            return 1;
        const container::TStoredContainer* row = storage->containerAt(position.y & 0x3F);
        if (!row)
            return 1;
        const container::TInventorySlot slot =
            storage->slotAt(row->id, static_cast<int>(row->firstIndex) + std::max(0, stackPosition));
        return slot.stackable ? std::max<int>(1, slot.count) : 1;
    }
    container::TInventoryContainer* inventory = container::TInventoryContainer::instance();
    if (!inventory)
        return 1;
    const container::TInventorySlot& slot = inventory->slotAt(position.y);
    return slot.stackable ? std::max<int>(1, slot.count) : 1;
}

/// A dragged object's count: the split dialog's amount, else the whole stack.
/// Creatures (wire id Proto::Creature) always move as count 1.
int moveCount(const TGameAction& action)
{
    if (action.creatureId || action.objectClientId == Proto::Creature)
        return 1;
    if (action.amount > 0)
        return action.amount;
    if (action.objectClientId && action.position.isValid())
        return storageSlotCount(action.position, action.stackPosition);
    return 1;
}

/// Canonical map position + stack for a creature MOVE (CipSoft lookCreature sibling).
bool resolveCreatureMove(const TGameAction& action, Position* from, int* stackPos)
{
    if (!from || !stackPos || !action.creatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> row =
        storage ? storage->creatureAt(action.creatureId) : nullptr;
    if (!row || !row->position().isValid())
        return false;
    *from = row->position;
    *stackPos = -1;
    if (worldmap::TWorldMapStorage* map = worldmap::TWorldMapStorage::instance()) {
        if (worldmap::TWorldMapField* field = map->fieldAt(*from))
            *stackPos = field->creatureStackPos(action.creatureId);
    }
    return *stackPos >= 0;
}

/// Ephemeral Item only where classic outbound wire still needs an ItemPtr.
ItemPtr wireItem(const TGameAction& action)
{
    if (!action.objectClientId || !action.position.isValid() || action.stackPosition < 0)
        return nullptr;
    uint16_t count = 0;
    uint8_t tier = 0;
    uint8_t fluid = 0;
    if (action.position.x == UINT16_MAX) {
        if (action.position.y & 0x40) {
            if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
                if (const container::TStoredContainer* row = storage->containerAt(action.position.y & 0x3F)) {
                    const container::TInventorySlot slot =
                        storage->slotAt(row->id, static_cast<int>(row->firstIndex) + action.stackPosition);
                    count = slot.stackable ? slot.count : 0;
                    tier = slot.tier;
                    fluid = slot.fluidOrSplash ? slot.fluid : 0;
                }
            }
        } else if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
            const container::TInventorySlot& slot = inventory->slotAt(action.position.y);
            count = slot.stackable ? slot.count : 0;
            tier = slot.tier;
            fluid = slot.fluidOrSplash ? slot.fluid : 0;
        }
    }
    return tibia::game::ephemeralItemForWire(action.objectClientId, action.position, action.stackPosition, count, tier, fluid);
}

uint16_t wireTypeId(const TGameAction& action)
{
    return action.objectClientId;
}

void moveObject(const TGameAction& action)
{
    if (action.creatureId || action.objectClientId == Proto::Creature) {
        Position from;
        int stackPos = -1;
        if (!resolveCreatureMove(action, &from, &stackPos))
            return;
        tibia::game::moveMapObject(from, static_cast<uint16_t>(Proto::Creature), stackPos, action.destination, 1);
        return;
    }
    if (action.objectClientId && action.position.isValid() && action.stackPosition >= 0)
        tibia::game::moveMapObject(action.position, action.objectClientId, action.stackPosition, action.destination,
                             moveCount(action));
}

/// The open container window id an object lies in (-1 if none). OPEN reuses
/// that window (CIP: TContainerGameActionHandler).
int parentContainerId(const TGameAction& action)
{
    const Position& position = action.position;
    if (position.x != UINT16_MAX || !(position.y & 0x40))
        return -1;
    const int id = position.y & 0x3F;
    container::TContainerStorage* storage = container::TContainerStorage::instance();
    if (!storage || !storage->hasContainer(id))
        return -1;
    return id;
}

/// CIP's NPC talk handler : greet NPCs closer than four fields
/// on the player's floor.
void startTalking(const TGameAction& action)
{
    if (!action.creatureId)
        return;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> npc =
        storage ? storage->creatureAt(action.creatureId) : nullptr;
    if (!npc || !tibia::game::localPlayer())
        return;
    const Position& from = tibia::game::localPlayerPosition();
    const Position& to = npc->position();
    if (from.z != to.z || std::abs(from.x - to.x) >= 4 || std::abs(from.y - to.y) >= 4)
        return;
    tibia::game::greetNpc(action.creatureId);
}

void openLauncher()
{
    // Quit closes the window. That must not raise Exit / Logout / Cancel.
    gamewindow::TGameWindowController::allowProcessExit();
#ifndef Q_OS_WASM
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
#endif
    QCoreApplication::quit();
}

/// Focused QML/text field helpers (CIP REQUEST_SELECT_ALL_TEXT / COPY / CONFIRM_COPY).
QObject* focusedTextField()
{
    return QGuiApplication::focusObject();
}

/// CIP InspectPlayer action byte (0xCE). Crystal services 1–5 as character
/// inspection tabs; handshake values are still sent for CIP-compatible servers.
uint8_t inspectPlayerAction(const EGameAction type)
{
    switch (type) {
    case A::ALLOW_ALL_TO_INSPECT_PLAYER:
    case A::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER:
        return 0;
    case A::INVITE_TO_INSPECT_PLAYER:
        return 1;
    case A::ALLOW_INSPECT_PLAYER:
        return 2;
    case A::ASK_TO_INSPECT_PLAYER:
        return 3;
    case A::REVOKE_INSPECT_PLAYER:
        return 4;
    case A::INSPECT_PLAYER:
    default:
        return 5;
    }
}

void setLenshelp(const bool enabled)
{
    if (gamewindow::TMouseCursorController* cursor = gamewindow::TMouseCursorController::instance())
        cursor->setLenshelp(enabled);
    if (gamewindow::TLenshelpController* lenshelp = gamewindow::TLenshelpController::instance()) {
        if (enabled)
            lenshelp->showHelp(QCoreApplication::translate("cip", "Lenshelp"),
                               QCoreApplication::translate("cip", "Click an object for more information."));
        else
            lenshelp->hideHelp();
    }
}

void clearSpriteCaches()
{
    // CIP REMOVE_SPRITEIDENTIFIER_FROM_CACHE evicts one id; the atlas cache only
    // exposes clear. Drop every window's shared cache when a QGuiApplication
    // top-level window is available.
    const QWindowList windows = QGuiApplication::allWindows();
    for (QWindow* window : windows) {
        if (auto* quick = qobject_cast<QQuickWindow*>(window))
            renderer::TSpriteTextureProviderManager::sharedCache(quick).clear();
    }
}

}

void publish(const TGameActionPtr& action)
{
    if (!action)
        return;
    // Clipboard COPY_* / REQUEST_COPY_* are owned by TClipboardGameActionHandler.
    // A few actions must work before login (sound helper, mute, HUD toggle, relaunch,
    // character-selection shell, focused-text clipboard helpers).
    static constexpr A OfflineActions[] = {
        A::SOUND_TRIGGER, A::TOGGLE_SOUND, A::TOGGLE_CREATURE_HUDS_VISIBLE, A::OPEN_LAUNCHER,
        A::TOGGLE_FULLSCREEN, A::QUIT_GAME, A::OPEN_OPTIONS_MENU_DIALOG, A::REQUEST_SELECT_ALL_TEXT,
        A::COPY_TEXT_TO_CLIPBOARD, A::COPY_TRUSTED_TEXT_TO_CLIPBOARD, A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
        A::COPY_CREATURE_NAME_TO_CLIPBOARD, A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
        A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD, A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD,
        A::SHOW_CREATE_CHARACTER_DIALOG, A::CHARACER_SELECTION_AUTO_CLOSE, A::LENSHELP_SHOW, A::LENSHELP_HIDE,
        A::LENSHELP_ACTIVATE, A::REMOVE_SPRITEIDENTIFIER_FROM_CACHE, A::RESOLVE_SPRITE, A::SPRITE_RESOLVED,
        A::OPEN_SHOW_IMAGE_DIALOG,
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
    const bool offlineOk = std::find(std::begin(OfflineActions), std::end(OfflineActions), action->type)
        != std::end(OfflineActions);
    if (!tibia::game::isOnline() && !offlineOk)
        return;
    if (notifyListeners(*action))
        return;
    if (const auto it = actionHandlers().find(action->type); it != actionHandlers().end() && it->second(*action))
        return;
    switch (action->type) {
    case A::SMART_CLICK:
        publish(action->inner);
        break;
    case A::LOOK:
        if (action->objectClientId && action->position.isValid() && action->stackPosition >= 0)
            tibia::game::lookMapObject(action->position, action->objectClientId, action->stackPosition);
        break;
    case A::LOOK_AT_CREATURE:
        if (action->creatureId)
            tibia::game::lookCreature(action->creatureId);
        break;
    case A::USE:
        if (action->objectClientId && action->position.isValid() && action->stackPosition >= 0)
            tibia::game::useMapObject(action->position, action->objectClientId, action->stackPosition);
        break;
    case A::TRY_TO_EQUIP:
        if (const uint16_t typeId = wireTypeId(*action); typeId != 0) {
            uint8_t tier = 0;
            if (const ItemPtr item = wireItem(*action))
                tier = item->getTier();
            tibia::game::equipItemId(typeId, tier);
        }
        break;
    case A::OPEN:
    case A::OPEN_IN_NEW_WINDOW:
        if (const uint16_t typeId = wireTypeId(*action); typeId != 0 && action->stackPosition >= 0)
            tibia::game::openAt(action->position, typeId, action->stackPosition,
                          action->type == A::OPEN ? parentContainerId(*action) : -1);
        break;
    case A::OPEN_PARENT_CONTAINER:
        if (const int containerId = parentContainerId(*action); containerId >= 0)
            tibia::game::openParentById(containerId);
        else if (wireTypeId(*action) != 0) {
            // Depot-search parent: the object's field (CIP open parent from search).
            tibia::game::openParentContainerFromDepotSearch(action->position);
        }
        break;
    case A::BROWSE_FIELD:
        if (action->position.isValid())
            tibia::game::browseField(action->position);
        break;
    case A::GATHER_ALL:
        // Handled by TGatherAllGameActionHandler when registered.
        break;
    case A::ROTATE_OBJECT:
        if (action->objectClientId && action->position.isValid() && action->stackPosition >= 0)
            tibia::game::rotateMapObject(action->position, action->objectClientId, action->stackPosition);
        break;
    case A::INSPECT_OBJECT:
        if (action->position.isValid())
            tibia::game::inspectionNormalObject(action->position);
        break;
    case A::INSPECT_PLAYER:
    case A::INVITE_TO_INSPECT_PLAYER:
    case A::ASK_TO_INSPECT_PLAYER:
    case A::ALLOW_INSPECT_PLAYER:
    case A::REVOKE_INSPECT_PLAYER:
        if (action->creatureId)
            tibia::game::inspectCharacter(action->creatureId, inspectPlayerAction(action->type));
        break;
    case A::ALLOW_ALL_TO_INSPECT_PLAYER:
        app::ClientOptions::store(QStringLiteral("inspectPlayerAllowAllEnabled"), true);
        // CIP InspectPlayer action 0 (no creature): allow everyone. Crystal no-ops today.
        tibia::game::inspectCharacter(0, inspectPlayerAction(A::ALLOW_ALL_TO_INSPECT_PLAYER));
        break;
    case A::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER:
        app::ClientOptions::store(QStringLiteral("inspectPlayerAllowAllEnabled"), false);
        tibia::game::inspectCharacter(0, inspectPlayerAction(A::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER));
        break;
    case A::INVITE_TO_PARTY:
        if (action->creatureId)
            tibia::game::partyInvite(action->creatureId);
        break;
    case A::JOIN_PARTY:
        if (action->creatureId)
            tibia::game::partyJoin(action->creatureId);
        break;
    case A::REVOKE_PARTY_INVITATION:
        if (action->creatureId)
            tibia::game::partyRevokeInvitation(action->creatureId);
        break;
    case A::PASS_PARTY_LEADERSHIP:
        if (action->creatureId)
            tibia::game::partyPassLeadership(action->creatureId);
        break;
    case A::LEAVE_PARTY: {
        const uint8_t shield = tibia::game::localPlayerShield();
        if (tibia::game::localPlayer() && (shield == 1 || shield == 3))
            tibia::game::partyDisband();
        else
            tibia::game::partyLeave();
        break;
    }
    case A::SET_SHARED_EXPERIENCE:
        tibia::game::partyShareExperience(action->enable);
        break;
    case A::REQUEST_DEPOT_SEARCH:
        tibia::game::openDepotSearch();
        break;
    case A::DEPOT_SEARCH_CLOSED:
        tibia::game::closeDepotSearchRequest();
        break;
    case A::REQUEST_SEARCH_TYPE_IN_DEPOT:
        if (const uint16_t typeId = wireTypeId(*action); typeId != 0)
            tibia::game::requestDepotSearchItem(typeId, static_cast<uint8_t>(std::max(0, action->parameter)));
        else if (action->parameter > 0)
            tibia::game::requestDepotSearchItem(static_cast<uint16_t>(action->parameter), 0);
        break;
    case A::DEPOT_SERACH_RETRIEVE:
        tibia::game::retrieveDepotSearch(static_cast<uint16_t>(action->exaltationTypeId ? action->exaltationTypeId : action->parameter),
                                   action->exaltationTier, static_cast<uint8_t>(action->category >= 0 ? action->category : 0));
        break;
    case A::CANCEL_USE_WITH:
    case A::CANCEL_CROSSHAIR_SPELL:
        finishTargetSelection(nullptr);
        break;
    case A::TOGGLE_CREATURE_HUDS_VISIBLE:
    case A::TOGGLE_SOUND:
        // Handled by TGenericGameActionHandler when registered.
        break;
    case A::OPEN_LAUNCHER:
        openLauncher();
        break;
    case A::WRAP:
    case A::UNWRAP:
        if (action->objectClientId && action->position.isValid() && action->stackPosition >= 0)
            tibia::game::wrapMapObject(action->position, action->objectClientId, action->stackPosition);
        break;
    case A::ATTACK_CREATURE:
        if (action->creatureId)
            tibia::game::attack(action->creatureId);
        break;
    case A::STOP_ATTACKING_CREATURE:
        tibia::game::cancelAttack();
        break;
    case A::FOLLOW_CREATURE:
        if (action->creatureId)
            tibia::game::follow(action->creatureId);
        break;
    case A::STOP_FOLLOWING_CREATURE:
        tibia::game::cancelFollow();
        break;
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
        // Handled by TClipboardGameActionHandler when registered.
        break;
    case A::SET_PLAYER_MOVEMENT_INTENT:
        // Handled by TPlayerMovementGameActionHandler when registered.
        break;
    case A::START_TALKING_TO_NPC:
        startTalking(*action);
        break;
    case A::MOUNT_PLAYER:
        tibia::game::mount(true);
        break;
    case A::UNMOUNT_PLAYER:
        tibia::game::mount(false);
        break;
    case A::TOGGLE_MOUNTED:
        if (tibia::game::localPlayer())
            tibia::game::mount(!tibia::game::localPlayerOutfit().hasMount());
        break;
    case A::MOVE_OBJECT:
    case A::MOVE_UP:
        moveObject(*action);
        break;
    case A::SHOW_SPLIT_STACK_DIALOG:
        if (action->inner)
            moveObject(*action->inner);
        break;
    case A::STOP_ATTACKING_FOLLOWING_WALKING:
        tibia::game::stop();
        break;
    case A::SET_TACTICS:
        tibia::game::setTactics(action->chase ? tibia::client::ChaseOpponent : tibia::client::DontChase, action->secure,
                          static_cast<tibia::client::PVPModes>(action->pvpMode));
        break;
    case A::GOD_TELEPORT:
        // CIP: TGameActionTeleportToCoordinate -> GameclientMessageTeleport.
        tibia::game::sendTeleport(action->position);
        break;
    case A::REQUEST_RESOURCE_BALANCE:
        // CIP : GameclientMessageRequestResourceBalance with the action's resource type.
        tibia::game::requestResourceBalance(static_cast<uint8_t>(action->parameter));
        break;
    case A::CONFIGURE_SHOW_OFF_SOCKET:
        if (const uint16_t typeId = wireTypeId(*action); typeId != 0) {
            const uint8_t stackpos = action->stackPosition >= 0 ? static_cast<uint8_t>(action->stackPosition) : 0;
            tibia::game::configureShowOffSocket(action->position, typeId, stackpos);
        }
        break;
    case A::SET_PLAYER_OUTFIT:
        if (action->outfit.getId() != 0)
            tibia::game::changeOutfitAndMount(action->outfit, action->mountCharacter, action->randomMount);
        break;
    case A::SET_HIRELING_OUTFIT:
        if (action->outfit.getId() != 0)
            tibia::game::setHirelingOutfit(action->outfit, action->creatureId);
        break;
    case A::SET_SHOW_OFF_SOCKET_CONFIGURATION: {
        // Outfit id 0 is valid (clear the displayed look / hide outfit).
        const uint16_t typedId = wireTypeId(*action);
        const uint16_t itemId = typedId ? typedId : static_cast<uint16_t>(action->parameter);
        if (itemId != 0) {
            const uint8_t stackpos = action->stackPosition >= 0 ? static_cast<uint8_t>(action->stackPosition) : 0;
            tibia::game::setShowOffSocketConfiguration(action->outfit, action->position, itemId, stackpos,
                                                 static_cast<uint8_t>(action->lookDirection),
                                                 action->showSocket || action->enable ? 1 : 0);
        }
        break;
    }
    case A::SET_CREATURE_PODIUM_CONFIGURATION: {
        const uint16_t typedId = wireTypeId(*action);
        const uint16_t itemId = typedId ? typedId : static_cast<uint16_t>(action->parameter);
        const uint8_t stackpos = action->stackPosition >= 0 ? static_cast<uint8_t>(action->stackPosition) : 0;
        tibia::game::setCreaturePodiumConfiguration(action->creatureId, action->position, itemId, stackpos,
                                              static_cast<uint8_t>(action->lookDirection),
                                              action->enable ? 1 : 0, action->showSocket || action->chase ? 1 : 0);
        break;
    }
    case A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG:
        tibia::game::sendOpenRewardWall();
        break;
    case A::CLOSE_REWARD_WALL_DIALOG:
        // Local dialog close is owned by the reward-wall controller; the server
        // also pushes CloseRewardWall. Clear the button-bar highlight.
        publish(makeAction(A::DAILY_REWARD_NOT_COLLECTABLE));
        break;
    case A::REQUEST_DAILY_REWARD_HISTORY:
        tibia::game::requestOpenRewardHistory();
        break;
    case A::COLLECT_FIXED_ITEMS_DAILY_REWARD:
        tibia::game::requestGetRewardDaily(static_cast<uint8_t>(action->parameter > 0 ? action->parameter : 0), {});
        break;
    case A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE:
    case A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE: {
        auto status = mutableClone(*makeAction(A::SET_STATUS_LINE));
        status->text = action->text.isEmpty()
            ? qtTrId(action->type == A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE
                         ? "rewardwall_need_to_reach_main_text"
                         : "rewardwall_no_daily_reward_available_caption")
            : action->text;
        status->color = QColor(Qt::white);
        publish(status);
        break;
    }
    case A::SHOW_MESSAGE_YOU_SEE_NOTHING: {
        auto message = std::make_shared<TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE);
        message->text = action->text.isEmpty() ? qtTrId("you_see_nothing") : action->text;
        message->color = action->color.isValid() ? action->color : QColor(Qt::green);
        publish(message);
        break;
    }
    case A::JOIN_AGGRESSION:
        if (action->creatureId)
            tibia::game::joinAggression(action->creatureId);
        break;
    case A::REQUEST_TRANSACTION_DETAILS:
        tibia::game::requestTransactionDetails(static_cast<uint32_t>(action->instance));
        break;
    case A::OPEN_TRADE_CHARACTER_DIALOG:
        // CIP → TCharacterTradeDialogController (CharacterTradeDialog.qml); action 0 = conditions.
        if (gamewindow::TCharacterTradeDialogController* trade =
                gamewindow::TCharacterTradeDialogController::instance()) {
            tibia::game::requestCharacterTradeConfiguration(0);
            trade->showDialog();
        } else {
            tibia::game::requestCharacterTradeConfiguration(0);
        }
        break;
    case A::MINIMAP_ONE_FLOOR_UP:
    case A::MINIMAP_ONE_FLOOR_DOWN:
    case A::MINIMAP_ZOOM_IN:
    case A::MINIMAP_ZOOM_OUT:
    case A::MINIMAP_SCROLL_NORTH:
    case A::MINIMAP_SCROLL_EAST:
    case A::MINIMAP_SCROLL_SOUTH:
    case A::MINIMAP_SCROLL_WEST:
    case A::MINIMAP_CENTER:
        if (const auto& handler = minimapActionHandler())
            handler(*action);
        break;
    case A::START_TARGET_SELECTION:
        // CIP: TTargetSelectionController handles this; our listener mirrors it.
        if (const auto& listener = targetSelectionListener())
            listener(true);
        break;
    case A::TARGET_SELECTION_FINISHED:
        if (const auto& listener = targetSelectionListener())
            listener(false);
        break;
    case A::LENSHELP_SHOW:
        setLenshelp(true);
        break;
    case A::LENSHELP_HIDE:
        setLenshelp(false);
        break;
    case A::REQUEST_SELECT_ALL_TEXT:
        // qtclient: qtclient chat TextEdits are often not focusable — fall back to qtclient chat select-all.
        if (QObject* field = focusedTextField()) {
            if (QMetaObject::invokeMethod(field, "selectAll", Qt::DirectConnection))
                break;
        }
        publish(makeAction(A::CHAT_CHANNEL_SELECT_ALL));
        break;
    case A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
    case A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD:
        // Handled by TClipboardGameActionHandler when registered.
        break;
    case A::REMOVE_SPRITEIDENTIFIER_FROM_CACHE:
        clearSpriteCaches();
        break;
    case A::RESOLVE_SPRITE:
        // CIP resolves a sprite id into the texture atlas. Without a window-bound
        // SpriteKey here we acknowledge the action; sheets load on next draw.
        qCInfo(lcGameActions) << "RESOLVE_SPRITE" << action->parameter;
        break;
    case A::SPRITE_RESOLVED:
        qCInfo(lcGameActions) << "SPRITE_RESOLVED" << action->parameter;
        break;
    case A::OPEN_SHOW_IMAGE_DIALOG:
        if (gamewindow::TShowImageDialogController* images = gamewindow::TShowImageDialogController::instance()) {
            const QString path = action->text;
            images->showImages(path.isEmpty() ? QStringList{} : QStringList{ path }, path);
        } else {
            qCInfo(lcGameActions) << "OPEN_SHOW_IMAGE_DIALOG" << action->text << action->parameter;
        }
        break;
    case A::SHOW_TUTORIAL_HINT:
    case A::CONFIRM_TUTORIAL_HINT:
    case A::ADD_GUI_TUTORIAL_MARKER:
    case A::REMOVE_ALL_TUTORIAL_MARKERS:
    case A::ADD_HINT_AT_TUTORIAL_MARKER:
    case A::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER:
    case A::FINISHED_TUTORIAL_FOR_CHARACTER:
    case A::TUTORIAL_PROGRESS:
    case A::SESSIONDUMP_START:
    case A::SESSIONDUMP_PAUSE:
    case A::SESSIONDUMP_RESUME:
    case A::SESSIONDUMP_SKIP_TO:
        // Handled by TTutorialShell action handlers when the game window is up.
        break;
    default:
        break;
    }
}

void setMinimapActionHandler(std::function<void(const TGameAction&)> handler)
{
    minimapActionHandler() = std::move(handler);
}

void setActionHandler(const EGameAction type, ActionHandler handler)
{
    if (handler)
        actionHandlers()[type] = std::move(handler);
    else
        actionHandlers().erase(type);
}

int addActionListener(const EGameAction type, ActionListener listener)
{
    static int nextId = 0;
    actionListeners().emplace(++nextId, std::make_pair(type, std::move(listener)));
    return nextId;
}

void removeActionListener(const int id) { actionListeners().erase(id); }

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

void finishTargetSelection(const TGameActionPtr& target)
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
