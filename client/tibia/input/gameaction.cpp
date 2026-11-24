#include "gameaction.h"

#include "chat/chatchannel.h"
#include "protocol/protocolcodes.h"
#include "worldmap/worldmapfield.h"
#include "worldmap/worldmapstorage.h"

#include <algorithm>

namespace tibia::input {

namespace {

constexpr EGameAction Separator = EGameAction::CONTEXT_MENU_SEPARATOR;

const IGameActionPtr* firstOfType(const TGameActionList& candidates, const EGameAction type)
{
    const auto it = std::find_if(candidates.begin(), candidates.end(),
                                 [type](const IGameActionPtr& action) { return action && action->gameActionType() == type; });
    return it == candidates.end() ? nullptr : &*it;
}

}

namespace {

/// The types CIP builds as TGameActionCreature.
bool isTGameActionCreatureType(const EGameAction type)
{
    switch (type) {
    case EGameAction::ATTACK_CREATURE:
    case EGameAction::STOP_ATTACKING_CREATURE:
    case EGameAction::FOLLOW_CREATURE:
    case EGameAction::STOP_FOLLOWING_CREATURE:
    case EGameAction::LOOK_AT_CREATURE:
    case EGameAction::TEMPORARY_SELECT_CREATURE:
    case EGameAction::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case EGameAction::INVITE_TO_PARTY:
    case EGameAction::JOIN_PARTY:
    case EGameAction::REVOKE_PARTY_INVITATION:
    case EGameAction::PASS_PARTY_LEADERSHIP:
    case EGameAction::JOIN_AGGRESSION:
    case EGameAction::INSPECT_PLAYER:
    case EGameAction::ASK_TO_INSPECT_PLAYER:
    case EGameAction::INVITE_TO_INSPECT_PLAYER:
    case EGameAction::ALLOW_INSPECT_PLAYER:
    case EGameAction::REVOKE_INSPECT_PLAYER:
    case EGameAction::REQUEST_SKILL_WHEEL:
        return true;
    default:
        return false;
    }
}

bool isCreatureActionType(const EGameAction type)
{
    switch (type) {
    case EGameAction::ATTACK_CREATURE:
    case EGameAction::STOP_ATTACKING_CREATURE:
    case EGameAction::FOLLOW_CREATURE:
    case EGameAction::STOP_FOLLOWING_CREATURE:
    case EGameAction::LOOK_AT_CREATURE:
    case EGameAction::TEMPORARY_SELECT_CREATURE:
    case EGameAction::TARGET_SELECTED:
    case EGameAction::START_TALKING_TO_NPC:
    case EGameAction::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case EGameAction::INVITE_TO_PARTY:
    case EGameAction::JOIN_PARTY:
    case EGameAction::REVOKE_PARTY_INVITATION:
    case EGameAction::PASS_PARTY_LEADERSHIP:
    case EGameAction::JOIN_AGGRESSION:
    case EGameAction::REPORT_BOT:
    case EGameAction::REPORT_HIRELING_NAME:
    case EGameAction::REQUEST_OUTFITS_FROM_SERVER:
    case EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE:
    case EGameAction::MOUNT_PLAYER:
    case EGameAction::UNMOUNT_PLAYER:
    case EGameAction::OPEN_PREY_DIALOG:
    case EGameAction::INSPECT_PLAYER:
    case EGameAction::ASK_TO_INSPECT_PLAYER:
    case EGameAction::INVITE_TO_INSPECT_PLAYER:
    case EGameAction::ALLOW_INSPECT_PLAYER:
    case EGameAction::REVOKE_INSPECT_PLAYER:
        return true;
    default:
        return false;
    }
}

}

TGameActionChatChannel::TGameActionChatChannel(const EGameAction type,
                                               std::shared_ptr<const chat::TChatChannelIdentifier> identifier)
    : TGameActionSimple(type)
    , m_identifier(std::move(identifier))
{
}

TGameActionOpenMessageToChatChannel::TGameActionOpenMessageToChatChannel(const QString& name)
    : TGameActionChatChannel(EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL,
                             std::make_shared<const chat::TChatChannelIdentifier>(chat::TChatChannelIdentifier::messageTo(name)))
{
}

std::shared_ptr<TGameActionRequestOutfitsFromServer> TGameActionRequestOutfitsFromServer::forHireling(const uint32_t creatureId)
{
    auto action = std::make_shared<TGameActionRequestOutfitsFromServer>();
    action->m_mode = 2;
    action->m_creatureId = creatureId;
    return action;
}

std::shared_ptr<TGameActionOpenNewsDialog> TGameActionOpenNewsDialog::withNews(const uint32_t newsId)
{
    auto action = std::make_shared<TGameActionOpenNewsDialog>();
    action->m_newsId = newsId;
    return action;
}

std::shared_ptr<TGameActionOpenNewsDialog> TGameActionOpenNewsDialog::withCategory(const uint32_t category)
{
    auto action = std::make_shared<TGameActionOpenNewsDialog>();
    action->m_category = category;
    return action;
}

namespace gameactions {

std::optional<bool> TGameActionSoundTrigger::pauseLoopingSounds() const
{
    if (const bool* pause = std::get_if<bool>(&m_trigger))
        return *pause;
    return std::nullopt;
}

std::optional<int> TGameActionSoundTrigger::soundEffectId() const
{
    if (const TSoundEffect* effect = std::get_if<TSoundEffect>(&m_trigger))
        return effect->id;
    return std::nullopt;
}

std::shared_ptr<const chat::TChatChannelIdentifier> TGameActionSoundTrigger::soundEffectChatChannel() const
{
    if (const TSoundEffect* effect = std::get_if<TSoundEffect>(&m_trigger))
        return effect->chatChannel;
    return nullptr;
}

}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setRaceId(const uint16_t raceId)
{
    m_raceId = raceId;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setCoordinate(const Position& coordinate)
{
    m_coordinate = coordinate;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setZoom(const float zoom)
{
    m_zoom = zoom;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setCategory(const int category)
{
    m_category = category;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setTypeId(const uint16_t typeId)
{
    m_typeId = typeId;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setHouseId(const uint32_t houseId)
{
    m_houseId = houseId;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setSpellId(const uint16_t spellId)
{
    m_spellId = spellId;
    return *this;
}

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setCharmId(const uint8_t charmId)
{
    m_charmId = charmId;
    return *this;
}

namespace gameactions {

}

TObjectGameActionContext::TObjectGameActionContext(const Position& position, const int stackPosition,
                                                   const uint16_t objectClientId, const uint32_t creatureId)
    : m_location(locationContextFromPosition(position, stackPosition))
{
    if (creatureId)
        m_object = std::make_shared<TObjectCreatureGameActionContext>(creatureId);
    else if (objectClientId)
        m_object = std::make_shared<TObjectApperanceTypeGameActionContext>(objectClientId);
}

uint32_t creatureOfObject(const TObjectGameActionContext& object)
{
    if (const uint32_t creatureId = object.creatureId())
        return creatureId;
    const auto* field = gameActionContextAs<TLocationWorldMapCoordinateWithStackPositionContext>(object.location());
    worldmap::TWorldMapStorage* map = field ? worldmap::TWorldMapStorage::instance() : nullptr;
    worldmap::TWorldMapField* mapField = map ? map->fieldAt(field->coordinate()) : nullptr;
    return mapField ? mapField->stackCreatureId(field->stackPosition()) : 0;
}

IGameActionPtr makeObjectGameAction(const EGameAction type, const Position& position, const int stackPosition,
                                    const uint16_t objectClientId)
{
    return makeObjectGameAction(type, TObjectGameActionContext{ position, stackPosition, objectClientId });
}

IGameActionPtr makeObjectGameAction(const EGameAction type, const TObjectGameActionContext& context)
{
    switch (type) {
    case EGameAction::LOOK:
        return std::make_shared<TGameActionLook>(context);
    case EGameAction::USE:
        return std::make_shared<TGameActionUse>(context);
    case EGameAction::OPEN:
        return std::make_shared<TGameActionOpen>(context);
    case EGameAction::OPEN_IN_NEW_WINDOW:
        return std::make_shared<TGameActionOpenInNewWindow>(context);
    case EGameAction::WRAP:
        return std::make_shared<TGameActionWrap>(context);
    case EGameAction::UNWRAP:
        return std::make_shared<TGameActionUnwrap>(context);
    case EGameAction::ROTATE_OBJECT:
        return std::make_shared<TGameActionRotate>(context);
    case EGameAction::BROWSE_FIELD:
        return std::make_shared<TGameActionBrowseField>(context);
    case EGameAction::MOVE_UP:
        return std::make_shared<TGameActionMoveUp>(context);
    case EGameAction::CONFIGURE_SHOW_OFF_SOCKET:
        return std::make_shared<TGameActionConfigureShowOffSocket>(context);
    case EGameAction::USE_WITH:
        return std::make_shared<TGameActionUseWith>(context);
    case EGameAction::USE_ON_TARGET:
        return std::make_shared<TGameActionUseOnTarget>(context);
    case EGameAction::USE_ON_YOURSELF:
        return std::make_shared<TGameActionUseOnYourself>(context);
    case EGameAction::USE_AT_MOUSECURSOR:
        return std::make_shared<TGameActionUseAtMouseCursor>(context);
    case EGameAction::TARGET_SELECTED:
        return std::make_shared<TGameActionTargetSelected>(context);
    case EGameAction::MOVE_OBJECT:
        return std::make_shared<TGameActionMoveObject>(context);
    case EGameAction::QUICK_LOOT:
        return std::make_shared<TGameActionQuickLoot>(context);
    case EGameAction::TRADE_ITEM:
        return std::make_shared<TGameActionPlayerTradeItem>(context);
    case EGameAction::GATHER_ALL:
        return std::make_shared<gameactions::TGameActionGatherAll>(context);
    case EGameAction::INSPECT_OBJECT:
        return std::make_shared<TGameActionInspectObject>(context.location());
    case EGameAction::OPEN_PARENT_CONTAINER:
        return std::make_shared<TGameActionOpenParentContainer>(context.location());
    case EGameAction::STASH_STOW_TYPE:
        return std::make_shared<TGameActionStashStowType>(context);
    case EGameAction::STASH_STOW_CONTAINER_CONTENT:
        return std::make_shared<TGameActionStashStowContainerContent>(context);
    case EGameAction::TRY_TO_EQUIP:
        return std::make_shared<TGameActionTryToEquip>(context);
    case EGameAction::MARKET_BROWSE:
        return std::make_shared<gameactions::TGameActionMarketBrowse>(context);
    default:
        return makeGameAction(type);
    }
}

std::optional<TObjectGameActionContext> objectContextOf(const IGameAction& action)
{
    if (const auto* object = dynamic_cast<const TObjectGameAction*>(&action))
        return object->context();
    return std::nullopt;
}

uint32_t creatureIdOf(const IGameAction& action)
{
    if (const auto* creature = dynamic_cast<const TGameActionCreature*>(&action))
        return creature->creatureId();
    if (const auto* npc = dynamic_cast<const TGameActionStartTalkingToNPC*>(&action))
        return npc->creatureId();
    if (const auto* use = dynamic_cast<const TGameActionUseOnCreature*>(&action))
        return use->targetCreatureId();
    if (const auto* object = dynamic_cast<const TObjectGameAction*>(&action))
        return object->context().creatureId();
    return 0;
}

IGameActionPtr makeGameAction(const EGameAction type)
{
    switch (type) {
    case EGameAction::CONTEXT_MENU:
        return std::make_shared<TGameActionContextMenu>();
    case EGameAction::CONTEXT_MENU_SEPARATOR:
        return std::make_shared<TGameActionContextMenuSeparator>();
    case EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES:
        return std::make_shared<TGameActionLoadDefaultMinimapTilesFromResources>(QString());
    case EGameAction::TOGGLE_MOUNTED:
        return std::make_shared<TGameActionToggleMounted>();
    case EGameAction::GET_CHAT_CHANNEL_LIST:
        return std::make_shared<TGameActionGetChatChannelList>();
    case EGameAction::SHOW_EXALTATION_RESULT_DIALOG:
        return std::make_shared<TGameActionShowExaltationResultDialog>(exaltation::TExaltationResult{});
    case EGameAction::ATTACK_FIRST_TARGET:
        return std::make_shared<TGameActionAttackFirstTarget>();
    case EGameAction::ATTACK_NEXT_TARGET:
        return std::make_shared<TGameActionAttackNextTarget>();
    case EGameAction::ATTACK_PREVIOUS_TARGET:
        return std::make_shared<TGameActionAttackPreviousTarget>();
    case EGameAction::CHANGE_CHARACTER:
        return std::make_shared<TGameActionChangeCharacter>();
    case EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW:
        return std::make_shared<TGameActionClearOldestMessageFromMapwindow>();
    case EGameAction::CLOSE_CURRENT_CHAT_CHANNEL:
        return std::make_shared<TGameActionCloseCurrentChatChannel>();
    case EGameAction::DISPLAY_NEXT_CHAT_CHANNEL_TAB:
        return std::make_shared<TGameActionDisplayNextChatChannelTab>();
    case EGameAction::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB:
        return std::make_shared<TGameActionDisplayPreviousChatChannelTab>();
    case EGameAction::GET_GUILD_MESSAGE:
        return std::make_shared<TGameActionGetGuildMessage>();
    case EGameAction::MOUNT_PLAYER:
        return std::make_shared<TGameActionMountPlayer>();
    case EGameAction::OPEN_ADD_VIP_DIALOG:
        return std::make_shared<TGameActionOpenAddVipDialog>();
    case EGameAction::OPEN_ADD_VIP_GROUP_DIALOG:
        return std::make_shared<TGameActionOpenAddVipGroupDialog>();
    case EGameAction::OPEN_HIGHSCORES_DIALOG:
        return std::make_shared<TGameActionOpenHighscoresDialog>();
    case EGameAction::OPEN_PREY_DIALOG:
        return std::make_shared<TGameActionOpenPreyDialog>();
    case EGameAction::OPEN_TRADE_CHARACTER_DIALOG:
        return std::make_shared<TGameActionOpenTradeCharacterDialog>();
    case EGameAction::REMOVE_ALL_TUTORIAL_MARKERS:
        return std::make_shared<TGameActionRemoveAllTutorialMarkers>();
    case EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
        return std::make_shared<TGameActionRequestCopySelectedTextToClipboard>();
    case EGameAction::REQUEST_SELECT_ALL_TEXT:
        return std::make_shared<TGameActionSelectAllText>();
    case EGameAction::SESSIONDUMP_PAUSE:
        return std::make_shared<TGameActionSessiondumpPause>();
    case EGameAction::SESSIONDUMP_RESUME:
        return std::make_shared<TGameActionSessiondumpResume>();
    case EGameAction::START_TARGET_SELECTION:
        return std::make_shared<TGameActionStartTargetSelection>();
    case EGameAction::TAKE_MANUAL_SCREENSHOT:
        return std::make_shared<TGameActionTakeManualScreenshot>();
    case EGameAction::TOGGLE_CREATURE_HUDS_VISIBLE:
        return std::make_shared<TGameActionToggleCreatureHudsVisible>();
    case EGameAction::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL:
        return std::make_shared<TGameActionToggleServerLogSubscriptionInCurrentChatChannel>();
    case EGameAction::UNMOUNT_PLAYER:
        return std::make_shared<TGameActionUnmountPlayer>();
    case EGameAction::ALLOW_ALL_TO_INSPECT_PLAYER:
    case EGameAction::APPLY_AUTOCHASE_SETTING:
    case EGameAction::CANCEL_CROSSHAIR_SPELL:
    case EGameAction::CANCEL_USE_WITH:
    case EGameAction::CHARACER_SELECTION_AUTO_CLOSE:
    case EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS:
    case EGameAction::CLOSE_CYCLOPEDIA:
    case EGameAction::CLOSE_EXALTATION_FORGE_DIALOG:
    case EGameAction::CLOSE_REWARD_WALL_DIALOG:
    case EGameAction::COLLECT_FIXED_ITEMS_DAILY_REWARD:
    case EGameAction::DAILY_REWARD_NOT_COLLECTABLE:
    case EGameAction::DAILY_REWARD_READY_TO_PICK_UP:
    case EGameAction::DEPOT_SEARCH_CLOSED:
    case EGameAction::EXALTATION_FORGE_DIALOG_CLOSED:
    case EGameAction::EXIVA_WAS_SUPPRESSED:
    case EGameAction::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES:
    case EGameAction::LEAVE_PARTY:
    case EGameAction::LENSHELP_ACTIVATE:
    case EGameAction::LENSHELP_HIDE:
    case EGameAction::OPEN_CALENDAR_DIALOG:
    case EGameAction::OPEN_EXIVA_OPTIONS_DIALOG:
    case EGameAction::OPEN_LAUNCHER:
    case EGameAction::OPEN_MANAGE_LOOT_CONTAINER:
    case EGameAction::OPEN_MANAGE_LOOT_CONTAINER_STASH:
    case EGameAction::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG:
    case EGameAction::OPEN_OR_UPDATE_REWARD_WALL_DIALOG:
    case EGameAction::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG:
    case EGameAction::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE:
    case EGameAction::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE:
    case EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE:
    case EGameAction::OPEN_URL_ACCESS_ACCOUNT:
    case EGameAction::OPEN_URL_CIPSOFT:
    case EGameAction::OPEN_URL_CREATE_ACCOUNT:
    case EGameAction::OPEN_URL_FAQ:
    case EGameAction::OPEN_URL_GET_COINS:
    case EGameAction::OPEN_URL_LOST_ACCOUNT:
    case EGameAction::OPEN_URL_MANUAL:
    case EGameAction::OPEN_URL_PREMIUM_FEATURES:
    case EGameAction::OPEN_URL_TIBIA_PAGE:
    case EGameAction::OPEN_URL_TWITCH_TIBIA:
    case EGameAction::OPEN_URL_YOUTUBE_TIBIA:
    case EGameAction::OPEN_WHITEBLACKLIST_DIALOG:
    case EGameAction::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON:
    case EGameAction::REQUEST_DAILY_REWARD_HISTORY:
    case EGameAction::REQUEST_DEPOT_SEARCH:
    case EGameAction::RESET_HUNTING_SESSION:
    case EGameAction::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES:
    case EGameAction::STASH_REQUEST_REOPEN:
    case EGameAction::STOP_ATTACKING_FOLLOWING_WALKING:
    case EGameAction::TARGET_SELECTION_FINISHED:
    case EGameAction::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER:
        return std::make_shared<gameactions::TGameActionSimple>(type);
    default:
        return std::make_shared<gameactions::TGameActionSimple>(type);
    }
}

IGameActionPtr makeCreatureAction(const EGameAction type, const uint32_t creatureId)
{
    if (type == EGameAction::START_TALKING_TO_NPC)
        return std::make_shared<TGameActionStartTalkingToNPC>(creatureId);
    if (type == EGameAction::TARGET_SELECTED)
        return std::make_shared<TGameActionTargetSelected>(TObjectGameActionContext{ {}, -1, 0, creatureId });
    if (isTGameActionCreatureType(type))
        return std::make_shared<TGameActionCreature>(type, creatureId);
    return makeGameAction(type);
}

std::shared_ptr<const TGameActionPlayerMovementIntent> makeMovementIntent(const Position& position, const bool enterUnwalkableGoal)
{
    return std::make_shared<TGameActionPlayerMovementIntent>(
        std::make_shared<TMovementIntentTargetCoordinate>(position.x, position.y, position.z, enterUnwalkableGoal));
}

std::shared_ptr<const TGameActionSmartClick> makeSmartClick(const IGameActionPtr& inner)
{
    return std::make_shared<TGameActionSmartClick>(inner);
}

TGameActionList filterActions(const TGameActionList& candidates, const std::span<const EGameAction> types)
{
    TGameActionList result;
    for (const EGameAction type : types) {
        if (const IGameActionPtr* action = firstOfType(candidates, type))
            result.push_back(*action);
    }
    return result;
}

TGameActionList contextMenuActions(const TGameActionList& candidates, const std::span<const EGameAction> order)
{
    TGameActionList result;
    for (const EGameAction type : order) {
        if (type == Separator) {
            if (!result.empty() && result.back()->gameActionType() != Separator)
                result.push_back(std::make_shared<TGameActionContextMenuSeparator>());
        } else if (const IGameActionPtr* action = firstOfType(candidates, type)) {
            result.push_back(*action);
        }
    }
    while (!result.empty() && result.back()->gameActionType() == Separator)
        result.pop_back();
    return result;
}

IGameActionPtr finalizeDrop(const IGameActionPtr& move, const Position& destination, const int modifiers, const bool defaultIsMoveAll)
{
    const auto* moved = gameActionAs<TGameActionMoveObject>(move);
    if (!moved)
        return nullptr;
    const TObjectGameActionContext& object = moved->context();
    if (moved->moveAll() || object.position() == destination)
        return std::make_shared<TGameActionMoveObject>(object, destination, moved->moveAll(), moved->amount());
    // Creatures are not stacks — never open the amount dialog for one.
    if (creatureOfObject(object))
        return std::make_shared<TGameActionMoveObject>(object, destination, false, moved->amount());
    if (modifiers & Qt::ShiftModifier)
        return std::make_shared<TGameActionMoveObject>(object, destination, true, moved->amount());
    auto action = std::make_shared<TGameActionMoveObject>(object, destination, false, moved->amount());
    if (((modifiers & Qt::ControlModifier) != 0) == defaultIsMoveAll)
        return std::make_shared<TGameActionShowSplitStackDialog>(action);
    return action;
}

}
