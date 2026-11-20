#include "game/objectreference.h"

#include "platform/platform.h"
#include <optional>
#include "gameactionhandlers.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "appearances/appearanceinstance.h"
#include "container/containerciptypes.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "renderer/spritetextureprovider.h"
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
#include <tibia/game/gamesession.h>

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
        if (worldmap::TWorldMapStorage* storage = worldmap::TWorldMapStorage::instance()) {
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
int moveCount(const TGameActionMoveObject& move)
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

/// Canonical map position + stack for a creature MOVE (CipSoft lookCreature sibling).
bool resolveCreatureMove(const uint32_t creatureId, Position* from, int* stackPos)
{
    if (!from || !stackPos || !creatureId)
        return false;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> row =
        storage ? storage->creatureAt(creatureId) : nullptr;
    if (!row || !row->position().isValid())
        return false;
    *from = row->position();
    *stackPos = -1;
    if (worldmap::TWorldMapStorage* map = worldmap::TWorldMapStorage::instance()) {
        if (worldmap::TWorldMapField* field = map->fieldAt(*from))
            *stackPos = field->creatureStackPos(creatureId);
    }
    return *stackPos >= 0;
}

/// The action's object with the protocol details a client message needs.
std::optional<tibia::game::TObjectReference> wireObject(const TObjectGameActionContext& action)
{
    if (!action.objectClientId() || !action.position().isValid() || action.stackPosition() < 0)
        return std::nullopt;
    uint16_t count = 0;
    uint8_t tier = 0;
    uint8_t fluid = 0;
    if (action.position().x == UINT16_MAX) {
        if (action.position().y & 0x40) {
            if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
                if (const container::TStoredContainer* row = storage->containerAt(action.position().y & 0x3F)) {
                    const container::TInventorySlot slot =
                        storage->slotAt(row->id, static_cast<int>(row->firstIndex) + action.stackPosition());
                    count = slot.stackable ? slot.count : 0;
                    tier = slot.tier;
                    fluid = slot.fluidOrSplash ? slot.fluid : 0;
                }
            }
        } else if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
            const container::TInventorySlot& slot = inventory->slotAt(action.position().y);
            count = slot.stackable ? slot.count : 0;
            tier = slot.tier;
            fluid = slot.fluidOrSplash ? slot.fluid : 0;
        }
    }
    return tibia::game::TObjectReference{ action.objectClientId(), action.position(), action.stackPosition(), count, tier, fluid };
}

uint16_t wireTypeId(const TObjectGameActionContext& action)
{
    return action.objectClientId();
}

void moveObject(const TGameActionMoveObject& move)
{
    if (!move.destination())
        return;
    const TObjectGameActionContext& action = move.context();
    if (const uint32_t creatureId = creatureOfObject(action)) {
        Position from;
        int stackPos = -1;
        if (!resolveCreatureMove(creatureId, &from, &stackPos))
            return;
        tibia::game::moveMapObject(from, static_cast<uint16_t>(Proto::Creature), stackPos, *move.destination(), 1);
        return;
    }
    if (action.objectClientId() && action.position().isValid() && action.stackPosition() >= 0)
        tibia::game::moveMapObject(action.position(), action.objectClientId(), action.stackPosition(), *move.destination(),
                                   moveCount(move));
}

/// The open container window id an object lies in (-1 if none). OPEN reuses
/// that window (CIP: TContainerGameActionHandler).
int parentContainerId(const TObjectGameActionContext& action)
{
    const Position& position = action.position();
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
void startTalking(TUnportedGameActionHandler& handler, const uint32_t creatureId)
{
    if (!creatureId)
        return;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    const std::shared_ptr<tibia::creatures::TCreature> npc =
        storage ? storage->creatureAt(creatureId) : nullptr;
    if (!npc || !tibia::game::hasLocalPlayer())
        return;
    // Within three fields on the floor the local channel opens and the NPC is
    // greeted; farther away the status line says so.
    const Position& from = tibia::game::localPlayerPosition();
    const Position& to = npc->position();
    if (from.z != to.z || std::abs(from.x - to.x) >= 4 || std::abs(from.y - to.y) >= 4) {
        Q_EMIT handler.publishGameAction(std::make_shared<TGameActionSetStatusLine>(qtTrId("you_are_too_far_away")));
        return;
    }
    Q_EMIT handler.publishGameAction(
        std::make_shared<TGameActionChatChannel>(A::OPEN_CHAT_CHANNEL, std::make_shared<const chat::TChatChannelIdentifier>()));
    tibia::game::greetNpc(creatureId);
}

void openLauncher()
{
    // Quit closes the window. That must not raise Exit / Logout / Cancel.
    gamewindow::TGameWindowController::allowProcessExit();
    tibia::platform::restartApplication();
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
    default:
        break;
    }
    // CIP creatures::TCreaturesGameActionHandler: INVITE_TO_INSPECT_PLAYER (15) ..
    // REVOKE_INSPECT_PLAYER (19) send their offset from 14, anything else 4.
    const int value = int(type);
    if (unsigned(value - int(A::INVITE_TO_INSPECT_PLAYER)) < 5u)
        return uint8_t(value - (int(A::INVITE_TO_INSPECT_PLAYER) - 1));
    return 4;
}

void clearSpriteCaches()
{
    // CIP REMOVE_SPRITEIDENTIFIER_FROM_CACHE evicts one id; the atlas cache only
    // exposes clear. Drop every window's shared cache when a QGuiApplication
    // top-level window is available.
    const QWindowList windows = QGuiApplication::allWindows();
    for (QWindow* window : windows) {
        if (auto* quick = qobject_cast<QQuickWindow*>(window))
            renderer::TSpriteTextureProviderManager::sharedProvider(quick).cache().clear();
    }
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
    const bool offlineOk = std::find(std::begin(OfflineActions), std::end(OfflineActions), type)
        != std::end(OfflineActions);
    if (!tibia::game::isOnline() && !offlineOk)
        return;
    // The object the action is on (CIP: its location and object contexts).
    const TObjectGameActionContext object = objectContextOf(*pGameAction).value_or(TObjectGameActionContext{});
    const uint32_t creatureId = creatureIdOf(*pGameAction);
    switch (type) {
    case A::LOOK_AT_CREATURE:
        // CIP creatures::TCreaturesGameActionHandler sends LookAtCreature for the creature id.
        if (creatureId)
            tibia::game::lookCreature(creatureId, true);
        break;
    case A::TRY_TO_EQUIP:
        if (const uint16_t typeId = wireTypeId(object); typeId != 0) {
            // The object's own tier (a hotkey's), else the slot's.
            uint8_t tier = 0;
            if (const auto* upgradeable = gameActionContextAs<TUpgradeableObjectApperanceTypeGameActionContext>(object.object()))
                tier = upgradeable->tier();
            else if (const auto wire = wireObject(object))
                tier = wire->tier;
            tibia::game::equipItemId(typeId, tier);
        }
        break;
    case A::OPEN_PARENT_CONTAINER:
        if (const int containerId = parentContainerId(object); containerId >= 0)
            tibia::game::openParentById(containerId);
        else if (wireTypeId(object) != 0) {
            // Depot-search parent: the object's field (CIP open parent from search).
            tibia::game::openParentContainerFromDepotSearch(object.position());
        }
        break;
    case A::INSPECT_OBJECT:
        if (object.position().isValid())
            tibia::game::inspectionNormalObject(object.position());
        break;
    case A::INSPECT_PLAYER:
    case A::INVITE_TO_INSPECT_PLAYER:
    case A::ASK_TO_INSPECT_PLAYER:
    case A::ALLOW_INSPECT_PLAYER:
    case A::REVOKE_INSPECT_PLAYER:
        if (creatureId)
            tibia::game::inspectCharacter(creatureId, inspectPlayerAction(type));
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
        if (creatureId)
            tibia::game::partyInvite(creatureId);
        break;
    case A::JOIN_PARTY:
        if (creatureId)
            tibia::game::partyJoin(creatureId);
        break;
    case A::REVOKE_PARTY_INVITATION:
        if (creatureId)
            tibia::game::partyRevokeInvitation(creatureId);
        break;
    case A::PASS_PARTY_LEADERSHIP:
        if (creatureId)
            tibia::game::partyPassLeadership(creatureId);
        break;
    case A::LEAVE_PARTY:
        tibia::game::partyLeave();
        break;
    case A::SET_SHARED_EXPERIENCE:
        if (const auto* shared = gameActionAs<TGameActionSetSharedExperience>(pGameAction))
            tibia::game::partyShareExperience(shared->enable());
        break;
    case A::REQUEST_DEPOT_SEARCH:
        tibia::game::openDepotSearch();
        break;
    case A::DEPOT_SEARCH_CLOSED:
        tibia::game::closeDepotSearchRequest();
        break;
    case A::REQUEST_SEARCH_TYPE_IN_DEPOT:
        if (const auto* search = gameActionAs<TGameActionRequestSearchTypeInDepot>(pGameAction); search && search->typeId() != 0)
            tibia::game::requestDepotSearchItem(search->typeId(), static_cast<uint8_t>(search->tier()));
        break;
    case A::DEPOT_SERACH_RETRIEVE:
        if (const auto* retrieve = gameActionAs<TGameActionDepotSearchRetrieve>(pGameAction))
            tibia::game::retrieveDepotSearch(retrieve->typeId(), static_cast<uint8_t>(retrieve->tier()),
                                             static_cast<uint8_t>(std::max(retrieve->location(), 0)));
        break;
    case A::CANCEL_USE_WITH:
    case A::CANCEL_CROSSHAIR_SPELL:
        finishTargetSelection(nullptr);
        break;
    case A::OPEN_LAUNCHER:
        openLauncher();
        break;
    case A::ATTACK_CREATURE:
        if (creatureId)
            tibia::game::attack(creatureId);
        break;
    case A::STOP_ATTACKING_CREATURE:
        tibia::game::cancelAttack();
        break;
    case A::FOLLOW_CREATURE:
        if (creatureId)
            tibia::game::follow(creatureId);
        break;
    case A::STOP_FOLLOWING_CREATURE:
        tibia::game::cancelFollow();
        break;
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
        // qtclient: CIP's handler of this TGameActionCreature is not ported yet.
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = creatureId ? storage->creatureAt(creatureId) : nullptr)
                QGuiApplication::clipboard()->setText(row->name());
        }
        break;
    case A::START_TALKING_TO_NPC:
        startTalking(*this, creatureId);
        break;
    case A::MOUNT_PLAYER:
        tibia::game::mount(true);
        break;
    case A::UNMOUNT_PLAYER:
        tibia::game::mount(false);
        break;
    case A::TOGGLE_MOUNTED:
        if (tibia::game::hasLocalPlayer())
            tibia::game::mount(!tibia::game::localPlayerOutfit().hasMount());
        break;
    case A::MOVE_OBJECT:
        moveObject(static_cast<const TGameActionMoveObject&>(*pGameAction));
        break;
    case A::STOP_ATTACKING_FOLLOWING_WALKING:
        tibia::game::stop();
        break;
    case A::SET_TACTICS:
        if (const auto* set = gameActionAs<TGameActionSetTactics>(pGameAction)) {
            const TGameActionSetTactics::TTactics& tactics = set->tactics();
            tibia::game::setTactics(tactics.chase ? tibia::client::ChaseOpponent : tibia::client::DontChase, tactics.secure,
                                    static_cast<tibia::client::PVPModes>(tactics.pvpMode));
        }
        break;
    case A::SET_PLAYER_OUTFIT:
        if (const auto* set = gameActionAs<TGameActionSetPlayerOutfit>(pGameAction); set && set->outfit().getId() != 0)
            tibia::game::changeOutfitAndMount(set->outfit(), set->mountCharacter(), set->randomMount());
        break;
    case A::SET_HIRELING_OUTFIT:
        if (const auto* set = gameActionAs<TGameActionSetHirelingOutfit>(pGameAction); set && set->outfit().getId() != 0)
            tibia::game::setHirelingOutfit(set->outfit(), set->creatureId());
        break;
    case A::SET_SHOW_OFF_SOCKET_CONFIGURATION:
        // Outfit id 0 is valid (clear the displayed look / hide outfit).
        if (const auto* set = gameActionAs<TGameActionSetShowOffSocketConfiguration>(pGameAction); set && set->podium().itemId != 0)
            tibia::game::setShowOffSocketConfiguration(set->outfit(), set->podium().position, set->podium().itemId,
                                                       set->podium().stackPosition, set->lookDirection(), set->showSocket() ? 1 : 0);
        break;
    case A::SET_CREATURE_PODIUM_CONFIGURATION:
        if (const auto* set = gameActionAs<TGameActionSetCreaturePodiumConfiguration>(pGameAction))
            tibia::game::setCreaturePodiumConfiguration(set->raceId(), set->podium().position, set->podium().itemId,
                                                        set->podium().stackPosition, set->lookDirection(), set->showPodium() ? 1 : 0,
                                                        set->showCreature() ? 1 : 0);
        break;
    case A::SHOW_MESSAGE_YOU_SEE_NOTHING: {
        Q_EMIT publishGameAction(std::make_shared<TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE,
                                                                             qtTrId("you_see_nothing"), QColor(Qt::green)));
        break;
    }
    case A::JOIN_AGGRESSION:
        if (creatureId)
            tibia::game::joinAggression(creatureId);
        break;
    case A::REQUEST_TRANSACTION_DETAILS:
        if (const auto* details = gameActionAs<TGameActionRequestTransactionDetailsFromServer>(pGameAction))
            tibia::game::requestTransactionDetails(static_cast<uint32_t>(details->transactionId()));
        break;
    case A::OPEN_TRADE_CHARACTER_DIALOG:
        if (gamewindow::TCharacterTradeDialogController* trade = gamewindow::TCharacterTradeDialogController::instance())
            trade->showDialog();
        break;
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
    case A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD:
        // qtclient: TLinkCopyWarningDialogController is not wired to the router yet; copy.
        if (const auto* copy = gameActionAs<TGameActionCopyTextToClipboard>(pGameAction))
            QGuiApplication::clipboard()->setText(copy->text());
        break;
    case A::REMOVE_SPRITEIDENTIFIER_FROM_CACHE:
        clearSpriteCaches();
        break;
    case A::RESOLVE_SPRITE:
        // CIP resolves a sprite id into the texture atlas. Without a window-bound
        // sprite identifier here we acknowledge the action; sheets load on next draw.
        break;
    case A::SPRITE_RESOLVED:
        break;
    case A::OPEN_SHOW_IMAGE_DIALOG:
        if (const auto* show = gameActionAs<TGameActionOpenShowImageDialog>(pGameAction)) {
            if (gamewindow::TShowImageDialogController* images = gamewindow::TShowImageDialogController::instance()) {
                const QString& path = show->imagePath();
                images->showImages(path.isEmpty() ? QStringList{} : QStringList{ path }, path);
            }
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
