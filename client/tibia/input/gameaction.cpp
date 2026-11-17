#include "gameaction.h"

#include "chat/chatchannel.h"
#include "protocol/protocolcodes.h"

#include <algorithm>

#include <QLoggingCategory>

namespace tibia::input {

namespace {

constexpr EGameAction Separator = EGameAction::CONTEXT_MENU_SEPARATOR;

// CIP's context menu table .
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

const IGameActionPtr* firstOfType(const TGameActionList& candidates, const EGameAction type)
{
    const auto it = std::find_if(candidates.begin(), candidates.end(),
                                 [type](const IGameActionPtr& action) { return action && action->gameActionType() == type; });
    return it == candidates.end() ? nullptr : &*it;
}

}

namespace {

Q_LOGGING_CATEGORY(lcGameAction, "tibia.input.gameaction")

/// The field carrier for a type with no class of its own yet.
TGameActionPtr unportedSimpleAction(const EGameAction type)
{
    auto action = std::make_shared<TGameAction>();
    action->type = type;
    return action;
}

void fillObject(TGameAction& action, const Position& position, const int stackPosition,
                const uint16_t objectClientId = 0)
{
    action.position = position;
    action.stackPosition = stackPosition;
    action.objectClientId = objectClientId;
}

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

TGameActionSetCurrentChatChannelInputText& TGameActionSetCurrentChatChannelInputText::setSpell(const bool isSpell,
                                                                                               const int spellCastOptionalTargetType)
{
    m_isSpell = isSpell;
    m_spellCastOptionalTargetType = spellCastOptionalTargetType;
    return *this;
}

TGameActionUseWith::TGameActionUseWith(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::USE_WITH;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionUseWith::TGameActionUseWith(const TGameAction& other)
    : TGameAction(other)
{
    type = EGameAction::USE_WITH;
}


TGameActionMoveObject::TGameActionMoveObject(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::MOVE_OBJECT;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionMoveObject::TGameActionMoveObject(const TGameAction& other)
    : TGameAction(other)
{
    type = EGameAction::MOVE_OBJECT;
}

TGameActionPlayerMovementIntent::TGameActionPlayerMovementIntent(Position pos, const int pathMode, const bool enterUnwalkable)
{
    type = EGameAction::SET_PLAYER_MOVEMENT_INTENT;
    position = pos;
    autoWalkPathMode = pathMode;
    enterUnwalkableGoal = enterUnwalkable;
}

TGameActionQuickLoot::TGameActionQuickLoot(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::QUICK_LOOT;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionPlayerTradeItem::TGameActionPlayerTradeItem(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::TRADE_ITEM;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionSmartClick::TGameActionSmartClick(IGameActionPtr innerAction)
{
    type = EGameAction::SMART_CLICK;
    inner = std::move(innerAction);
}

TGameActionUseAtMouseCursor::TGameActionUseAtMouseCursor(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::USE_AT_MOUSECURSOR;
    fillObject(*this, position, stackPosition, objectClientId);
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


TGameActionBuyHirelingNameOrSexChange::TGameActionBuyHirelingNameOrSexChange(const uint32_t id)
{
    type = EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE;
    creatureId = id;
}

TGameActionCyclopediaMapSelectionItem::TGameActionCyclopediaMapSelectionItem(const int subareaId)
{
    type = EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM;
    instance = subareaId;
}

TGameActionForMinimapMarkerUsingCoordinate::TGameActionForMinimapMarkerUsingCoordinate(const EGameAction actionType)
{
    type = actionType;
}

TGameActionForMinimapMarkerUsingFullMarkerData::TGameActionForMinimapMarkerUsingFullMarkerData(const EGameAction actionType)
{
    type = actionType;
}

TGameActionInspectObject::TGameActionInspectObject(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::INSPECT_OBJECT;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionManagedContainer::TGameActionManagedContainer(const EGameAction actionType)
{
    type = actionType;
}

TGameActionOpenParentContainer::TGameActionOpenParentContainer(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::OPEN_PARENT_CONTAINER;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionOpenShowImageDialog::TGameActionOpenShowImageDialog(QString value)
{
    type = EGameAction::OPEN_SHOW_IMAGE_DIALOG;
    text = std::move(value);
}

TGameActionPreySystem::TGameActionPreySystem(const EGameAction actionType)
{
    type = actionType;
}

TGameActionRequestOutfitsFromServer::TGameActionRequestOutfitsFromServer(const uint32_t id)
{
    type = EGameAction::REQUEST_OUTFITS_FROM_SERVER;
    creatureId = id;
}

TGameActionSetHirelingName::TGameActionSetHirelingName(QString value)
{
    type = EGameAction::SET_HIRELING_NAME;
    text = std::move(value);
}

TGameActionShowConfirmMoveObjectToStashDialog::TGameActionShowConfirmMoveObjectToStashDialog(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionShowSplitStackDialog::TGameActionShowSplitStackDialog(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::SHOW_SPLIT_STACK_DIALOG;
    fillObject(*this, position, stackPosition, objectClientId);
}


TGameActionStashOpenStowStackDialog::TGameActionStashOpenStowStackDialog(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::STASH_OPEN_STOW_STACK_DIALOG;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionStashStowContainerContent::TGameActionStashStowContainerContent(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::STASH_STOW_CONTAINER_CONTENT;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionStashStowStack::TGameActionStashStowStack(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::STASH_STOW_STACK;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionTargetSelected::TGameActionTargetSelected(const uint32_t id)
{
    type = EGameAction::TARGET_SELECTED;
    creatureId = id;
}

TGameActionTryToEquip::TGameActionTryToEquip(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::TRY_TO_EQUIP;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionUseOnCreature::TGameActionUseOnCreature(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::USE_ON_CREATURE;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionUseOnTarget::TGameActionUseOnTarget(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::USE_ON_TARGET;
    fillObject(*this, position, stackPosition, objectClientId);
}

TGameActionUseOnYourself::TGameActionUseOnYourself(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::USE_ON_YOURSELF;
    fillObject(*this, position, stackPosition, objectClientId);
}

namespace gameactions {

TGameActionGatherAll::TGameActionGatherAll(Position position, int stackPosition, const uint16_t objectClientId)
{
    type = EGameAction::GATHER_ALL;
    fillObject(*this, position, stackPosition, objectClientId);
}

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

TGameActionOpenCyclopedia& TGameActionOpenCyclopedia::setCharmId(const uint8_t charmId)
{
    m_charmId = charmId;
    return *this;
}

namespace gameactions {

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
    case EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON:
        return std::make_shared<TGameActionAssignSpellToActionButton>();
    case EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE:
        return std::make_shared<TGameActionChangeAutoPreyExtendType>();
    case EGameAction::CHANGE_CONTEXT_MENU_ENABLED:
        return std::make_shared<TGameActionChangeContextMenuEnabled>();
    case EGameAction::CHANGE_QUESTTRACKER_OPTIONS:
        return std::make_shared<TGameActionChangeQuesttrackerOptions>();
    case EGameAction::CHANGE_TRACKED_QUESTFLAGS:
        return std::make_shared<TGameActionChangeTrackedQuestFlags>();
    case EGameAction::CHANGE_TUTORIAL_MODE_ENABLED:
        return std::make_shared<TGameActionChangeTutorialModeEnabled>();
    case EGameAction::LOOT_CONTAINER_CLEAR:
        return std::make_shared<TGameActionClearLootContainer>();
    case EGameAction::OBTAIN_CONTAINER_CLEAR:
        return std::make_shared<TGameActionClearObtainContainer>();
    case EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM:
        return std::make_shared<TGameActionCyclopediaMapSelectionItem>();
    case EGameAction::DELETE_MINIMAP_MARKER:
        return std::make_shared<TGameActionDeleteMinimapMarker>();
    case EGameAction::DEPOT_SERACH_RETRIEVE:
        return std::make_shared<TGameActionDepotSearchRetrieve>();
    case EGameAction::EXALTATION_FORGE_CONVERGENCE_FUSION:
        return std::make_shared<TGameActionExaltationForgeConvergenceFusion>();
    case EGameAction::EXALTATION_FORGE_FUSION:
        return std::make_shared<TGameActionExaltationForgeFusion>();
    case EGameAction::EXALTATION_FORGE_TRANSFER:
        return std::make_shared<TGameActionExaltationForgeTransfer>();
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
    case EGameAction::INSPECT_OBJECT:
        return std::make_shared<TGameActionInspectObject>();
    case EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES:
        return std::make_shared<TGameActionLoadDefaultMinimapTilesFromResources>();
    case EGameAction::LOOT_CONTAINER_OPEN:
        return std::make_shared<TGameActionOpenLootContainer>();
    case EGameAction::OBTAIN_CONTAINER_OPEN:
        return std::make_shared<TGameActionOpenObtainContainer>();
    case EGameAction::OPEN_PARENT_CONTAINER:
        return std::make_shared<TGameActionOpenParentContainer>();
    case EGameAction::OPEN_SHOW_IMAGE_DIALOG:
        return std::make_shared<TGameActionOpenShowImageDialog>();
    case EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE:
        return std::make_shared<TGameActionRemoveSpriteIdentifierFromCache>();
    case EGameAction::REQUEST_OUTFITS_FROM_SERVER:
        return std::make_shared<TGameActionRequestOutfitsFromServer>();
    case EGameAction::REQUEST_SEARCH_TYPE_IN_DEPOT:
        return std::make_shared<TGameActionRequestSearchTypeInDepot>();
    case EGameAction::REQUEST_TRANSACTION_DETAILS:
        return std::make_shared<TGameActionRequestTransactionDetailsFromServer>();
    case EGameAction::RESOLVE_SPRITE:
        return std::make_shared<TGameActionResolveSprite>();
    case EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY:
        return std::make_shared<TGameActionSelectPreyMonsterFromBestiary>();
    case EGameAction::PREY_SELECT_MONSTER_FROM_LIST:
        return std::make_shared<TGameActionSelectPreyMonsterFromList>();
    case EGameAction::SESSIONDUMP_SKIP_TO:
        return std::make_shared<TGameActionSessiondumpSkipTo>();
    case EGameAction::SESSIONDUMP_START:
        return std::make_shared<TGameActionSessiondumpStart>();
    case EGameAction::SET_CREATURE_PODIUM_CONFIGURATION:
        return std::make_shared<TGameActionSetCreaturePodiumConfiguration>();
    case EGameAction::SET_HIRELING_NAME:
        return std::make_shared<TGameActionSetHirelingName>();
    case EGameAction::SET_HIRELING_OUTFIT:
        return std::make_shared<TGameActionSetHirelingOutfit>();
    case EGameAction::LOOT_CONTAINER_FALLBACK:
        return std::make_shared<TGameActionSetLootContainerFallback>();
    case EGameAction::SET_PLAYER_OUTFIT:
        return std::make_shared<TGameActionSetPlayerOutfit>();
    case EGameAction::SET_SHOW_OFF_SOCKET_CONFIGURATION:
        return std::make_shared<TGameActionSetShowOffSocketConfiguration>();
    case EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG:
        return std::make_shared<TGameActionShowConfirmMoveObjectToStashDialog>();
    case EGameAction::SHOW_EXALTATION_RESULT_DIALOG:
        return std::make_shared<TGameActionShowExaltationResultDialog>();
    case EGameAction::SHOW_SPLIT_STACK_DIALOG:
        return std::make_shared<TGameActionShowSplitStackDialog>();
    case EGameAction::SHOW_TUTORIAL_HINT:
        return std::make_shared<TGameActionShowTutorialHint>();
    case EGameAction::SPRITE_RESOLVED:
        return std::make_shared<TGameActionSpriteResolved>();
    case EGameAction::STASH_OPEN_RETRIEVE_DIALOG:
        return std::make_shared<TGameActionStashOpenRetrieveDialog>();
    case EGameAction::STASH_OPEN_STOW_STACK_DIALOG:
        return std::make_shared<TGameActionStashOpenStowStackDialog>();
    case EGameAction::STASH_REQUEST_REOPEN:
        return unportedSimpleAction(EGameAction::STASH_REQUEST_REOPEN);
    case EGameAction::STASH_RETRIEVE:
        return std::make_shared<TGameActionStashRetrieve>();
    case EGameAction::STASH_STOW_CONTAINER_CONTENT:
        return std::make_shared<TGameActionStashStowContainerContent>();
    case EGameAction::STASH_STOW_STACK:
        return std::make_shared<TGameActionStashStowStack>();
    case EGameAction::STASH_STOW_TYPE:
        return std::make_shared<TGameActionStashStowType>();
    case EGameAction::TARGET_SELECTED:
        return std::make_shared<TGameActionTargetSelected>();
    case EGameAction::GOD_TELEPORT:
        return std::make_shared<TGameActionTeleportToCoordinate>();
    case EGameAction::TRY_TO_EQUIP:
        return std::make_shared<TGameActionTryToEquip>();
    case EGameAction::CONFIRM_TUTORIAL_HINT:
        return std::make_shared<TGameActionTutorialHintConfirmed>();
    case EGameAction::TUTORIAL_PROGRESS:
        return std::make_shared<TGameActionTutorialProgress>();
    case EGameAction::USE_ON_CREATURE:
        return std::make_shared<TGameActionUseOnCreature>();
    case EGameAction::USE_ON_TARGET:
        return std::make_shared<TGameActionUseOnTarget>();
    case EGameAction::USE_ON_YOURSELF:
        return std::make_shared<TGameActionUseOnYourself>();
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
    case EGameAction::GEM_ATELIER_SELECT_GEM:
        return std::make_shared<gameactions::TSelectAtelierGemGameAction>();
    case EGameAction::FRAGMENT_WORKSHOP_SELECT_MOD:
        return std::make_shared<gameactions::TSelectFragmentWorkshopModGameAction>();
    case EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER:
        return std::make_shared<TGameActionFinishedTutorialForCharacter>();
    case EGameAction::OPEN_CYCLOPEDIA:
    case EGameAction::LENSHELP_SHOW:
    case EGameAction::SOUND_TRIGGER:
    case EGameAction::START_TALKING_TO_NPC:
        // Ported classes are constructed with their fields, not by type.
        qCCritical(lcGameAction) << "makeAction: construct" << static_cast<int>(type) << "with its class";
        return unportedSimpleAction(type);
    default:
        if (isTGameActionCreatureType(type))
            qCCritical(lcGameAction) << "makeAction: construct" << static_cast<int>(type) << "with TGameActionCreature";
        return unportedSimpleAction(type);
    }
}

TGameActionPtr makeObjectAction(const EGameAction type, const Position& position, const int stackPosition, const uint16_t objectClientId)
{
    switch (type) {
    case EGameAction::USE_WITH: {
        auto action = std::make_shared<TGameActionUseWith>(position, stackPosition, objectClientId);
        return action;
    }
    case EGameAction::USE_ON_CREATURE:
        return std::make_shared<TGameActionUseOnCreature>(position, stackPosition, objectClientId);
    case EGameAction::USE_ON_TARGET:
        return std::make_shared<TGameActionUseOnTarget>(position, stackPosition, objectClientId);
    case EGameAction::USE_ON_YOURSELF:
        return std::make_shared<TGameActionUseOnYourself>(position, stackPosition, objectClientId);
    case EGameAction::USE_AT_MOUSECURSOR:
        return std::make_shared<TGameActionUseAtMouseCursor>(position, stackPosition, objectClientId);
    case EGameAction::MOVE_OBJECT:
        return std::make_shared<TGameActionMoveObject>(position, stackPosition, objectClientId);
    case EGameAction::QUICK_LOOT:
        return std::make_shared<TGameActionQuickLoot>(position, stackPosition, objectClientId);
    case EGameAction::TRADE_ITEM:
        return std::make_shared<TGameActionPlayerTradeItem>(position, stackPosition, objectClientId);
    case EGameAction::GATHER_ALL:
        return std::make_shared<gameactions::TGameActionGatherAll>(position, stackPosition, objectClientId);
    case EGameAction::INSPECT_OBJECT:
        return std::make_shared<TGameActionInspectObject>(position, stackPosition, objectClientId);
    case EGameAction::OPEN_PARENT_CONTAINER:
        return std::make_shared<TGameActionOpenParentContainer>(position, stackPosition, objectClientId);
    case EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG:
        return std::make_shared<TGameActionShowConfirmMoveObjectToStashDialog>(position, stackPosition, objectClientId);
    case EGameAction::SHOW_SPLIT_STACK_DIALOG:
        return std::make_shared<TGameActionShowSplitStackDialog>(position, stackPosition, objectClientId);
    case EGameAction::STASH_OPEN_STOW_STACK_DIALOG:
        return std::make_shared<TGameActionStashOpenStowStackDialog>(position, stackPosition, objectClientId);
    case EGameAction::STASH_STOW_CONTAINER_CONTENT:
        return std::make_shared<TGameActionStashStowContainerContent>(position, stackPosition, objectClientId);
    case EGameAction::STASH_STOW_STACK:
        return std::make_shared<TGameActionStashStowStack>(position, stackPosition, objectClientId);
    case EGameAction::TRY_TO_EQUIP:
        return std::make_shared<TGameActionTryToEquip>(position, stackPosition, objectClientId);
    default: {
        // Unknown object-shaped enums still keep the typed makeAction carrier.
        auto action = std::const_pointer_cast<TGameAction>(makeAction(type));
        fillObject(*action, position, stackPosition, objectClientId);
        return action;
    }
    }
}

IGameActionPtr makeObjectGameAction(const EGameAction type, const Position& position, const int stackPosition,
                                    const uint16_t objectClientId)
{
    const TObjectGameActionContext context{ position, stackPosition, objectClientId };
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
    default:
        return makeObjectAction(type, position, stackPosition, objectClientId);
    }
}

std::optional<TObjectGameActionContext> objectContextOf(const IGameAction& action)
{
    if (const auto* object = dynamic_cast<const TObjectGameAction*>(&action))
        return object->context();
    if (const auto* fields = dynamic_cast<const TGameAction*>(&action))
        return TObjectGameActionContext{ fields->position, fields->stackPosition, fields->objectClientId };
    return std::nullopt;
}

std::shared_ptr<TGameAction> mutableClone(const IGameAction& action)
{
    if (const auto* fields = dynamic_cast<const TGameAction*>(&action))
        return mutableClone(*fields);
    qCCritical(lcGameAction) << "mutableClone: action" << static_cast<int>(action.gameActionType()) << "has its class";
    auto carrier = std::make_shared<TGameAction>();
    carrier->type = action.gameActionType();
    return carrier;
}

uint32_t creatureIdOf(const IGameAction& action)
{
    if (const auto* creature = dynamic_cast<const TGameActionCreature*>(&action))
        return creature->creatureId();
    if (const auto* npc = dynamic_cast<const TGameActionStartTalkingToNPC*>(&action))
        return npc->creatureId();
    if (const auto* fields = dynamic_cast<const TGameAction*>(&action))
        return fields->creatureId;
    return 0;
}

IGameActionPtr makeGameAction(const EGameAction type)
{
    switch (type) {
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
        return makeAction(type);
    }
}

IGameActionPtr makeCreatureAction(const EGameAction type, const uint32_t creatureId)
{
    if (type == EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE)
        return std::make_shared<TGameActionBuyHirelingNameOrSexChange>(creatureId);
    if (type == EGameAction::REQUEST_OUTFITS_FROM_SERVER)
        return std::make_shared<TGameActionRequestOutfitsFromServer>(creatureId);
    if (type == EGameAction::START_TALKING_TO_NPC)
        return std::make_shared<TGameActionStartTalkingToNPC>(creatureId);
    if (type == EGameAction::TARGET_SELECTED)
        return std::make_shared<TGameActionTargetSelected>(creatureId);
    if (isTGameActionCreatureType(type))
        return std::make_shared<TGameActionCreature>(type, creatureId);
    if (type == EGameAction::MOUNT_PLAYER || type == EGameAction::UNMOUNT_PLAYER || type == EGameAction::OPEN_PREY_DIALOG)
        return makeGameAction(type);
    auto action = mutableClone(*makeAction(type));
    action->creatureId = creatureId;
    return action;
}

TGameActionPtr makeMovementIntent(const Position& position, const int pathMode, const bool enterUnwalkableGoal)
{
    return std::make_shared<TGameActionPlayerMovementIntent>(position, pathMode, enterUnwalkableGoal);
}

TGameActionPtr makeSmartClick(const IGameActionPtr& inner)
{
    return std::make_shared<TGameActionSmartClick>(inner);
}

std::shared_ptr<TGameAction> mutableClone(const TGameAction& action)
{
    // Prefer objectClientId / creature factories when those fields are set so LOOK/USE /
    // ATTACK_* keep their CIP typed carriers through handler reshapes.
    // Do not key off Position::isValid — default Position(0,0,0) is "valid".
    TGameActionPtr typed;
    if (action.objectClientId || action.stackPosition >= 0)
        typed = makeObjectAction(action.type, action.position, action.stackPosition, action.objectClientId);
    else if (action.creatureId)
        typed = unportedGameAction(makeCreatureAction(action.type, action.creatureId));
    if (!typed)
        typed = unportedSimpleAction(action.type);
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
        if (const IGameActionPtr* action = firstOfType(candidates, type))
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

TGameActionPtr finalizeDrop(const IGameActionPtr& move, const Position& destination, const int modifiers, const bool defaultIsMoveAll)
{
    const auto* moved = gameActionAs<TGameAction>(move);
    if (!moved)
        return nullptr;
    auto action = std::make_shared<TGameActionMoveObject>(*moved);
    action->destination = destination;
    if (action->moveAll || action->position == destination)
        return action;
    // Creatures are not stacks — never open the amount dialog for Proto::Creature.
    if (action->creatureId || action->objectClientId == Proto::Creature)
        return action;
    if (modifiers & Qt::ShiftModifier) {
        action->moveAll = true;
        return action;
    }
    if (((modifiers & Qt::ControlModifier) != 0) == defaultIsMoveAll) {
        auto split = std::const_pointer_cast<TGameAction>(
            makeObjectAction(EGameAction::SHOW_SPLIT_STACK_DIALOG, action->position,
                             action->stackPosition, action->objectClientId));
        split->destination = action->destination;
        split->amount = action->amount;
        split->moveAll = action->moveAll;
        split->inner = action;
        return split;
    }
    return action;
}

}
