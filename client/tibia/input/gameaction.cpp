#include "gameaction.h"

#include <algorithm>

namespace tibia::input {

namespace {

constexpr EGameAction Separator = EGameAction::CONTEXT_MENU_SEPARATOR;

// CIP's context menu table (FUN_100e6f820).
constexpr EGameAction ContextMenuOrder[] = {
    EGameAction::LOOK,
    EGameAction::INSPECT_OBJECT,
    EGameAction::OPEN_CYCLOPEDIA,
    EGameAction::OPEN_WEAPON_PROFICIENCY_DIALOG,
    EGameAction::USE,
    EGameAction::USE_WITH,
    EGameAction::OPEN_IN_NEW_WINDOW,
    EGameAction::WRAP,
    EGameAction::UNWRAP,
    EGameAction::ROTATE_OBJECT,
    EGameAction::CONFIGURE_SHOW_OFF_SOCKET,
    EGameAction::QUICK_LOOT,
    EGameAction::GATHER_ALL,
    EGameAction::BROWSE_FIELD,
    Separator,
    EGameAction::REPORT_BUG_MAP,
    Separator,
    EGameAction::TRADE_ITEM,
    Separator,
    EGameAction::LOOT_ADD_TO_BLACKWHITELIST,
    EGameAction::LOOT_REMOVE_FROM_BLACKWHITELIST,
    Separator,
    EGameAction::START_TALKING_TO_NPC,
    EGameAction::ATTACK_CREATURE,
    EGameAction::STOP_ATTACKING_CREATURE,
    EGameAction::FOLLOW_CREATURE,
    EGameAction::STOP_FOLLOWING_CREATURE,
    Separator,
    EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL,
    EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL,
    EGameAction::ADD_NEW_VIP,
    EGameAction::IGNORE_NAME,
    EGameAction::UNIGNORE_NAME,
    EGameAction::RECENTLY_TRADED,
    EGameAction::INVITE_TO_PARTY,
    EGameAction::JOIN_PARTY,
    EGameAction::REVOKE_PARTY_INVITATION,
    EGameAction::PASS_PARTY_LEADERSHIP,
    EGameAction::JOIN_AGGRESSION,
    EGameAction::REQUEST_OUTFITS_FROM_SERVER,
    EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE,
    EGameAction::MOUNT_PLAYER,
    EGameAction::UNMOUNT_PLAYER,
    EGameAction::OPEN_PREY_DIALOG,
    EGameAction::SET_SHARED_EXPERIENCE,
    EGameAction::LEAVE_PARTY,
    EGameAction::INSPECT_PLAYER,
    EGameAction::ASK_TO_INSPECT_PLAYER,
    EGameAction::INVITE_TO_INSPECT_PLAYER,
    EGameAction::ALLOW_INSPECT_PLAYER,
    EGameAction::REVOKE_INSPECT_PLAYER,
    Separator,
    EGameAction::REPORT_NAME,
    EGameAction::REPORT_BOT,
    EGameAction::REPORT_HIRELING_NAME,
    EGameAction::GET_TEXT_FOR_REPORT,
    Separator,
    EGameAction::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
};

const TGameActionPtr* firstOfType(const TGameActionList& candidates, const EGameAction type)
{
    const auto it = std::find_if(candidates.begin(), candidates.end(),
                                 [type](const TGameActionPtr& action) { return action && action->type == type; });
    return it == candidates.end() ? nullptr : &*it;
}

}

namespace {

void fillObject(TGameAction& action, const ThingPtr& object, const Position& position, const int stackPosition)
{
    action.object = object;
    action.position = position;
    action.stackPosition = stackPosition;
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

TGameActionLook::TGameActionLook(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::LOOK;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUse::TGameActionUse(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUseWith::TGameActionUseWith(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE_WITH;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUseWith::TGameActionUseWith(const TGameAction& other)
    : TGameAction(other)
{
    type = EGameAction::USE_WITH;
}

TGameActionCreature::TGameActionCreature(const EGameAction actionType, const uint32_t id)
{
    type = actionType;
    creatureId = id;
}

TGameActionMoveObject::TGameActionMoveObject(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::MOVE_OBJECT;
    fillObject(*this, object, position, stackPosition);
}

TGameActionMoveObject::TGameActionMoveObject(const TGameAction& other)
    : TGameAction(other)
{
    type = EGameAction::MOVE_OBJECT;
}

TGameActionPlayerMovementIntent::TGameActionPlayerMovementIntent(Position pos, const int pathMode)
{
    type = EGameAction::SET_PLAYER_MOVEMENT_INTENT;
    position = pos;
    autoWalkPathMode = pathMode;
}

TGameActionOpen::TGameActionOpen(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::OPEN;
    fillObject(*this, object, position, stackPosition);
}

TGameActionOpenInNewWindow::TGameActionOpenInNewWindow(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::OPEN_IN_NEW_WINDOW;
    fillObject(*this, object, position, stackPosition);
}

TGameActionWrap::TGameActionWrap(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::WRAP;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUnwrap::TGameActionUnwrap(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::UNWRAP;
    fillObject(*this, object, position, stackPosition);
}

TGameActionRotate::TGameActionRotate(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::ROTATE_OBJECT;
    fillObject(*this, object, position, stackPosition);
}

TGameActionQuickLoot::TGameActionQuickLoot(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::QUICK_LOOT;
    fillObject(*this, object, position, stackPosition);
}

TGameActionPlayerTradeItem::TGameActionPlayerTradeItem(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::TRADE_ITEM;
    fillObject(*this, object, position, stackPosition);
}

TGameActionSmartClick::TGameActionSmartClick(TGameActionPtr innerAction)
{
    type = EGameAction::SMART_CLICK;
    inner = std::move(innerAction);
}

TGameActionUseAtMouseCursor::TGameActionUseAtMouseCursor(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE_AT_MOUSECURSOR;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUseAtMouseCursor::TGameActionUseAtMouseCursor(const TGameAction& other)
    : TGameAction(other)
{
    type = EGameAction::USE_AT_MOUSECURSOR;
}

TGameActionActionButtonTriggered::TGameActionActionButtonTriggered(const int barId, const int buttonId,
                                                                   const int multiIndex)
{
    type = EGameAction::ACTION_BUTTON_TRIGGERED;
    actionBarId = barId;
    actionButtonId = buttonId;
    multiActionIndex = multiIndex;
}


TGameActionAddNewVip::TGameActionAddNewVip(QString name)
{
    type = EGameAction::ADD_NEW_VIP;
    text = std::move(name);
}

TGameActionAddTextMessage::TGameActionAddTextMessage(const EGameAction actionType)
{
    type = actionType;
}

TGameActionBrowseField::TGameActionBrowseField(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::BROWSE_FIELD;
    fillObject(*this, object, position, stackPosition);
}

TGameActionBuyHirelingNameOrSexChange::TGameActionBuyHirelingNameOrSexChange(const uint32_t id)
{
    type = EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE;
    creatureId = id;
}

TGameActionChatChannel::TGameActionChatChannel(const EGameAction actionType)
{
    type = actionType;
}

TGameActionChatChannel::TGameActionChatChannel(const EGameAction actionType, const int channelType, const int channelId,
                                               QString name)
{
    type = actionType;
    chatChannelType = channelType;
    chatChannelId = channelId;
    text = std::move(name);
}

TGameActionChatChannelModeration::TGameActionChatChannelModeration(const EGameAction actionType)
{
    type = actionType;
}

TGameActionChatChannelModeration::TGameActionChatChannelModeration(const EGameAction actionType, QString playerName)
{
    type = actionType;
    text = std::move(playerName);
}

TGameActionConfigureShowOffSocket::TGameActionConfigureShowOffSocket(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::CONFIGURE_SHOW_OFF_SOCKET;
    fillObject(*this, object, position, stackPosition);
}

TGameActionCopyChatEntryToClipboard::TGameActionCopyChatEntryToClipboard(QString value)
{
    type = EGameAction::COPY_CHAT_ENTRY_TO_CLIPBOARD;
    text = std::move(value);
}

TGameActionCopyCreatureNameToClipboard::TGameActionCopyCreatureNameToClipboard(QString name)
{
    type = EGameAction::COPY_CREATURE_NAME_TO_CLIPBOARD;
    text = std::move(name);
}

TGameActionCopyTextToClipboard::TGameActionCopyTextToClipboard(QString value)
{
    type = EGameAction::COPY_TEXT_TO_CLIPBOARD;
    text = std::move(value);
}

TGameActionCopyTrustedTextToClipboard::TGameActionCopyTrustedTextToClipboard(QString value)
{
    type = EGameAction::COPY_TRUSTED_TEXT_TO_CLIPBOARD;
    text = std::move(value);
}

TGameActionCreatureWithName::TGameActionCreatureWithName(const EGameAction actionType)
{
    type = actionType;
}

TGameActionCreatureWithName::TGameActionCreatureWithName(const EGameAction actionType, QString name)
{
    type = actionType;
    text = std::move(name);
}

TGameActionCyclopediaMapSelectionItem::TGameActionCyclopediaMapSelectionItem(const int subareaId)
{
    type = EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM;
    instance = subareaId;
}

TGameActionExcludePlayerFromChatChannel::TGameActionExcludePlayerFromChatChannel(QString playerName)
{
    type = EGameAction::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL;
    text = std::move(playerName);
}

TGameActionForMinimapMarkerUsingCoordinate::TGameActionForMinimapMarkerUsingCoordinate(const EGameAction actionType)
{
    type = actionType;
}

TGameActionForMinimapMarkerUsingFullMarkerData::TGameActionForMinimapMarkerUsingFullMarkerData(const EGameAction actionType)
{
    type = actionType;
}

TGameActionGetTextForReport::TGameActionGetTextForReport(QString value)
{
    type = EGameAction::GET_TEXT_FOR_REPORT;
    text = std::move(value);
}

TGameActionIgnoreName::TGameActionIgnoreName(QString value)
{
    type = EGameAction::IGNORE_NAME;
    text = std::move(value);
}

TGameActionInspectObject::TGameActionInspectObject(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::INSPECT_OBJECT;
    fillObject(*this, object, position, stackPosition);
}

TGameActionInvitePlayerToChatChannel::TGameActionInvitePlayerToChatChannel(QString value)
{
    type = EGameAction::INVITE_PLAYER_TO_CHAT_CHANNEL;
    text = std::move(value);
}

TGameActionInvitePlayerToPrivateChatChannel::TGameActionInvitePlayerToPrivateChatChannel(QString value)
{
    type = EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL;
    text = std::move(value);
}

TGameActionManagedContainer::TGameActionManagedContainer(const EGameAction actionType)
{
    type = actionType;
}

TGameActionMountPlayer::TGameActionMountPlayer(const uint32_t id)
{
    type = EGameAction::MOUNT_PLAYER;
    creatureId = id;
}

TGameActionMoveUp::TGameActionMoveUp(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::MOVE_UP;
    fillObject(*this, object, position, stackPosition);
}

TGameActionOpenEditVipDialog::TGameActionOpenEditVipDialog(QString value)
{
    type = EGameAction::OPEN_EDIT_VIP_DIALOG;
    text = std::move(value);
}

TGameActionOpenEditVipGroupDialog::TGameActionOpenEditVipGroupDialog(QString value)
{
    type = EGameAction::OPEN_EDIT_VIP_GROUP_DIALOG;
    text = std::move(value);
}

TGameActionOpenMessageToChatChannel::TGameActionOpenMessageToChatChannel(QString value)
{
    type = EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL;
    text = std::move(value);
}

TGameActionOpenParentContainer::TGameActionOpenParentContainer(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::OPEN_PARENT_CONTAINER;
    fillObject(*this, object, position, stackPosition);
}

TGameActionOpenPreyDialog::TGameActionOpenPreyDialog(const uint32_t id)
{
    type = EGameAction::OPEN_PREY_DIALOG;
    creatureId = id;
}

TGameActionOpenShowImageDialog::TGameActionOpenShowImageDialog(QString value)
{
    type = EGameAction::OPEN_SHOW_IMAGE_DIALOG;
    text = std::move(value);
}

TGameActionOpenUrl::TGameActionOpenUrl(QString value)
{
    type = EGameAction::OPEN_URL;
    text = std::move(value);
}

TGameActionPreySystem::TGameActionPreySystem(const EGameAction actionType)
{
    type = actionType;
}

TGameActionRecentlyTraded::TGameActionRecentlyTraded(QString value)
{
    type = EGameAction::RECENTLY_TRADED;
    text = std::move(value);
}

TGameActionReportCharacter::TGameActionReportCharacter(QString value)
{
    type = EGameAction::REPORT_NAME;
    text = std::move(value);
}

TGameActionReportHireling::TGameActionReportHireling(const uint32_t id)
{
    type = EGameAction::REPORT_HIRELING_NAME;
    creatureId = id;
}

TGameActionReportStatement::TGameActionReportStatement(QString value)
{
    type = EGameAction::REPORT_STATEMENT;
    text = std::move(value);
}

TGameActionReportText::TGameActionReportText(QString value)
{
    type = EGameAction::REPORT_TEXT;
    text = std::move(value);
}

TGameActionRequestConformationForCopyingTextToClipboard::TGameActionRequestConformationForCopyingTextToClipboard(QString value)
{
    type = EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD;
    text = std::move(value);
}

TGameActionRequestOutfitsFromServer::TGameActionRequestOutfitsFromServer(const uint32_t id)
{
    type = EGameAction::REQUEST_OUTFITS_FROM_SERVER;
    creatureId = id;
}

TGameActionSaveJsonDocumentToFile::TGameActionSaveJsonDocumentToFile(QString path)
{
    type = EGameAction::SAVE_JSON_TO_FILE;
    filePath = std::move(path);
}

TGameActionSaveTextToFile::TGameActionSaveTextToFile(QString path)
{
    type = EGameAction::SAVE_TEXT_TO_FILE;
    filePath = std::move(path);
}

TGameActionSendChatMessage::TGameActionSendChatMessage(const EGameAction actionType)
{
    type = actionType;
}

TGameActionSendChatMessageToNpc::TGameActionSendChatMessageToNpc(QString value)
{
    type = EGameAction::SEND_CHAT_MESSAGE_TO_NPC;
    text = std::move(value);
}

TGameActionSendChatMessageWithRecipient::TGameActionSendChatMessageWithRecipient(QString value)
{
    type = EGameAction::SEND_CHAT_MESSAGE_WITH_RECIPIENT;
    text = std::move(value);
}

TGameActionSendChatMessageWithSpeechVolume::TGameActionSendChatMessageWithSpeechVolume(QString value)
{
    type = EGameAction::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME;
    text = std::move(value);
}

TGameActionSetCurrentChatChannelInputText::TGameActionSetCurrentChatChannelInputText(QString value)
{
    type = EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT;
    text = std::move(value);
}

TGameActionSetHirelingName::TGameActionSetHirelingName(QString value)
{
    type = EGameAction::SET_HIRELING_NAME;
    text = std::move(value);
}

TGameActionSetStatusLine::TGameActionSetStatusLine(QString value)
{
    type = EGameAction::SET_STATUS_LINE;
    text = std::move(value);
}

TGameActionShowConfirmMoveObjectToStashDialog::TGameActionShowConfirmMoveObjectToStashDialog(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG;
    fillObject(*this, object, position, stackPosition);
}

TGameActionShowMessageDialog::TGameActionShowMessageDialog(const EGameAction actionType)
{
    type = actionType;
}

TGameActionShowSplitStackDialog::TGameActionShowSplitStackDialog(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::SHOW_SPLIT_STACK_DIALOG;
    fillObject(*this, object, position, stackPosition);
}

TGameActionSortFilterVipList::TGameActionSortFilterVipList(const EGameAction actionType)
{
    type = actionType;
}

TGameActionStartTalkingToNPC::TGameActionStartTalkingToNPC(const uint32_t id)
{
    type = EGameAction::START_TALKING_TO_NPC;
    creatureId = id;
}

TGameActionStashOpenStowStackDialog::TGameActionStashOpenStowStackDialog(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::STASH_OPEN_STOW_STACK_DIALOG;
    fillObject(*this, object, position, stackPosition);
}

TGameActionStashStowContainerContent::TGameActionStashStowContainerContent(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::STASH_STOW_CONTAINER_CONTENT;
    fillObject(*this, object, position, stackPosition);
}

TGameActionStashStowStack::TGameActionStashStowStack(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::STASH_STOW_STACK;
    fillObject(*this, object, position, stackPosition);
}

TGameActionTargetSelected::TGameActionTargetSelected(const uint32_t id)
{
    type = EGameAction::TARGET_SELECTED;
    creatureId = id;
}

TGameActionToggleBlackWhitelistStatus::TGameActionToggleBlackWhitelistStatus(const EGameAction actionType)
{
    type = actionType;
}

TGameActionTryToEquip::TGameActionTryToEquip(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::TRY_TO_EQUIP;
    fillObject(*this, object, position, stackPosition);
}

TGameActionTryToSwitchHotkeySet::TGameActionTryToSwitchHotkeySet(QString value)
{
    type = EGameAction::TRY_TO_SWITCH_HOTKEYSET;
    text = std::move(value);
}

TGameActionUnignoreName::TGameActionUnignoreName(QString value)
{
    type = EGameAction::UNIGNORE_NAME;
    text = std::move(value);
}

TGameActionUnmountPlayer::TGameActionUnmountPlayer(const uint32_t id)
{
    type = EGameAction::UNMOUNT_PLAYER;
    creatureId = id;
}

TGameActionUseOnCreature::TGameActionUseOnCreature(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE_ON_CREATURE;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUseOnTarget::TGameActionUseOnTarget(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE_ON_TARGET;
    fillObject(*this, object, position, stackPosition);
}

TGameActionUseOnYourself::TGameActionUseOnYourself(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::USE_ON_YOURSELF;
    fillObject(*this, object, position, stackPosition);
}

TGameActionVip::TGameActionVip(const EGameAction actionType)
{
    type = actionType;
}

TGameActionVipGroup::TGameActionVipGroup(const EGameAction actionType)
{
    type = actionType;
}

TGameActionVisibleChatChannel::TGameActionVisibleChatChannel(const EGameAction actionType)
{
    type = actionType;
}

namespace gameactions {

TGameActionGatherAll::TGameActionGatherAll(ThingPtr object, Position position, int stackPosition)
{
    type = EGameAction::GATHER_ALL;
    fillObject(*this, object, position, stackPosition);
}

gameactions::TGameActionSimple::TGameActionSimple(const EGameAction actionType)
{
    type = actionType;
}

}

TGameActionPtr makeAction(const EGameAction type)
{
    switch (type) {
    case EGameAction::SET_PLAYER_MOVEMENT_INTENT:
        return std::make_shared<TGameActionPlayerMovementIntent>();
    case EGameAction::CONTEXT_MENU:
        return std::make_shared<TGameActionContextMenu>();
    case EGameAction::CONTEXT_MENU_SEPARATOR:
        return std::make_shared<TGameActionContextMenuSeparator>();
    case EGameAction::CROSSHAIR_SPELL:
        return std::make_shared<TGameActionCrossHairSpell>();
    case EGameAction::SPELL_AT_MOUSE_CURSOR:
        return std::make_shared<TGameActionSpellAtMouseCursor>();
    case EGameAction::ACTION_BUTTON_TRIGGERED:
        return std::make_shared<TGameActionActionButtonTriggered>();
    case EGameAction::SMART_CLICK:
        return std::make_shared<TGameActionSmartClick>();
    case EGameAction::ADD_GUI_TUTORIAL_MARKER:
        return std::make_shared<TGameActionAddGUITutorialMarker>();
    case EGameAction::ADD_HINT_AT_TUTORIAL_MARKER:
        return std::make_shared<TGameActionAddHintAtTutorialMarker>();
    case EGameAction::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER:
        return std::make_shared<TGameActionAddMouseTrailHintAtTutorialMarker>();
    case EGameAction::ADD_NEW_VIP:
        return std::make_shared<TGameActionAddNewVip>();
    case EGameAction::ADD_NEW_VIP_GROUP:
        return std::make_shared<TGameActionAddNewVipGroup>();
    case EGameAction::ADD_SYSTEM_CHAT_MESSAGE:
    case EGameAction::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE:
        return std::make_shared<TGameActionAddTextMessage>(type);
    case EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON:
        return std::make_shared<TGameActionAssignSpellToActionButton>();
    case EGameAction::ATTACK_FIRST_TARGET:
        return std::make_shared<TGameActionAttackFirstTarget>();
    case EGameAction::ATTACK_NEXT_TARGET:
        return std::make_shared<TGameActionAttackNextTarget>();
    case EGameAction::ATTACK_PREVIOUS_TARGET:
        return std::make_shared<TGameActionAttackPreviousTarget>();
    case EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE:
        return std::make_shared<TGameActionChangeAutoPreyExtendType>();
    case EGameAction::CHANGE_CHARACTER:
        return std::make_shared<TGameActionChangeCharacter>();
    case EGameAction::CHANGE_CONTEXT_MENU_ENABLED:
        return std::make_shared<TGameActionChangeContextMenuEnabled>();
    case EGameAction::CHANGE_QUESTTRACKER_OPTIONS:
        return std::make_shared<TGameActionChangeQuesttrackerOptions>();
    case EGameAction::CHANGE_TRACKED_QUESTFLAGS:
        return std::make_shared<TGameActionChangeTrackedQuestFlags>();
    case EGameAction::CHANGE_TUTORIAL_MODE_ENABLED:
        return std::make_shared<TGameActionChangeTutorialModeEnabled>();
    case EGameAction::OPEN_CHAT_CHANNEL:
    case EGameAction::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT:
    case EGameAction::CLOSE_CHAT_CHANNEL:
    case EGameAction::CLEAR_CHAT_CHANNEL:
    case EGameAction::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG:
    case EGameAction::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG:
    case EGameAction::CHAT_CHANNEL_MUTE:
    case EGameAction::CHAT_CHANNEL_UNMUTE:
    case EGameAction::SAVE_CHAT_CHANNEL_TO_FILE:
    case EGameAction::CHAT_CHANNEL_SELECT_ALL:
    case EGameAction::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES:
    case EGameAction::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES:
        return std::make_shared<TGameActionChatChannel>(type);
    case EGameAction::LOOT_CONTAINER_CLEAR:
        return std::make_shared<TGameActionClearLootContainer>();
    case EGameAction::OBTAIN_CONTAINER_CLEAR:
        return std::make_shared<TGameActionClearObtainContainer>();
    case EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW:
        return std::make_shared<TGameActionClearOldestMessageFromMapwindow>();
    case EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS:
        return std::make_shared<TGameActionCloseAllMultiActionButtons>();
    case EGameAction::CLOSE_CURRENT_CHAT_CHANNEL:
        return std::make_shared<TGameActionCloseCurrentChatChannel>();
    case EGameAction::COLLECT_PICK_ITEMS_DAILY_REWARD:
        return std::make_shared<TGameActionCollectPickItemsDailyReward>();
    case EGameAction::COPY_CHAT_ENTRY_TO_CLIPBOARD:
        return std::make_shared<TGameActionCopyChatEntryToClipboard>();
    case EGameAction::COPY_CREATURE_NAME_TO_CLIPBOARD:
        return std::make_shared<TGameActionCopyCreatureNameToClipboard>();
    case EGameAction::COPY_TEXT_TO_CLIPBOARD:
        return std::make_shared<TGameActionCopyTextToClipboard>();
    case EGameAction::COPY_TRUSTED_TEXT_TO_CLIPBOARD:
        return std::make_shared<TGameActionCopyTrustedTextToClipboard>();
    case EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM:
        return std::make_shared<TGameActionCyclopediaMapSelectionItem>();
    case EGameAction::DELETE_MINIMAP_MARKER:
        return std::make_shared<TGameActionDeleteMinimapMarker>();
    case EGameAction::DEPOT_SERACH_RETRIEVE:
        return std::make_shared<TGameActionDepotSearchRetrieve>();
    case EGameAction::DISPLAY_NEXT_CHAT_CHANNEL_TAB:
        return std::make_shared<TGameActionDisplayNextChatChannelTab>();
    case EGameAction::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB:
        return std::make_shared<TGameActionDisplayPreviousChatChannelTab>();
    case EGameAction::EXALTATION_FORGE_CONVERGENCE_FUSION:
        return std::make_shared<TGameActionExaltationForgeConvergenceFusion>();
    case EGameAction::EXALTATION_FORGE_FUSION:
        return std::make_shared<TGameActionExaltationForgeFusion>();
    case EGameAction::EXALTATION_FORGE_TRANSFER:
        return std::make_shared<TGameActionExaltationForgeTransfer>();
    case EGameAction::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL:
        return std::make_shared<TGameActionExcludePlayerFromChatChannel>();
    case EGameAction::SHOW_DIALOG_CREATE_MINIMAP_MARKER:
        return std::make_shared<TGameActionShowDialogCreateMinimapMarker>();
    case EGameAction::UPDATE_MINIMAP_MARKER:
        return std::make_shared<TGameActionUpdateMinimapMarker>();
    case EGameAction::SHOW_DIALOG_EDIT_MINIMAP_MARKER:
        return std::make_shared<TGameActionShowDialogEditMinimapMarker>();
    case EGameAction::LOOT_CONTAINER_SELECT:
        return std::make_shared<TGameActionSelectLootContainer>();
    case EGameAction::OBTAIN_CONTAINER_SELECT:
        return std::make_shared<TGameActionSelectObtainContainer>();
    case EGameAction::PREY_REQUEST_LIST_REROLL:
        return std::make_shared<TGameActionPreySystem>(type);
    case EGameAction::PREY_REQUEST_BONUS_REROLL:
        return std::make_shared<TGameActionPreySystem>(type);
    case EGameAction::PREY_REQUEST_FREEZE_AND_SHOW_BESTIARY_MONSTERS:
        return std::make_shared<TGameActionPreySystem>(type);
    case EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT:
        return std::make_shared<TGameActionFriendsDialogSearchAccount>();
    case EGameAction::GET_CHAT_CHANNEL_LIST:
        return std::make_shared<TGameActionGetChatChannelList>();
    case EGameAction::GET_GUILD_MESSAGE:
        return std::make_shared<TGameActionGetGuildMessage>();
    case EGameAction::GET_TEXT_FOR_REPORT:
        return std::make_shared<TGameActionGetTextForReport>();
    case EGameAction::IGNORE_NAME:
        return std::make_shared<TGameActionIgnoreName>();
    case EGameAction::INSPECT_OBJECT:
        return std::make_shared<TGameActionInspectObject>();
    case EGameAction::INVITE_PLAYER_TO_CHAT_CHANNEL:
        return std::make_shared<TGameActionInvitePlayerToChatChannel>();
    case EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL:
        return std::make_shared<TGameActionInvitePlayerToPrivateChatChannel>();
    case EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES:
        return std::make_shared<TGameActionLoadDefaultMinimapTilesFromResources>();
    case EGameAction::MAKE_BATTLE_LIST_PRIMARY:
        return std::make_shared<TGameActionMakeBattleListPrimary>();
    case EGameAction::MOUNT_PLAYER:
        return std::make_shared<TGameActionMountPlayer>();
    case EGameAction::MOVE_UP:
        return std::make_shared<TGameActionMoveUp>();
    case EGameAction::OPEN_ADD_VIP_DIALOG:
        return std::make_shared<TGameActionOpenAddVipDialog>();
    case EGameAction::OPEN_ADD_VIP_GROUP_DIALOG:
        return std::make_shared<TGameActionOpenAddVipGroupDialog>();
    case EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR:
        return std::make_shared<TGameActionOpenContainerInSpecificSidebar>();
    case EGameAction::OPEN_CYCLOPEDIA:
        return std::make_shared<TGameActionOpenCyclopedia>();
    case EGameAction::OPEN_EDIT_VIP_DIALOG:
        return std::make_shared<TGameActionOpenEditVipDialog>();
    case EGameAction::OPEN_EDIT_VIP_GROUP_DIALOG:
        return std::make_shared<TGameActionOpenEditVipGroupDialog>();
    case EGameAction::OPEN_URL_GET_PREMIUM:
        return std::make_shared<TGameActionOpenGetPremiumUrl>();
    case EGameAction::OPEN_HIGHSCORES_DIALOG:
        return std::make_shared<TGameActionOpenHighscoresDialog>();
    case EGameAction::LOOT_CONTAINER_OPEN:
        return std::make_shared<TGameActionOpenLootContainer>();
    case EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL:
        return std::make_shared<TGameActionOpenMessageToChatChannel>();
    case EGameAction::OPEN_NEWS_DIALOG:
        return std::make_shared<TGameActionOpenNewsDialog>();
    case EGameAction::OPEN_NEWS_CATEGORY:
        return std::make_shared<TGameActionOpenNewsDialogWithCategory>();
    case EGameAction::OBTAIN_CONTAINER_OPEN:
        return std::make_shared<TGameActionOpenObtainContainer>();
    case EGameAction::OPEN_OPTIONS_MENU_DIALOG:
        return std::make_shared<TGameActionOpenOptionsMenuDialog>();
    case EGameAction::OPEN_PARENT_CONTAINER:
        return std::make_shared<TGameActionOpenParentContainer>();
    case EGameAction::OPEN_PREY_DIALOG:
        return std::make_shared<TGameActionOpenPreyDialog>();
    case EGameAction::OPEN_QUESTLOG_DIALOG:
        return std::make_shared<TGameActionOpenQuestLog>();
    case EGameAction::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR:
        return std::make_shared<TGameActionOpenSecondaryBattleListInSpecificSidebar>();
    case EGameAction::OPEN_SHOW_IMAGE_DIALOG:
        return std::make_shared<TGameActionOpenShowImageDialog>();
    case EGameAction::OPEN_SOCIAL_DIALOG:
        return std::make_shared<TGameActionOpenSocialDialog>();
    case EGameAction::OPEN_STORE:
        return std::make_shared<TGameActionOpenStore>();
    case EGameAction::OPEN_URL_SURVEY:
        return std::make_shared<TGameActionOpenSurveyUrl>();
    case EGameAction::OPEN_TRADE_CHARACTER_DIALOG:
        return std::make_shared<TGameActionOpenTradeCharacterDialog>();
    case EGameAction::OPEN_URL:
        return std::make_shared<TGameActionOpenUrl>();
    case EGameAction::QUIT_GAME:
        return std::make_shared<TGameActionQuitGame>();
    case EGameAction::RECENTLY_TRADED:
        return std::make_shared<TGameActionRecentlyTraded>();
    case EGameAction::REFRESH_COMPENDIUM_NEWS:
        return std::make_shared<TGameActionRefreshCompendiumNews>();
    case EGameAction::REMOVE_ALL_TUTORIAL_MARKERS:
        return std::make_shared<TGameActionRemoveAllTutorialMarkers>();
    case EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE:
        return std::make_shared<TGameActionRemoveSpriteIdentifierFromCache>();
    case EGameAction::REMOVE_VIP:
        return std::make_shared<TGameActionRemoveVip>();
    case EGameAction::REMOVE_VIP_GROUP:
        return std::make_shared<TGameActionRemoveVipGroup>();
    case EGameAction::REPORT_NAME:
        return std::make_shared<TGameActionReportCharacter>();
    case EGameAction::REPORT_HIRELING_NAME:
        return std::make_shared<TGameActionReportHireling>();
    case EGameAction::REPORT_BUG_MAP:
        return std::make_shared<TGameActionReportMapBug>();
    case EGameAction::REPORT_STATEMENT:
        return std::make_shared<TGameActionReportStatement>();
    case EGameAction::REPORT_TEXT:
        return std::make_shared<TGameActionReportText>();
    case EGameAction::REPORT_BUG_TYPO:
        return std::make_shared<TGameActionReportTypo>();
    case EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD:
        return std::make_shared<TGameActionRequestConformationForCopyingTextToClipboard>();
    case EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
        return std::make_shared<TGameActionRequestCopySelectedTextToClipboard>();
    case EGameAction::REQUEST_OUTFITS_FROM_SERVER:
        return std::make_shared<TGameActionRequestOutfitsFromServer>();
    case EGameAction::REQUEST_RESOURCE_BALANCE:
        return std::make_shared<TGameActionRequestResourceBalance>();
    case EGameAction::REQUEST_SEARCH_TYPE_IN_DEPOT:
        return std::make_shared<TGameActionRequestSearchTypeInDepot>();
    case EGameAction::REQUEST_TRANSACTION_DETAILS:
        return std::make_shared<TGameActionRequestTransactionDetailsFromServer>();
    case EGameAction::REQUST_VIP_DATA_UPDATE:
        return std::make_shared<TGameActionRequestVipDataUpdate>();
    case EGameAction::REQUEST_VIP_GROUP_UPDATE:
        return std::make_shared<TGameActionRequestVipGroupUpdate>();
    case EGameAction::RESOLVE_SPRITE:
        return std::make_shared<TGameActionResolveSprite>();
    case EGameAction::SAVE_JSON_TO_FILE:
        return std::make_shared<TGameActionSaveJsonDocumentToFile>();
    case EGameAction::SAVE_TEXT_TO_FILE:
        return std::make_shared<TGameActionSaveTextToFile>();
    case EGameAction::REQUEST_SELECT_ALL_TEXT:
        return std::make_shared<TGameActionSelectAllText>();
    case EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY:
        return std::make_shared<TGameActionSelectPreyMonsterFromBestiary>();
    case EGameAction::PREY_SELECT_MONSTER_FROM_LIST:
        return std::make_shared<TGameActionSelectPreyMonsterFromList>();
    case EGameAction::PRESS_ENTER_IN_CHAT:
        return std::make_shared<TGameActionSendChatMessage>(type);
    case EGameAction::SEND_CHAT_MESSAGE_TO_NPC:
        return std::make_shared<TGameActionSendChatMessageToNpc>();
    case EGameAction::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID:
        return std::make_shared<TGameActionSendChatMessageWithChannelID>();
    case EGameAction::SEND_CHAT_MESSAGE_WITH_RECIPIENT:
        return std::make_shared<TGameActionSendChatMessageWithRecipient>();
    case EGameAction::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME:
        return std::make_shared<TGameActionSendChatMessageWithSpeechVolume>();
    case EGameAction::SESSIONDUMP_PAUSE:
        return std::make_shared<TGameActionSessiondumpPause>();
    case EGameAction::SESSIONDUMP_RESUME:
        return std::make_shared<TGameActionSessiondumpResume>();
    case EGameAction::SESSIONDUMP_SKIP_TO:
        return std::make_shared<TGameActionSessiondumpSkipTo>();
    case EGameAction::SESSIONDUMP_START:
        return std::make_shared<TGameActionSessiondumpStart>();
    case EGameAction::SET_CHASE_OPPONENTS:
        return std::make_shared<TGameActionSetChaseMode>();
    case EGameAction::SET_CREATURE_PODIUM_CONFIGURATION:
        return std::make_shared<TGameActionSetCreaturePodiumConfiguration>();
    case EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT:
        return std::make_shared<TGameActionSetCurrentChatChannelInputText>();
    case EGameAction::SET_HIRELING_NAME:
        return std::make_shared<TGameActionSetHirelingName>();
    case EGameAction::SET_HIRELING_OUTFIT:
        return std::make_shared<TGameActionSetHirelingOutfit>();
    case EGameAction::LOOT_CONTAINER_FALLBACK:
        return std::make_shared<TGameActionSetLootContainerFallback>();
    case EGameAction::SET_PLAYER_OUTFIT:
        return std::make_shared<TGameActionSetPlayerOutfit>();
    case EGameAction::SET_PVP_MODE:
        return std::make_shared<TGameActionSetPvPMode>();
    case EGameAction::SET_SECURE_MODE:
        return std::make_shared<TGameActionSetSecureMode>();
    case EGameAction::SET_SHARED_EXPERIENCE:
        return std::make_shared<TGameActionSetSharedExperience>();
    case EGameAction::SET_SHOW_OFF_SOCKET_CONFIGURATION:
        return std::make_shared<TGameActionSetShowOffSocketConfiguration>();
    case EGameAction::SET_STATUS_LINE:
        return std::make_shared<TGameActionSetStatusLine>();
    case EGameAction::SET_TACTICS:
        return std::make_shared<TGameActionSetTactics>();
    case EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG:
        return std::make_shared<TGameActionShowConfirmMoveObjectToStashDialog>();
    case EGameAction::SHOW_EXALTATION_RESULT_DIALOG:
        return std::make_shared<TGameActionShowExaltationResultDialog>();
    case EGameAction::LENSHELP_SHOW:
        return std::make_shared<TGameActionShowLenshelp>();
    case EGameAction::SHOW_MESSAGE_YOU_SEE_NOTHING:
    case EGameAction::SHOW_INVENTORY_CAPACITY_WARNING:
    case EGameAction::SHOW_OFFLINE_TRAINING_SKILLS:
        return std::make_shared<TGameActionShowMessageDialog>(type);
    case EGameAction::SHOW_NAGGING_DIALOG:
        return std::make_shared<TGameActionShowNaggingDialog>();
    case EGameAction::SHOW_SPLIT_STACK_DIALOG:
        return std::make_shared<TGameActionShowSplitStackDialog>();
    case EGameAction::SHOW_TUTORIAL_HINT:
        return std::make_shared<TGameActionShowTutorialHint>();
    case EGameAction::SORT_VIPS_BY_NAME:
    case EGameAction::SORT_VIPS_BY_ICON:
    case EGameAction::SORT_VIPS_BY_STATUS:
    case EGameAction::HIDE_OFFLINE_VIPS:
    case EGameAction::SHOW_OFFLINE_VIPS:
    case EGameAction::SHOW_VIPS_GROUPS:
    case EGameAction::HIDE_VIPS_GROUPS:
        return std::make_shared<TGameActionSortFilterVipList>(type);
    case EGameAction::SPRITE_RESOLVED:
        return std::make_shared<TGameActionSpriteResolved>();
    case EGameAction::START_TALKING_TO_NPC:
        return std::make_shared<TGameActionStartTalkingToNPC>();
    case EGameAction::START_TARGET_SELECTION:
        return std::make_shared<TGameActionStartTargetSelection>();
    case EGameAction::STASH_OPEN_RETRIEVE_DIALOG:
        return std::make_shared<TGameActionStashOpenRetrieveDialog>();
    case EGameAction::STASH_OPEN_STOW_STACK_DIALOG:
        return std::make_shared<TGameActionStashOpenStowStackDialog>();
    case EGameAction::STASH_RETRIEVE:
        return std::make_shared<TGameActionStashRetrieve>();
    case EGameAction::STASH_STOW_CONTAINER_CONTENT:
        return std::make_shared<TGameActionStashStowContainerContent>();
    case EGameAction::STASH_STOW_STACK:
        return std::make_shared<TGameActionStashStowStack>();
    case EGameAction::STASH_STOW_TYPE:
        return std::make_shared<TGameActionStashStowType>();
    case EGameAction::SWITCH_CHAT_MODE:
        return std::make_shared<TGameActionSwitchChatMode>();
    case EGameAction::SWITCH_CLIENTOPTIONS_MODE:
        return std::make_shared<TGameActionSwitchClientOptions>();
    case EGameAction::TAKE_AUTOMATIC_SCREENSHOT:
        return std::make_shared<TGameActionTakeAutomaticScreenshot>();
    case EGameAction::TAKE_MANUAL_SCREENSHOT:
        return std::make_shared<TGameActionTakeManualScreenshot>();
    case EGameAction::TARGET_SELECTED:
        return std::make_shared<TGameActionTargetSelected>();
    case EGameAction::GOD_TELEPORT:
        return std::make_shared<TGameActionTeleportToCoordinate>();
    case EGameAction::TOGGLE_ACTION_BAR:
        return std::make_shared<TGameActionToggleActionBar>();
    case EGameAction::TOGGLE_ACTION_BARS:
        return std::make_shared<TGameActionToggleActionBars>();
    case EGameAction::LOOT_ADD_TO_BLACKWHITELIST:
    case EGameAction::LOOT_REMOVE_FROM_BLACKWHITELIST:
        return std::make_shared<TGameActionToggleBlackWhitelistStatus>(type);
    case EGameAction::TOGGLE_CREATURE_HUDS_VISIBLE:
        return std::make_shared<TGameActionToggleCreatureHudsVisible>();
    case EGameAction::TOGGLE_MOUNTED:
        return std::make_shared<TGameActionToggleMounted>();
    case EGameAction::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL:
        return std::make_shared<TGameActionToggleServerLogSubscriptionInCurrentChatChannel>();
    case EGameAction::TOGGLE_WIDGET:
        return std::make_shared<TGameActionToggleWidget>();
    case EGameAction::TRY_TO_EQUIP:
        return std::make_shared<TGameActionTryToEquip>();
    case EGameAction::TRY_TO_SWITCH_HOTKEYSET:
        return std::make_shared<TGameActionTryToSwitchHotkeySet>();
    case EGameAction::CONFIRM_TUTORIAL_HINT:
        return std::make_shared<TGameActionTutorialHintConfirmed>();
    case EGameAction::TUTORIAL_PROGRESS:
        return std::make_shared<TGameActionTutorialProgress>();
    case EGameAction::UNIGNORE_NAME:
        return std::make_shared<TGameActionUnignoreName>();
    case EGameAction::UNMOUNT_PLAYER:
        return std::make_shared<TGameActionUnmountPlayer>();
    case EGameAction::EXIVA_UPADATE_OPTIONS:
        return std::make_shared<TGameActionUpdateExivaOptions>();
    case EGameAction::USE_ON_CREATURE:
        return std::make_shared<TGameActionUseOnCreature>();
    case EGameAction::USE_ON_TARGET:
        return std::make_shared<TGameActionUseOnTarget>();
    case EGameAction::USE_ON_YOURSELF:
        return std::make_shared<TGameActionUseOnYourself>();
    case EGameAction::SHOW_CHAT_CHANNEL_AS_SECONDARY:
    case EGameAction::SELECT_CHAT_CHANNEL_AS_SECONDARY:
    case EGameAction::HIDE_SECONDARY_CHAT_CHANNEL:
        return std::make_shared<TGameActionVisibleChatChannel>(type);
    case EGameAction::GAME_EVENT:
        return std::make_shared<gameactions::TGameActionGameEvent>();
    case EGameAction::MARKET_BROWSE:
        return std::make_shared<gameactions::TGameActionMarketBrowse>();
    case EGameAction::OPEN_TASKBOARD_DIALOG:
        return std::make_shared<gameactions::TGameActionOpenTaskboardDialog>();
    case EGameAction::OPEN_WEAPON_PROFICIENCY_DIALOG:
        return std::make_shared<gameactions::TGameActionOpenWeaponProficiencyDialog>();
    case EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG:
        return std::make_shared<gameactions::TGameActionOpenWeaponProficiencyShapeDialog>();
    case EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG:
        return std::make_shared<gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog>();
    case EGameAction::SAVE_SKILL_WHEEL:
        return std::make_shared<gameactions::TGameActionSaveSkillWheel>();
    case EGameAction::SHOW_CREATE_CHARACTER_DIALOG:
        return std::make_shared<gameactions::TGameActionShowCreateCharacterDialog>();
    case EGameAction::SOUND_TRIGGER:
        return std::make_shared<gameactions::TGameActionSoundTrigger>();
    case EGameAction::GEM_ATELIER_SELECT_GEM:
        return std::make_shared<gameactions::TSelectAtelierGemGameAction>();
    case EGameAction::FRAGMENT_WORKSHOP_SELECT_MOD:
        return std::make_shared<gameactions::TSelectFragmentWorkshopModGameAction>();
    case EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER:
        return std::make_shared<TGameActionFinishedTutorialForCharacter>();
    default:
        return std::make_shared<gameactions::TGameActionSimple>(type);
    }
}

TGameActionPtr makeObjectAction(const EGameAction type, const ThingPtr& object, const Position& position, const int stackPosition)
{
    switch (type) {
    case EGameAction::LOOK:
        return std::make_shared<TGameActionLook>(object, position, stackPosition);
    case EGameAction::USE:
        return std::make_shared<TGameActionUse>(object, position, stackPosition);
    case EGameAction::USE_WITH: {
        auto action = std::make_shared<TGameActionUseWith>(object, position, stackPosition);
        return action;
    }
    case EGameAction::USE_ON_CREATURE:
        return std::make_shared<TGameActionUseOnCreature>(object, position, stackPosition);
    case EGameAction::USE_ON_TARGET:
        return std::make_shared<TGameActionUseOnTarget>(object, position, stackPosition);
    case EGameAction::USE_ON_YOURSELF:
        return std::make_shared<TGameActionUseOnYourself>(object, position, stackPosition);
    case EGameAction::USE_AT_MOUSECURSOR:
        return std::make_shared<TGameActionUseAtMouseCursor>(object, position, stackPosition);
    case EGameAction::MOVE_OBJECT:
        return std::make_shared<TGameActionMoveObject>(object, position, stackPosition);
    case EGameAction::OPEN:
        return std::make_shared<TGameActionOpen>(object, position, stackPosition);
    case EGameAction::OPEN_IN_NEW_WINDOW:
        return std::make_shared<TGameActionOpenInNewWindow>(object, position, stackPosition);
    case EGameAction::WRAP:
        return std::make_shared<TGameActionWrap>(object, position, stackPosition);
    case EGameAction::UNWRAP:
        return std::make_shared<TGameActionUnwrap>(object, position, stackPosition);
    case EGameAction::ROTATE_OBJECT:
        return std::make_shared<TGameActionRotate>(object, position, stackPosition);
    case EGameAction::QUICK_LOOT:
        return std::make_shared<TGameActionQuickLoot>(object, position, stackPosition);
    case EGameAction::TRADE_ITEM:
        return std::make_shared<TGameActionPlayerTradeItem>(object, position, stackPosition);
    case EGameAction::GATHER_ALL:
        return std::make_shared<gameactions::TGameActionGatherAll>(object, position, stackPosition);
    case EGameAction::BROWSE_FIELD:
        return std::make_shared<TGameActionBrowseField>(object, position, stackPosition);
    case EGameAction::CONFIGURE_SHOW_OFF_SOCKET:
        return std::make_shared<TGameActionConfigureShowOffSocket>(object, position, stackPosition);
    case EGameAction::INSPECT_OBJECT:
        return std::make_shared<TGameActionInspectObject>(object, position, stackPosition);
    case EGameAction::MOVE_UP:
        return std::make_shared<TGameActionMoveUp>(object, position, stackPosition);
    case EGameAction::OPEN_PARENT_CONTAINER:
        return std::make_shared<TGameActionOpenParentContainer>(object, position, stackPosition);
    case EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG:
        return std::make_shared<TGameActionShowConfirmMoveObjectToStashDialog>(object, position, stackPosition);
    case EGameAction::SHOW_SPLIT_STACK_DIALOG:
        return std::make_shared<TGameActionShowSplitStackDialog>(object, position, stackPosition);
    case EGameAction::STASH_OPEN_STOW_STACK_DIALOG:
        return std::make_shared<TGameActionStashOpenStowStackDialog>(object, position, stackPosition);
    case EGameAction::STASH_STOW_CONTAINER_CONTENT:
        return std::make_shared<TGameActionStashStowContainerContent>(object, position, stackPosition);
    case EGameAction::STASH_STOW_STACK:
        return std::make_shared<TGameActionStashStowStack>(object, position, stackPosition);
    case EGameAction::TRY_TO_EQUIP:
        return std::make_shared<TGameActionTryToEquip>(object, position, stackPosition);
    default: {
        // Unknown object-shaped enums still keep the typed makeAction carrier.
        auto action = std::const_pointer_cast<TGameAction>(makeAction(type));
        fillObject(*action, object, position, stackPosition);
        return action;
    }
    }
}

TGameActionPtr makeCreatureAction(const EGameAction type, const uint32_t creatureId)
{
    if (type == EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE)
        return std::make_shared<TGameActionBuyHirelingNameOrSexChange>(creatureId);
    if (type == EGameAction::MOUNT_PLAYER)
        return std::make_shared<TGameActionMountPlayer>(creatureId);
    if (type == EGameAction::OPEN_PREY_DIALOG)
        return std::make_shared<TGameActionOpenPreyDialog>(creatureId);
    if (type == EGameAction::REPORT_HIRELING_NAME)
        return std::make_shared<TGameActionReportHireling>(creatureId);
    if (type == EGameAction::REQUEST_OUTFITS_FROM_SERVER)
        return std::make_shared<TGameActionRequestOutfitsFromServer>(creatureId);
    if (type == EGameAction::START_TALKING_TO_NPC)
        return std::make_shared<TGameActionStartTalkingToNPC>(creatureId);
    if (type == EGameAction::TARGET_SELECTED)
        return std::make_shared<TGameActionTargetSelected>(creatureId);
    if (type == EGameAction::UNMOUNT_PLAYER)
        return std::make_shared<TGameActionUnmountPlayer>(creatureId);
    if (isCreatureActionType(type))
        return std::make_shared<TGameActionCreature>(type, creatureId);
    auto action = mutableClone(*makeAction(type));
    action->creatureId = creatureId;
    return action;
}

TGameActionPtr makeMovementIntent(const Position& position, const int pathMode)
{
    return std::make_shared<TGameActionPlayerMovementIntent>(position, pathMode);
}

TGameActionPtr makeSmartClick(const TGameActionPtr& inner)
{
    return std::make_shared<TGameActionSmartClick>(inner);
}

std::shared_ptr<TGameAction> mutableClone(const TGameAction& action)
{
    // Prefer object / creature factories when those fields are set so LOOK/USE /
    // ATTACK_* keep their CIP typed carriers through handler reshapes.
    // Do not key off Position::isValid — default Position(0,0,0) is "valid".
    TGameActionPtr typed;
    if (action.object || action.stackPosition >= 0)
        typed = makeObjectAction(action.type, action.object, action.position, action.stackPosition);
    else if (action.creatureId)
        typed = makeCreatureAction(action.type, action.creatureId);
    else
        typed = makeAction(action.type);
    auto out = std::const_pointer_cast<TGameAction>(typed);
    const EGameAction type = out->type;
    *out = action;
    out->type = type;
    return out;
}

TGameActionList filterActions(const TGameActionList& candidates, const std::span<const EGameAction> types)
{
    TGameActionList result;
    for (const EGameAction type : types) {
        if (const TGameActionPtr* action = firstOfType(candidates, type))
            result.push_back(*action);
    }
    return result;
}

TGameActionList contextMenuActions(const TGameActionList& candidates)
{
    return contextMenuActions(candidates, ContextMenuOrder);
}

TGameActionList contextMenuActions(const TGameActionList& candidates, const std::span<const EGameAction> order)
{
    TGameActionList result;
    for (const EGameAction type : order) {
        if (type == Separator) {
            if (!result.empty() && result.back()->type != Separator)
                result.push_back(std::make_shared<TGameActionContextMenuSeparator>());
        } else if (const TGameActionPtr* action = firstOfType(candidates, type)) {
            result.push_back(*action);
        }
    }
    while (!result.empty() && result.back()->type == Separator)
        result.pop_back();
    return result;
}

TGameActionPtr finalizeDrop(const TGameActionPtr& move, const Position& destination, const int modifiers, const bool defaultIsMoveAll)
{
    if (!move)
        return nullptr;
    auto action = std::make_shared<TGameActionMoveObject>(*move);
    action->destination = destination;
    if (action->moveAll || action->position == destination)
        return action;
    if (modifiers & Qt::ShiftModifier) {
        action->moveAll = true;
        return action;
    }
    if (((modifiers & Qt::ControlModifier) != 0) == defaultIsMoveAll) {
        auto split = std::const_pointer_cast<TGameAction>(
            makeObjectAction(EGameAction::SHOW_SPLIT_STACK_DIALOG, action->object, action->position,
                             action->stackPosition));
        split->destination = action->destination;
        split->amount = action->amount;
        split->moveAll = action->moveAll;
        split->inner = action;
        return split;
    }
    return action;
}

}
