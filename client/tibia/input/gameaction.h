#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>

#include <QColor>
#include <QString>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

namespace tibia {
namespace input {

using EGameAction = cip::tibia::input::EGameAction;

struct TGameAction;
using TGameActionPtr = std::shared_ptr<const TGameAction>;
using TGameActionList = std::vector<TGameActionPtr>;

/// One thing the player can do (CIP: the input::TGameAction* classes). Each
/// type uses the fields its CIP class carries. Typed subclasses are data
/// carriers constructed at the provider / makeAction sites.
struct TGameAction
{
    virtual ~TGameAction() = default;

    EGameAction type = EGameAction::UNKNOWN;
    /// Object actions: world-map / inventory field and stack position
    /// (CIP: TGameActionContext 0xb) plus the item client id for wire.
    Position position;
    int stackPosition = -1;
    /// Item client id at `position` / `stackPosition` (CIP stackpos + position wire).
    uint16_t objectClientId = 0;
    /// Creature actions.
    uint32_t creatureId = 0;
    /// SMART_CLICK: the action a click runs.
    TGameActionPtr inner;
    /// ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE.
    QString text;
    QColor color;
    /// SAVE_TEXT_TO_FILE / SAVE_JSON_TO_FILE: where `text` is written (CIP:
    /// TGameActionSaveTextToFile / TGameActionSaveJsonToFile,).
    QString filePath;
    /// REPORT_TEXT (CIP: TGameActionReportStatement,): `text` is
    /// the author, `parameter` the text's edit id and this the reported text.
    QString statement;
    /// LENSHELP_SHOW.
    int lensHelpId = 0;
    /// MOVE_OBJECT: where the object is dropped (inventory slots are
    /// Position(0xFFFF, slot, 0), slot 0 for any) and whether Shift asked
    /// for everything.
    Position destination;
    bool moveAll = false;
    /// MOVE_OBJECT: how many of a stack move (CIP , set by the
    /// split-stack dialog); 0 moves the whole stack. STASH_STOW_STACK /
    /// STASH_RETRIEVE: the count, the stack's / stored one until the amount
    /// dialog sets the chosen one (CIP).
    int amount = 0;
    /// SET_SHARED_EXPERIENCE: whether it turns shared experience on.
    bool enable = false;
    /// SET_TACTICS.
    bool chase = false;
    bool secure = false;
    int pvpMode = 0;
    /// ACTION_BUTTON_TRIGGERED (CIP: TGameActionActionButtonTriggered,
    /// ); TOGGLE_ACTION_BAR: the bar.
    int actionBarId = 0;
    int actionButtonId = 0;
    int multiActionIndex = 0;
    /// The one value of a parameterised action: TOGGLE_ACTION_BARS position, SWITCH_CHAT_MODE mode, OPEN_CYCLOPEDIA /
    /// OPEN_SOCIAL_DIALOG tab, OPEN_OPTIONS_MENU_DIALOG page (CIP).
    int parameter = 0;
    /// TOGGLE_WIDGET / OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR /
    /// OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR / MAKE_BATTLE_LIST_PRIMARY:
    /// the container or battle list id, and the sidebar (0xff: any).
    int instance = 0;
    uint sidebarId = 0xff;
    /// Repeated by a held key (CIP:).
    bool autoRepeat = false;
    /// SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT from a hotkey (CIP , action +0x21 / +0x24),
    /// CROSSHAIR_SPELL, SPELL_AT_MOUSE_CURSOR: whether `text` casts a spell, its
    /// ESpellCastOptionalTargetType, and the say's tibia::client::MessageMode.
    bool isSpell = false;
    int spellCastType = 0;
    int talkMode = 0;
    /// USE_WITH / CROSSHAIR_SPELL: the TARGET_SELECTED action picked for it (CIP);
    /// null starts a target selection.
    TGameActionPtr target;
    /// OPEN_CHAT_CHANNEL / OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT: the channel's
    /// chat::TChatChannelIdentifier type and server channel id; `text` the name.
    int chatChannelType = 0;
    int chatChannelId = 0;
    /// OPEN_CYCLOPEDIA: the tab's sub-page, -1 for none (CIP: the optional
    /// int at +0x38 of TGameActionOpenCyclopedia,). The
    /// character tab (parameter 2) takes
    /// TCharacterInfoDialogController::EDialogCategory (1: BattleMainMenu).
    /// OPEN_STORE: `parameter` is the TStoreOpenParameters open action and
    /// `instance` its detail (CIP ; open action 1 is the premium /
    /// boost page, detail 1 its XP boost).
    int category = -1;
    /// EXALTATION_FORGE_FUSION / _CONVERGENCE_FUSION / _TRANSFER (CIP: the
    /// TGameActionExaltationForge* read by): the object and its
    /// tier, the resource (fusion) or target (transfer) type, a fusion's
    /// success chance and tier loss improvements, a transfer's convergence.
    uint16_t exaltationTypeId = 0;
    uint8_t exaltationTier = 0;
    uint16_t exaltationSecondTypeId = 0;
    bool exaltationSuccessIncreased = false;
    bool exaltationTierLossImproved = false;
    bool exaltationConvergence = false;
    /// SET_PLAYER_OUTFIT / SET_HIRELING_OUTFIT / SET_SHOW_OFF_SOCKET_CONFIGURATION
    /// (CIP outfit-window OK): the chosen look, ride flag, random mount, and
    /// for a show-off socket the look direction and whether the podium shows.
    Outfit outfit;
    bool mountCharacter = false;
    bool randomMount = false;
    bool showSocket = false;
    int lookDirection = 0;
    /// REQUST_VIP_DATA_UPDATE (CIP: TGameActionRequestVipDataUpdate):
    /// `instance` is the VIP id, `text` the description, `parameter` the icon,
    /// `enable` notify-on-login, and these the selected group ids.
    std::vector<uint8_t> vipGroups;
    /// SET_PLAYER_MOVEMENT_INTENT: Minimap / Cyclopedia / Travel qtclient policy.
    int autoWalkPathMode = 0;
};

/// CIP input::TGameActionLook (v0 → LOOK).
class TGameActionLook final : public TGameAction
{
public:
    TGameActionLook() { type = EGameAction::LOOK; }
    TGameActionLook(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionUse (v0 → USE).
class TGameActionUse final : public TGameAction
{
public:
    TGameActionUse() { type = EGameAction::USE; }
    TGameActionUse(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionUseWith (ctor); also USE_ON_*.
class TGameActionUseWith final : public TGameAction
{
public:
    TGameActionUseWith() { type = EGameAction::USE_WITH; }
    TGameActionUseWith(Position position, int stackPosition, uint16_t objectClientId = 0);
    explicit TGameActionUseWith(const TGameAction& other);
};

/// CIP input::TGameActionCreature (ctor): ATTACK_CREATURE,
/// FOLLOW_CREATURE, LOOK_AT_CREATURE, stop variants, and other creature-id
/// actions share this carrier.
class TGameActionCreature final : public TGameAction
{
public:
    TGameActionCreature() = default;
    TGameActionCreature(EGameAction type, uint32_t creatureId);
};

/// CIP input::TGameActionMoveObject (ctor).
class TGameActionMoveObject final : public TGameAction
{
public:
    TGameActionMoveObject() { type = EGameAction::MOVE_OBJECT; }
    TGameActionMoveObject(Position position, int stackPosition, uint16_t objectClientId = 0);
    explicit TGameActionMoveObject(const TGameAction& other);
};

/// CIP input::TGameActionPlayerMovementIntent for
/// SET_PLAYER_MOVEMENT_INTENT.
class TGameActionPlayerMovementIntent final : public TGameAction
{
public:
    TGameActionPlayerMovementIntent() { type = EGameAction::SET_PLAYER_MOVEMENT_INTENT; }
    explicit TGameActionPlayerMovementIntent(Position position, int pathMode = 0);
};
/// CIP input::TGameActionManagedContainer .
class TGameActionManagedContainer : public TGameAction
{
public:
    TGameActionManagedContainer() = default;
    explicit TGameActionManagedContainer(EGameAction type);
};

/// CIP input::TGameActionForMinimapMarkerUsingCoordinate .
class TGameActionForMinimapMarkerUsingCoordinate : public TGameAction
{
public:
    TGameActionForMinimapMarkerUsingCoordinate() = default;
    explicit TGameActionForMinimapMarkerUsingCoordinate(EGameAction type);
};

/// CIP input::TGameActionForMinimapMarkerUsingFullMarkerData .
class TGameActionForMinimapMarkerUsingFullMarkerData : public TGameAction
{
public:
    TGameActionForMinimapMarkerUsingFullMarkerData() = default;
    explicit TGameActionForMinimapMarkerUsingFullMarkerData(EGameAction type);
};

/// CIP input::TGameActionVip .
class TGameActionVip : public TGameAction
{
public:
    TGameActionVip() = default;
    explicit TGameActionVip(EGameAction type);
};

/// CIP input::TGameActionVipGroup .
class TGameActionVipGroup : public TGameAction
{
public:
    TGameActionVipGroup() = default;
    explicit TGameActionVipGroup(EGameAction type);
};


/// CIP input::TGameActionOpen (v0 → OPEN).
class TGameActionOpen final : public TGameAction
{
public:
    TGameActionOpen() { type = EGameAction::OPEN; }
    TGameActionOpen(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionOpenInNewWindow (v0).
class TGameActionOpenInNewWindow final : public TGameAction
{
public:
    TGameActionOpenInNewWindow() { type = EGameAction::OPEN_IN_NEW_WINDOW; }
    TGameActionOpenInNewWindow(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionWrap (v0).
class TGameActionWrap final : public TGameAction
{
public:
    TGameActionWrap() { type = EGameAction::WRAP; }
    TGameActionWrap(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionUnwrap (v0).
class TGameActionUnwrap final : public TGameAction
{
public:
    TGameActionUnwrap() { type = EGameAction::UNWRAP; }
    TGameActionUnwrap(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionRotate (v0 → ROTATE_OBJECT).
class TGameActionRotate final : public TGameAction
{
public:
    TGameActionRotate() { type = EGameAction::ROTATE_OBJECT; }
    TGameActionRotate(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionQuickLoot (v0).
class TGameActionQuickLoot final : public TGameAction
{
public:
    TGameActionQuickLoot() { type = EGameAction::QUICK_LOOT; }
    TGameActionQuickLoot(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionPlayerTradeItem (v0 → TRADE_ITEM).
class TGameActionPlayerTradeItem final : public TGameAction
{
public:
    TGameActionPlayerTradeItem() { type = EGameAction::TRADE_ITEM; }
    TGameActionPlayerTradeItem(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionSmartClick (v0).
class TGameActionSmartClick final : public TGameAction
{
public:
    TGameActionSmartClick() { type = EGameAction::SMART_CLICK; }
    explicit TGameActionSmartClick(TGameActionPtr innerAction);
};

/// CIP input::TGameActionContextMenu (v0).
class TGameActionContextMenu final : public TGameAction
{
public:
    TGameActionContextMenu() { type = EGameAction::CONTEXT_MENU; }
};

/// CIP input::TGameActionContextMenuSeparator (v0).
class TGameActionContextMenuSeparator final : public TGameAction
{
public:
    TGameActionContextMenuSeparator() { type = EGameAction::CONTEXT_MENU_SEPARATOR; }
};

/// CIP input::TGameActionCrossHairSpell (v0).
class TGameActionCrossHairSpell final : public TGameAction
{
public:
    TGameActionCrossHairSpell() { type = EGameAction::CROSSHAIR_SPELL; }
};

/// CIP input::TGameActionSpellAtMouseCursor (v0).
class TGameActionSpellAtMouseCursor final : public TGameAction
{
public:
    TGameActionSpellAtMouseCursor() { type = EGameAction::SPELL_AT_MOUSE_CURSOR; }
};

/// CIP input::TGameActionUseAtMouseCursor (v0); also built from
/// TGameActionUseWith fields when a crosshair use is aimed at the cursor.
class TGameActionUseAtMouseCursor final : public TGameAction
{
public:
    TGameActionUseAtMouseCursor() { type = EGameAction::USE_AT_MOUSECURSOR; }
    TGameActionUseAtMouseCursor(Position position, int stackPosition, uint16_t objectClientId = 0);
    explicit TGameActionUseAtMouseCursor(const TGameAction& other);
};

/// CIP input::TGameActionActionButtonTriggered .
class TGameActionActionButtonTriggered final : public TGameAction
{
public:
    TGameActionActionButtonTriggered() { type = EGameAction::ACTION_BUTTON_TRIGGERED; }
    TGameActionActionButtonTriggered(int actionBarId, int actionButtonId, int multiActionIndex = -1);
};


/// CIP input::TGameActionAddGUITutorialMarker .
class TGameActionAddGUITutorialMarker final : public TGameAction
{
public:
    TGameActionAddGUITutorialMarker() { type = EGameAction::ADD_GUI_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddHintAtTutorialMarker .
class TGameActionAddHintAtTutorialMarker final : public TGameAction
{
public:
    TGameActionAddHintAtTutorialMarker() { type = EGameAction::ADD_HINT_AT_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddMouseTrailHintAtTutorialMarker .
class TGameActionAddMouseTrailHintAtTutorialMarker final : public TGameAction
{
public:
    TGameActionAddMouseTrailHintAtTutorialMarker() { type = EGameAction::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddNewVip (v0 → ADD_NEW_VIP).
class TGameActionAddNewVip final : public TGameActionVip
{
public:
    TGameActionAddNewVip() { type = EGameAction::ADD_NEW_VIP; }
    explicit TGameActionAddNewVip(QString name);
};

/// CIP input::TGameActionAddNewVipGroup .
class TGameActionAddNewVipGroup final : public TGameActionVipGroup
{
public:
    TGameActionAddNewVipGroup() { type = EGameAction::ADD_NEW_VIP_GROUP; }
};

/// CIP input::TGameActionAddTextMessage : ADD_SYSTEM_CHAT_MESSAGE /
/// ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE.
class TGameActionAddTextMessage final : public TGameAction
{
public:
    TGameActionAddTextMessage() = default;
    explicit TGameActionAddTextMessage(EGameAction type);
};

/// CIP input::TGameActionAssignSpellToActionButton .
class TGameActionAssignSpellToActionButton final : public TGameAction
{
public:
    TGameActionAssignSpellToActionButton() { type = EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON; }
};

/// CIP input::TGameActionAttackFirstTarget .
class TGameActionAttackFirstTarget final : public TGameAction
{
public:
    TGameActionAttackFirstTarget() { type = EGameAction::ATTACK_FIRST_TARGET; }
};

/// CIP input::TGameActionAttackNextTarget .
class TGameActionAttackNextTarget final : public TGameAction
{
public:
    TGameActionAttackNextTarget() { type = EGameAction::ATTACK_NEXT_TARGET; }
};

/// CIP input::TGameActionAttackPreviousTarget .
class TGameActionAttackPreviousTarget final : public TGameAction
{
public:
    TGameActionAttackPreviousTarget() { type = EGameAction::ATTACK_PREVIOUS_TARGET; }
};

/// CIP input::TGameActionBrowseField (v0 → BROWSE_FIELD).
class TGameActionBrowseField final : public TGameAction
{
public:
    TGameActionBrowseField() { type = EGameAction::BROWSE_FIELD; }
    TGameActionBrowseField(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionBuyHirelingNameOrSexChange .
class TGameActionBuyHirelingNameOrSexChange final : public TGameAction
{
public:
    TGameActionBuyHirelingNameOrSexChange() { type = EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE; }
    explicit TGameActionBuyHirelingNameOrSexChange(uint32_t creatureId);
};

/// CIP input::TGameActionChangeAutoPreyExtendType .
class TGameActionChangeAutoPreyExtendType final : public TGameAction
{
public:
    TGameActionChangeAutoPreyExtendType() { type = EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE; }
};

/// CIP input::TGameActionChangeCharacter .
class TGameActionChangeCharacter final : public TGameAction
{
public:
    TGameActionChangeCharacter() { type = EGameAction::CHANGE_CHARACTER; }
};

/// CIP input::TGameActionChangeContextMenuEnabled .
class TGameActionChangeContextMenuEnabled final : public TGameAction
{
public:
    TGameActionChangeContextMenuEnabled() { type = EGameAction::CHANGE_CONTEXT_MENU_ENABLED; }
};

/// CIP input::TGameActionChangeQuesttrackerOptions .
class TGameActionChangeQuesttrackerOptions final : public TGameAction
{
public:
    TGameActionChangeQuesttrackerOptions() { type = EGameAction::CHANGE_QUESTTRACKER_OPTIONS; }
};

/// CIP input::TGameActionChangeTrackedQuestFlags .
class TGameActionChangeTrackedQuestFlags final : public TGameAction
{
public:
    TGameActionChangeTrackedQuestFlags() { type = EGameAction::CHANGE_TRACKED_QUESTFLAGS; }
};

/// CIP input::TGameActionChangeTutorialModeEnabled .
class TGameActionChangeTutorialModeEnabled final : public TGameAction
{
public:
    TGameActionChangeTutorialModeEnabled() { type = EGameAction::CHANGE_TUTORIAL_MODE_ENABLED; }
};

/// CIP input::TGameActionChatChannel : OPEN/CLOSE/MUTE and related
/// channel-id actions share this carrier.
class TGameActionChatChannel final : public TGameAction
{
public:
    TGameActionChatChannel() = default;
    explicit TGameActionChatChannel(EGameAction type);
    TGameActionChatChannel(EGameAction type, int chatChannelType, int chatChannelId, QString name = {});
};

/// CIP input::TGameActionChatChannelModeration : invite/exclude
/// player from a channel.
class TGameActionChatChannelModeration final : public TGameAction
{
public:
    TGameActionChatChannelModeration() = default;
    explicit TGameActionChatChannelModeration(EGameAction type);
    TGameActionChatChannelModeration(EGameAction type, QString playerName);
};

/// CIP input::TGameActionClearLootContainer (→ LOOT_CONTAINER_CLEAR).
class TGameActionClearLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionClearLootContainer() { type = EGameAction::LOOT_CONTAINER_CLEAR; }
};

/// CIP input::TGameActionClearObtainContainer (→ OBTAIN_CONTAINER_CLEAR).
class TGameActionClearObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionClearObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_CLEAR; }
};

/// CIP input::TGameActionClearOldestMessageFromMapwindow .
class TGameActionClearOldestMessageFromMapwindow final : public TGameAction
{
public:
    TGameActionClearOldestMessageFromMapwindow() { type = EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW; }
};

/// CIP input::TGameActionCloseAllMultiActionButtons .
class TGameActionCloseAllMultiActionButtons final : public TGameAction
{
public:
    TGameActionCloseAllMultiActionButtons() { type = EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS; }
};

/// CIP input::TGameActionCloseCurrentChatChannel .
class TGameActionCloseCurrentChatChannel final : public TGameAction
{
public:
    TGameActionCloseCurrentChatChannel() { type = EGameAction::CLOSE_CURRENT_CHAT_CHANNEL; }
};


/// CIP input::TGameActionConfigureShowOffSocket (v0).
class TGameActionConfigureShowOffSocket final : public TGameAction
{
public:
    TGameActionConfigureShowOffSocket() { type = EGameAction::CONFIGURE_SHOW_OFF_SOCKET; }
    TGameActionConfigureShowOffSocket(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionCopyChatEntryToClipboard .
class TGameActionCopyChatEntryToClipboard final : public TGameAction
{
public:
    TGameActionCopyChatEntryToClipboard() { type = EGameAction::COPY_CHAT_ENTRY_TO_CLIPBOARD; }
    explicit TGameActionCopyChatEntryToClipboard(QString text);
};

/// CIP input::TGameActionCopyCreatureNameToClipboard .
class TGameActionCopyCreatureNameToClipboard final : public TGameAction
{
public:
    TGameActionCopyCreatureNameToClipboard() { type = EGameAction::COPY_CREATURE_NAME_TO_CLIPBOARD; }
    explicit TGameActionCopyCreatureNameToClipboard(QString name);
};

/// CIP input::TGameActionCopyTextToClipboard (v0).
class TGameActionCopyTextToClipboard final : public TGameAction
{
public:
    TGameActionCopyTextToClipboard() { type = EGameAction::COPY_TEXT_TO_CLIPBOARD; }
    explicit TGameActionCopyTextToClipboard(QString text);
};

/// CIP input::TGameActionCopyTrustedTextToClipboard .
class TGameActionCopyTrustedTextToClipboard final : public TGameAction
{
public:
    TGameActionCopyTrustedTextToClipboard() { type = EGameAction::COPY_TRUSTED_TEXT_TO_CLIPBOARD; }
    explicit TGameActionCopyTrustedTextToClipboard(QString text);
};

/// CIP input::TGameActionCreatureWithName : IGNORE/UNIGNORE,
/// OPEN_MESSAGE_TO_CHAT_CHANNEL, REPORT_NAME, RECENTLY_TRADED, and similar
/// name-keyed actions share this carrier.
class TGameActionCreatureWithName final : public TGameAction
{
public:
    TGameActionCreatureWithName() = default;
    explicit TGameActionCreatureWithName(EGameAction type);
    TGameActionCreatureWithName(EGameAction type, QString name);
};

/// CIP input::TGameActionCyclopediaMapSelectionItem .
class TGameActionCyclopediaMapSelectionItem final : public TGameAction
{
public:
    TGameActionCyclopediaMapSelectionItem() { type = EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM; }
    explicit TGameActionCyclopediaMapSelectionItem(int subareaId);
};

/// CIP input::TGameActionDeleteMinimapMarker .
class TGameActionDeleteMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    TGameActionDeleteMinimapMarker() { type = EGameAction::DELETE_MINIMAP_MARKER; }
};

/// CIP input::TGameActionDepotSearchRetrieve (→ DEPOT_SERACH_RETRIEVE).
class TGameActionDepotSearchRetrieve final : public TGameAction
{
public:
    TGameActionDepotSearchRetrieve() { type = EGameAction::DEPOT_SERACH_RETRIEVE; }
};

/// CIP input::TGameActionDisplayNextChatChannelTab .
class TGameActionDisplayNextChatChannelTab final : public TGameAction
{
public:
    TGameActionDisplayNextChatChannelTab() { type = EGameAction::DISPLAY_NEXT_CHAT_CHANNEL_TAB; }
};

/// CIP input::TGameActionDisplayPreviousChatChannelTab .
class TGameActionDisplayPreviousChatChannelTab final : public TGameAction
{
public:
    TGameActionDisplayPreviousChatChannelTab() { type = EGameAction::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB; }
};

/// CIP input::TGameActionExaltationForgeConvergenceFusion .
class TGameActionExaltationForgeConvergenceFusion final : public TGameAction
{
public:
    TGameActionExaltationForgeConvergenceFusion() { type = EGameAction::EXALTATION_FORGE_CONVERGENCE_FUSION; }
};

/// CIP input::TGameActionExaltationForgeFusion .
class TGameActionExaltationForgeFusion final : public TGameAction
{
public:
    TGameActionExaltationForgeFusion() { type = EGameAction::EXALTATION_FORGE_FUSION; }
};

/// CIP input::TGameActionExaltationForgeTransfer .
class TGameActionExaltationForgeTransfer final : public TGameAction
{
public:
    TGameActionExaltationForgeTransfer() { type = EGameAction::EXALTATION_FORGE_TRANSFER; }
};

/// CIP input::TGameActionExcludePlayerFromChatChannel (v0).
class TGameActionExcludePlayerFromChatChannel final : public TGameAction
{
public:
    TGameActionExcludePlayerFromChatChannel() { type = EGameAction::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL; }
    explicit TGameActionExcludePlayerFromChatChannel(QString playerName);
};

/// CIP input::TGameActionFinishedTutorialForCharacter .
class TGameActionFinishedTutorialForCharacter final : public TGameAction
{
public:
    TGameActionFinishedTutorialForCharacter() { type = EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER; }
};



/// CIP input::TGameActionFriendsDialogSearchAccount .
class TGameActionFriendsDialogSearchAccount final : public TGameAction
{
public:
    TGameActionFriendsDialogSearchAccount() { type = EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT; }
};

/// CIP input::TGameActionGetChatChannelList .
class TGameActionGetChatChannelList final : public TGameAction
{
public:
    TGameActionGetChatChannelList() { type = EGameAction::GET_CHAT_CHANNEL_LIST; }
};

/// CIP input::TGameActionGetGuildMessage .
class TGameActionGetGuildMessage final : public TGameAction
{
public:
    TGameActionGetGuildMessage() { type = EGameAction::GET_GUILD_MESSAGE; }
};

/// CIP input::TGameActionGetTextForReport .
class TGameActionGetTextForReport final : public TGameAction
{
public:
    TGameActionGetTextForReport() { type = EGameAction::GET_TEXT_FOR_REPORT; }
    explicit TGameActionGetTextForReport(QString text);
};

/// CIP input::TGameActionIgnoreName .
class TGameActionIgnoreName final : public TGameAction
{
public:
    TGameActionIgnoreName() { type = EGameAction::IGNORE_NAME; }
    explicit TGameActionIgnoreName(QString text);
};

/// CIP input::TGameActionInspectObject .
class TGameActionInspectObject final : public TGameAction
{
public:
    TGameActionInspectObject() { type = EGameAction::INSPECT_OBJECT; }
    TGameActionInspectObject(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionInvitePlayerToChatChannel .
class TGameActionInvitePlayerToChatChannel final : public TGameAction
{
public:
    TGameActionInvitePlayerToChatChannel() { type = EGameAction::INVITE_PLAYER_TO_CHAT_CHANNEL; }
    explicit TGameActionInvitePlayerToChatChannel(QString text);
};

/// CIP input::TGameActionInvitePlayerToPrivateChatChannel .
class TGameActionInvitePlayerToPrivateChatChannel final : public TGameAction
{
public:
    TGameActionInvitePlayerToPrivateChatChannel() { type = EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL; }
    explicit TGameActionInvitePlayerToPrivateChatChannel(QString text);
};

/// CIP input::TGameActionLoadDefaultMinimapTilesFromResources .
class TGameActionLoadDefaultMinimapTilesFromResources final : public TGameAction
{
public:
    TGameActionLoadDefaultMinimapTilesFromResources() { type = EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES; }
};

/// CIP input::TGameActionMakeBattleListPrimary .
class TGameActionMakeBattleListPrimary final : public TGameAction
{
public:
    TGameActionMakeBattleListPrimary() { type = EGameAction::MAKE_BATTLE_LIST_PRIMARY; }
};


/// CIP input::TGameActionMountPlayer .
class TGameActionMountPlayer final : public TGameAction
{
public:
    TGameActionMountPlayer() { type = EGameAction::MOUNT_PLAYER; }
    explicit TGameActionMountPlayer(uint32_t creatureId);
};

/// CIP input::TGameActionMoveUp .
class TGameActionMoveUp final : public TGameAction
{
public:
    TGameActionMoveUp() { type = EGameAction::MOVE_UP; }
    TGameActionMoveUp(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionOpenAddVipDialog .
class TGameActionOpenAddVipDialog final : public TGameActionVip
{
public:
    TGameActionOpenAddVipDialog() { type = EGameAction::OPEN_ADD_VIP_DIALOG; }
};

/// CIP input::TGameActionOpenAddVipGroupDialog .
class TGameActionOpenAddVipGroupDialog final : public TGameActionVipGroup
{
public:
    TGameActionOpenAddVipGroupDialog() { type = EGameAction::OPEN_ADD_VIP_GROUP_DIALOG; }
};

/// CIP input::TGameActionOpenContainerInSpecificSidebar .
class TGameActionOpenContainerInSpecificSidebar final : public TGameAction
{
public:
    TGameActionOpenContainerInSpecificSidebar() { type = EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR; }
};

/// CIP input::TGameActionOpenCyclopedia .
class TGameActionOpenCyclopedia final : public TGameAction
{
public:
    TGameActionOpenCyclopedia() { type = EGameAction::OPEN_CYCLOPEDIA; }
};

/// CIP input::TGameActionOpenEditVipDialog .
class TGameActionOpenEditVipDialog final : public TGameActionVip
{
public:
    TGameActionOpenEditVipDialog() { type = EGameAction::OPEN_EDIT_VIP_DIALOG; }
    explicit TGameActionOpenEditVipDialog(QString text);
};

/// CIP input::TGameActionOpenEditVipGroupDialog .
class TGameActionOpenEditVipGroupDialog final : public TGameActionVipGroup
{
public:
    TGameActionOpenEditVipGroupDialog() { type = EGameAction::OPEN_EDIT_VIP_GROUP_DIALOG; }
    explicit TGameActionOpenEditVipGroupDialog(QString text);
};

/// CIP input::TGameActionOpenGetPremiumUrl .
class TGameActionOpenGetPremiumUrl final : public TGameAction
{
public:
    TGameActionOpenGetPremiumUrl() { type = EGameAction::OPEN_URL_GET_PREMIUM; }
};

/// CIP input::TGameActionOpenHighscoresDialog .
class TGameActionOpenHighscoresDialog final : public TGameAction
{
public:
    TGameActionOpenHighscoresDialog() { type = EGameAction::OPEN_HIGHSCORES_DIALOG; }
};

/// CIP input::TGameActionOpenLootContainer .
class TGameActionOpenLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionOpenLootContainer() { type = EGameAction::LOOT_CONTAINER_OPEN; }
};

/// CIP input::TGameActionOpenMessageToChatChannel .
class TGameActionOpenMessageToChatChannel final : public TGameAction
{
public:
    TGameActionOpenMessageToChatChannel() { type = EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL; }
    explicit TGameActionOpenMessageToChatChannel(QString text);
};

/// CIP input::TGameActionOpenNewsDialog .
class TGameActionOpenNewsDialog final : public TGameAction
{
public:
    TGameActionOpenNewsDialog() { type = EGameAction::OPEN_NEWS_DIALOG; }
};

/// CIP input::TGameActionOpenNewsDialogWithCategory .
class TGameActionOpenNewsDialogWithCategory final : public TGameAction
{
public:
    TGameActionOpenNewsDialogWithCategory() { type = EGameAction::OPEN_NEWS_CATEGORY; }
};

/// CIP input::TGameActionOpenObtainContainer .
class TGameActionOpenObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionOpenObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_OPEN; }
};

/// CIP input::TGameActionOpenOptionsMenuDialog .
class TGameActionOpenOptionsMenuDialog final : public TGameAction
{
public:
    TGameActionOpenOptionsMenuDialog() { type = EGameAction::OPEN_OPTIONS_MENU_DIALOG; }
};

/// CIP input::TGameActionOpenParentContainer .
class TGameActionOpenParentContainer final : public TGameAction
{
public:
    TGameActionOpenParentContainer() { type = EGameAction::OPEN_PARENT_CONTAINER; }
    TGameActionOpenParentContainer(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::TGameActionOpenPreyDialog .
class TGameActionOpenPreyDialog final : public TGameAction
{
public:
    TGameActionOpenPreyDialog() { type = EGameAction::OPEN_PREY_DIALOG; }
    explicit TGameActionOpenPreyDialog(uint32_t creatureId);
};

/// CIP input::TGameActionOpenQuestLog .
class TGameActionOpenQuestLog final : public TGameAction
{
public:
    TGameActionOpenQuestLog() { type = EGameAction::OPEN_QUESTLOG_DIALOG; }
};

/// CIP input::TGameActionOpenSecondaryBattleListInSpecificSidebar .
class TGameActionOpenSecondaryBattleListInSpecificSidebar final : public TGameAction
{
public:
    TGameActionOpenSecondaryBattleListInSpecificSidebar() { type = EGameAction::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR; }
};

/// CIP input::TGameActionOpenShowImageDialog .
class TGameActionOpenShowImageDialog final : public TGameAction
{
public:
    TGameActionOpenShowImageDialog() { type = EGameAction::OPEN_SHOW_IMAGE_DIALOG; }
    explicit TGameActionOpenShowImageDialog(QString text);
};

/// CIP input::TGameActionOpenSocialDialog .
class TGameActionOpenSocialDialog final : public TGameAction
{
public:
    TGameActionOpenSocialDialog() { type = EGameAction::OPEN_SOCIAL_DIALOG; }
};

/// CIP input::TGameActionOpenStore .
class TGameActionOpenStore final : public TGameAction
{
public:
    TGameActionOpenStore() { type = EGameAction::OPEN_STORE; }
};

/// CIP input::TGameActionOpenSurveyUrl .
class TGameActionOpenSurveyUrl final : public TGameAction
{
public:
    TGameActionOpenSurveyUrl() { type = EGameAction::OPEN_URL_SURVEY; }
};

/// CIP input::TGameActionOpenTradeCharacterDialog .
class TGameActionOpenTradeCharacterDialog final : public TGameAction
{
public:
    TGameActionOpenTradeCharacterDialog() { type = EGameAction::OPEN_TRADE_CHARACTER_DIALOG; }
};

/// CIP input::TGameActionOpenUrl .
class TGameActionOpenUrl final : public TGameAction
{
public:
    TGameActionOpenUrl() { type = EGameAction::OPEN_URL; }
    explicit TGameActionOpenUrl(QString text);
};

/// CIP input::TGameActionPreySystem .
class TGameActionPreySystem final : public TGameAction
{
public:
    TGameActionPreySystem() = default;
    explicit TGameActionPreySystem(EGameAction type);
};

/// CIP input::TGameActionQuitGame .
class TGameActionQuitGame final : public TGameAction
{
public:
    TGameActionQuitGame() { type = EGameAction::QUIT_GAME; }
};

/// CIP input::TGameActionRecentlyTraded .
class TGameActionRecentlyTraded final : public TGameAction
{
public:
    TGameActionRecentlyTraded() { type = EGameAction::RECENTLY_TRADED; }
    explicit TGameActionRecentlyTraded(QString text);
};

/// CIP input::TGameActionRefreshCompendiumNews .
class TGameActionRefreshCompendiumNews final : public TGameAction
{
public:
    TGameActionRefreshCompendiumNews() { type = EGameAction::REFRESH_COMPENDIUM_NEWS; }
};

/// CIP input::TGameActionRemoveAllTutorialMarkers .
class TGameActionRemoveAllTutorialMarkers final : public TGameAction
{
public:
    TGameActionRemoveAllTutorialMarkers() { type = EGameAction::REMOVE_ALL_TUTORIAL_MARKERS; }
};

/// CIP input::TGameActionRemoveSpriteIdentifierFromCache .
class TGameActionRemoveSpriteIdentifierFromCache final : public TGameAction
{
public:
    TGameActionRemoveSpriteIdentifierFromCache() { type = EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE; }
};

/// CIP input::TGameActionRemoveVip .
class TGameActionRemoveVip final : public TGameActionVip
{
public:
    TGameActionRemoveVip() { type = EGameAction::REMOVE_VIP; }
};

/// CIP input::TGameActionRemoveVipGroup .
class TGameActionRemoveVipGroup final : public TGameActionVipGroup
{
public:
    TGameActionRemoveVipGroup() { type = EGameAction::REMOVE_VIP_GROUP; }
};

/// CIP input::TGameActionReportCharacter .
class TGameActionReportCharacter final : public TGameAction
{
public:
    TGameActionReportCharacter() { type = EGameAction::REPORT_NAME; }
    explicit TGameActionReportCharacter(QString text);
};

/// CIP input::TGameActionReportHireling .
class TGameActionReportHireling final : public TGameAction
{
public:
    TGameActionReportHireling() { type = EGameAction::REPORT_HIRELING_NAME; }
    explicit TGameActionReportHireling(uint32_t creatureId);
};

/// CIP input::TGameActionReportMapBug .
class TGameActionReportMapBug final : public TGameAction
{
public:
    TGameActionReportMapBug() { type = EGameAction::REPORT_BUG_MAP; }
};

/// CIP input::TGameActionReportStatement .
class TGameActionReportStatement final : public TGameAction
{
public:
    TGameActionReportStatement() { type = EGameAction::REPORT_STATEMENT; }
    explicit TGameActionReportStatement(QString text);
};

/// CIP input::TGameActionReportText .
class TGameActionReportText final : public TGameAction
{
public:
    TGameActionReportText() { type = EGameAction::REPORT_TEXT; }
    explicit TGameActionReportText(QString text);
};

/// CIP input::TGameActionReportTypo .
class TGameActionReportTypo final : public TGameAction
{
public:
    TGameActionReportTypo() { type = EGameAction::REPORT_BUG_TYPO; }
};

/// CIP input::TGameActionRequestConformationForCopyingTextToClipboard .
class TGameActionRequestConformationForCopyingTextToClipboard final : public TGameAction
{
public:
    TGameActionRequestConformationForCopyingTextToClipboard() { type = EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD; }
    explicit TGameActionRequestConformationForCopyingTextToClipboard(QString text);
};

/// CIP input::TGameActionRequestCopySelectedTextToClipboard .
class TGameActionRequestCopySelectedTextToClipboard final : public TGameAction
{
public:
    TGameActionRequestCopySelectedTextToClipboard() { type = EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD; }
};

/// CIP input::TGameActionRequestOutfitsFromServer .
class TGameActionRequestOutfitsFromServer final : public TGameAction
{
public:
    TGameActionRequestOutfitsFromServer() { type = EGameAction::REQUEST_OUTFITS_FROM_SERVER; }
    explicit TGameActionRequestOutfitsFromServer(uint32_t creatureId);
};
/// CIP input::TGameActionRequestResourceBalance .
class TGameActionRequestResourceBalance final : public TGameAction
{
public:
    TGameActionRequestResourceBalance() { type = EGameAction::REQUEST_RESOURCE_BALANCE; }
};
/// CIP input::TGameActionRequestSearchTypeInDepot .
class TGameActionRequestSearchTypeInDepot final : public TGameAction
{
public:
    TGameActionRequestSearchTypeInDepot() { type = EGameAction::REQUEST_SEARCH_TYPE_IN_DEPOT; }
};
/// CIP input::TGameActionRequestTransactionDetailsFromServer .
class TGameActionRequestTransactionDetailsFromServer final : public TGameAction
{
public:
    TGameActionRequestTransactionDetailsFromServer() { type = EGameAction::REQUEST_TRANSACTION_DETAILS; }
};
/// CIP input::TGameActionRequestVipDataUpdate .
class TGameActionRequestVipDataUpdate final : public TGameActionVip
{
public:
    TGameActionRequestVipDataUpdate() { type = EGameAction::REQUST_VIP_DATA_UPDATE; }
};
/// CIP input::TGameActionRequestVipGroupUpdate .
class TGameActionRequestVipGroupUpdate final : public TGameActionVipGroup
{
public:
    TGameActionRequestVipGroupUpdate() { type = EGameAction::REQUEST_VIP_GROUP_UPDATE; }
};
/// CIP input::TGameActionResolveSprite .
class TGameActionResolveSprite final : public TGameAction
{
public:
    TGameActionResolveSprite() { type = EGameAction::RESOLVE_SPRITE; }
};
/// CIP input::TGameActionSaveJsonDocumentToFile .
class TGameActionSaveJsonDocumentToFile final : public TGameAction
{
public:
    TGameActionSaveJsonDocumentToFile() { type = EGameAction::SAVE_JSON_TO_FILE; }
    explicit TGameActionSaveJsonDocumentToFile(QString filePath);
};
/// CIP input::TGameActionSaveTextToFile .
class TGameActionSaveTextToFile final : public TGameAction
{
public:
    TGameActionSaveTextToFile() { type = EGameAction::SAVE_TEXT_TO_FILE; }
    explicit TGameActionSaveTextToFile(QString filePath);
};
/// CIP input::TGameActionSelectAllText .
class TGameActionSelectAllText final : public TGameAction
{
public:
    TGameActionSelectAllText() { type = EGameAction::REQUEST_SELECT_ALL_TEXT; }
};
/// CIP input::TGameActionSelectLootContainer .
class TGameActionSelectLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionSelectLootContainer() { type = EGameAction::LOOT_CONTAINER_SELECT; }
};
/// CIP input::TGameActionSelectObtainContainer .
class TGameActionSelectObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionSelectObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_SELECT; }
};
/// CIP input::TGameActionSelectPreyMonsterFromBestiary .
class TGameActionSelectPreyMonsterFromBestiary final : public TGameAction
{
public:
    TGameActionSelectPreyMonsterFromBestiary() { type = EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY; }
};
/// CIP input::TGameActionSelectPreyMonsterFromList .
class TGameActionSelectPreyMonsterFromList final : public TGameAction
{
public:
    TGameActionSelectPreyMonsterFromList() { type = EGameAction::PREY_SELECT_MONSTER_FROM_LIST; }
};
/// CIP input::TGameActionSendChatMessage .
class TGameActionSendChatMessage final : public TGameAction
{
public:
    TGameActionSendChatMessage() = default;
    explicit TGameActionSendChatMessage(EGameAction type);
};
/// CIP input::TGameActionSendChatMessageToNpc .
class TGameActionSendChatMessageToNpc final : public TGameAction
{
public:
    TGameActionSendChatMessageToNpc() { type = EGameAction::SEND_CHAT_MESSAGE_TO_NPC; }
    explicit TGameActionSendChatMessageToNpc(QString text);
};
/// CIP input::TGameActionSendChatMessageWithChannelID .
class TGameActionSendChatMessageWithChannelID final : public TGameAction
{
public:
    TGameActionSendChatMessageWithChannelID() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID; }
};
/// CIP input::TGameActionSendChatMessageWithRecipient .
class TGameActionSendChatMessageWithRecipient final : public TGameAction
{
public:
    TGameActionSendChatMessageWithRecipient() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_RECIPIENT; }
    explicit TGameActionSendChatMessageWithRecipient(QString text);
};
/// CIP input::TGameActionSendChatMessageWithSpeechVolume .
class TGameActionSendChatMessageWithSpeechVolume final : public TGameAction
{
public:
    TGameActionSendChatMessageWithSpeechVolume() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME; }
    explicit TGameActionSendChatMessageWithSpeechVolume(QString text);
};
/// CIP input::TGameActionSessiondumpPause .
class TGameActionSessiondumpPause final : public TGameAction
{
public:
    TGameActionSessiondumpPause() { type = EGameAction::SESSIONDUMP_PAUSE; }
};
/// CIP input::TGameActionSessiondumpResume .
class TGameActionSessiondumpResume final : public TGameAction
{
public:
    TGameActionSessiondumpResume() { type = EGameAction::SESSIONDUMP_RESUME; }
};
/// CIP input::TGameActionSessiondumpSkipTo .
class TGameActionSessiondumpSkipTo final : public TGameAction
{
public:
    TGameActionSessiondumpSkipTo() { type = EGameAction::SESSIONDUMP_SKIP_TO; }
};
/// CIP input::TGameActionSessiondumpStart .
class TGameActionSessiondumpStart final : public TGameAction
{
public:
    TGameActionSessiondumpStart() { type = EGameAction::SESSIONDUMP_START; }
};
/// CIP input::TGameActionSetChaseMode .
class TGameActionSetChaseMode final : public TGameAction
{
public:
    TGameActionSetChaseMode() { type = EGameAction::SET_CHASE_OPPONENTS; }
};
/// CIP input::TGameActionSetCreaturePodiumConfiguration .
class TGameActionSetCreaturePodiumConfiguration final : public TGameAction
{
public:
    TGameActionSetCreaturePodiumConfiguration() { type = EGameAction::SET_CREATURE_PODIUM_CONFIGURATION; }
};
/// CIP input::TGameActionSetCurrentChatChannelInputText .
class TGameActionSetCurrentChatChannelInputText final : public TGameAction
{
public:
    TGameActionSetCurrentChatChannelInputText() { type = EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT; }
    explicit TGameActionSetCurrentChatChannelInputText(QString text);
};
/// CIP input::TGameActionSetHirelingName .
class TGameActionSetHirelingName final : public TGameAction
{
public:
    TGameActionSetHirelingName() { type = EGameAction::SET_HIRELING_NAME; }
    explicit TGameActionSetHirelingName(QString text);
};
/// CIP input::TGameActionSetHirelingOutfit .
class TGameActionSetHirelingOutfit final : public TGameAction
{
public:
    TGameActionSetHirelingOutfit() { type = EGameAction::SET_HIRELING_OUTFIT; }
};
/// CIP input::TGameActionSetLootContainerFallback .
class TGameActionSetLootContainerFallback final : public TGameActionManagedContainer
{
public:
    TGameActionSetLootContainerFallback() { type = EGameAction::LOOT_CONTAINER_FALLBACK; }
};
/// CIP input::TGameActionSetPlayerOutfit .
class TGameActionSetPlayerOutfit final : public TGameAction
{
public:
    TGameActionSetPlayerOutfit() { type = EGameAction::SET_PLAYER_OUTFIT; }
};
/// CIP input::TGameActionSetPvPMode .
class TGameActionSetPvPMode final : public TGameAction
{
public:
    TGameActionSetPvPMode() { type = EGameAction::SET_PVP_MODE; }
};
/// CIP input::TGameActionSetSecureMode .
class TGameActionSetSecureMode final : public TGameAction
{
public:
    TGameActionSetSecureMode() { type = EGameAction::SET_SECURE_MODE; }
};
/// CIP input::TGameActionSetSharedExperience .
class TGameActionSetSharedExperience final : public TGameAction
{
public:
    TGameActionSetSharedExperience() { type = EGameAction::SET_SHARED_EXPERIENCE; }
};
/// CIP input::TGameActionSetShowOffSocketConfiguration .
class TGameActionSetShowOffSocketConfiguration final : public TGameAction
{
public:
    TGameActionSetShowOffSocketConfiguration() { type = EGameAction::SET_SHOW_OFF_SOCKET_CONFIGURATION; }
};
/// CIP input::TGameActionSetStatusLine .
class TGameActionSetStatusLine final : public TGameAction
{
public:
    TGameActionSetStatusLine() { type = EGameAction::SET_STATUS_LINE; }
    explicit TGameActionSetStatusLine(QString text);
};
/// CIP input::TGameActionSetTactics .
class TGameActionSetTactics final : public TGameAction
{
public:
    TGameActionSetTactics() { type = EGameAction::SET_TACTICS; }
};
/// CIP input::TGameActionShowConfirmMoveObjectToStashDialog .
class TGameActionShowConfirmMoveObjectToStashDialog final : public TGameAction
{
public:
    TGameActionShowConfirmMoveObjectToStashDialog() { type = EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG; }
    TGameActionShowConfirmMoveObjectToStashDialog(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionShowDialogCreateMinimapMarker .
class TGameActionShowDialogCreateMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    TGameActionShowDialogCreateMinimapMarker() { type = EGameAction::SHOW_DIALOG_CREATE_MINIMAP_MARKER; }
};
/// CIP input::TGameActionShowDialogEditMinimapMarker .
class TGameActionShowDialogEditMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    TGameActionShowDialogEditMinimapMarker() { type = EGameAction::SHOW_DIALOG_EDIT_MINIMAP_MARKER; }
};
/// CIP input::TGameActionShowExaltationResultDialog .
class TGameActionShowExaltationResultDialog final : public TGameAction
{
public:
    TGameActionShowExaltationResultDialog() { type = EGameAction::SHOW_EXALTATION_RESULT_DIALOG; }
};
/// CIP input::TGameActionShowLenshelp .
class TGameActionShowLenshelp final : public TGameAction
{
public:
    TGameActionShowLenshelp() { type = EGameAction::LENSHELP_SHOW; }
};
/// CIP input::TGameActionShowMessageDialog .
class TGameActionShowMessageDialog final : public TGameAction
{
public:
    TGameActionShowMessageDialog() = default;
    explicit TGameActionShowMessageDialog(EGameAction type);
};
/// CIP input::TGameActionShowNaggingDialog .
class TGameActionShowNaggingDialog final : public TGameAction
{
public:
    TGameActionShowNaggingDialog() { type = EGameAction::SHOW_NAGGING_DIALOG; }
};
/// CIP input::TGameActionShowSplitStackDialog .
class TGameActionShowSplitStackDialog final : public TGameAction
{
public:
    TGameActionShowSplitStackDialog() { type = EGameAction::SHOW_SPLIT_STACK_DIALOG; }
    TGameActionShowSplitStackDialog(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionShowTutorialHint .
class TGameActionShowTutorialHint final : public TGameAction
{
public:
    TGameActionShowTutorialHint() { type = EGameAction::SHOW_TUTORIAL_HINT; }
};
/// CIP input::TGameActionSortFilterVipList .
class TGameActionSortFilterVipList final : public TGameAction
{
public:
    TGameActionSortFilterVipList() = default;
    explicit TGameActionSortFilterVipList(EGameAction type);
};
/// CIP input::TGameActionSpriteResolved .
class TGameActionSpriteResolved final : public TGameAction
{
public:
    TGameActionSpriteResolved() { type = EGameAction::SPRITE_RESOLVED; }
};
/// CIP input::TGameActionStartTalkingToNPC .
class TGameActionStartTalkingToNPC final : public TGameAction
{
public:
    TGameActionStartTalkingToNPC() { type = EGameAction::START_TALKING_TO_NPC; }
    explicit TGameActionStartTalkingToNPC(uint32_t creatureId);
};
/// CIP input::TGameActionStartTargetSelection .
class TGameActionStartTargetSelection final : public TGameAction
{
public:
    TGameActionStartTargetSelection() { type = EGameAction::START_TARGET_SELECTION; }
};
/// CIP input::TGameActionStashOpenRetrieveDialog .
class TGameActionStashOpenRetrieveDialog final : public TGameAction
{
public:
    TGameActionStashOpenRetrieveDialog() { type = EGameAction::STASH_OPEN_RETRIEVE_DIALOG; }
};
/// CIP input::TGameActionStashOpenStowStackDialog .
class TGameActionStashOpenStowStackDialog final : public TGameAction
{
public:
    TGameActionStashOpenStowStackDialog() { type = EGameAction::STASH_OPEN_STOW_STACK_DIALOG; }
    TGameActionStashOpenStowStackDialog(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionStashRetrieve .
class TGameActionStashRetrieve final : public TGameAction
{
public:
    TGameActionStashRetrieve() { type = EGameAction::STASH_RETRIEVE; }
};
/// CIP input::TGameActionStashStowContainerContent .
class TGameActionStashStowContainerContent final : public TGameAction
{
public:
    TGameActionStashStowContainerContent() { type = EGameAction::STASH_STOW_CONTAINER_CONTENT; }
    TGameActionStashStowContainerContent(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionStashStowStack .
class TGameActionStashStowStack final : public TGameAction
{
public:
    TGameActionStashStowStack() { type = EGameAction::STASH_STOW_STACK; }
    TGameActionStashStowStack(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionStashStowType .
class TGameActionStashStowType final : public TGameAction
{
public:
    TGameActionStashStowType() { type = EGameAction::STASH_STOW_TYPE; }
};
/// CIP input::TGameActionSwitchChatMode .
class TGameActionSwitchChatMode final : public TGameAction
{
public:
    TGameActionSwitchChatMode() { type = EGameAction::SWITCH_CHAT_MODE; }
};
/// CIP input::TGameActionSwitchClientOptions .
class TGameActionSwitchClientOptions final : public TGameAction
{
public:
    TGameActionSwitchClientOptions() { type = EGameAction::SWITCH_CLIENTOPTIONS_MODE; }
};
/// CIP input::TGameActionTakeAutomaticScreenshot .
class TGameActionTakeAutomaticScreenshot final : public TGameAction
{
public:
    TGameActionTakeAutomaticScreenshot() { type = EGameAction::TAKE_AUTOMATIC_SCREENSHOT; }
};
/// CIP input::TGameActionTakeManualScreenshot .
class TGameActionTakeManualScreenshot final : public TGameAction
{
public:
    TGameActionTakeManualScreenshot() { type = EGameAction::TAKE_MANUAL_SCREENSHOT; }
};
/// CIP input::TGameActionTargetSelected .
class TGameActionTargetSelected final : public TGameAction
{
public:
    TGameActionTargetSelected() { type = EGameAction::TARGET_SELECTED; }
    explicit TGameActionTargetSelected(uint32_t creatureId);
};
/// CIP input::TGameActionTeleportToCoordinate .
class TGameActionTeleportToCoordinate final : public TGameAction
{
public:
    TGameActionTeleportToCoordinate() { type = EGameAction::GOD_TELEPORT; }
};
/// CIP input::TGameActionToggleActionBar .
class TGameActionToggleActionBar final : public TGameAction
{
public:
    TGameActionToggleActionBar() { type = EGameAction::TOGGLE_ACTION_BAR; }
};
/// CIP input::TGameActionToggleActionBars .
class TGameActionToggleActionBars final : public TGameAction
{
public:
    TGameActionToggleActionBars() { type = EGameAction::TOGGLE_ACTION_BARS; }
};
/// CIP input::TGameActionToggleBlackWhitelistStatus .
class TGameActionToggleBlackWhitelistStatus final : public TGameAction
{
public:
    TGameActionToggleBlackWhitelistStatus() = default;
    explicit TGameActionToggleBlackWhitelistStatus(EGameAction type);
};
/// CIP input::TGameActionToggleCreatureHudsVisible .
class TGameActionToggleCreatureHudsVisible final : public TGameAction
{
public:
    TGameActionToggleCreatureHudsVisible() { type = EGameAction::TOGGLE_CREATURE_HUDS_VISIBLE; }
};
/// CIP input::TGameActionToggleMounted .
class TGameActionToggleMounted final : public TGameAction
{
public:
    TGameActionToggleMounted() { type = EGameAction::TOGGLE_MOUNTED; }
};
/// CIP input::TGameActionToggleServerLogSubscriptionInCurrentChatChannel .
class TGameActionToggleServerLogSubscriptionInCurrentChatChannel final : public TGameAction
{
public:
    TGameActionToggleServerLogSubscriptionInCurrentChatChannel() { type = EGameAction::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL; }
};
/// CIP input::TGameActionToggleWidget .
class TGameActionToggleWidget final : public TGameAction
{
public:
    TGameActionToggleWidget() { type = EGameAction::TOGGLE_WIDGET; }
};
/// CIP input::TGameActionTryToEquip .
class TGameActionTryToEquip final : public TGameAction
{
public:
    TGameActionTryToEquip() { type = EGameAction::TRY_TO_EQUIP; }
    TGameActionTryToEquip(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionTryToSwitchHotkeySet .
class TGameActionTryToSwitchHotkeySet final : public TGameAction
{
public:
    TGameActionTryToSwitchHotkeySet() { type = EGameAction::TRY_TO_SWITCH_HOTKEYSET; }
    explicit TGameActionTryToSwitchHotkeySet(QString text);
};
/// CIP input::TGameActionTutorialHintConfirmed .
class TGameActionTutorialHintConfirmed final : public TGameAction
{
public:
    TGameActionTutorialHintConfirmed() { type = EGameAction::CONFIRM_TUTORIAL_HINT; }
};
/// CIP input::TGameActionTutorialProgress .
class TGameActionTutorialProgress final : public TGameAction
{
public:
    TGameActionTutorialProgress() { type = EGameAction::TUTORIAL_PROGRESS; }
};
/// CIP input::TGameActionUnignoreName .
class TGameActionUnignoreName final : public TGameAction
{
public:
    TGameActionUnignoreName() { type = EGameAction::UNIGNORE_NAME; }
    explicit TGameActionUnignoreName(QString text);
};
/// CIP input::TGameActionUnmountPlayer .
class TGameActionUnmountPlayer final : public TGameAction
{
public:
    TGameActionUnmountPlayer() { type = EGameAction::UNMOUNT_PLAYER; }
    explicit TGameActionUnmountPlayer(uint32_t creatureId);
};
/// CIP input::TGameActionUpdateExivaOptions .
class TGameActionUpdateExivaOptions final : public TGameAction
{
public:
    TGameActionUpdateExivaOptions() { type = EGameAction::EXIVA_UPADATE_OPTIONS; }
};
/// CIP input::TGameActionUpdateMinimapMarker .
class TGameActionUpdateMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    TGameActionUpdateMinimapMarker() { type = EGameAction::UPDATE_MINIMAP_MARKER; }
};
/// CIP input::TGameActionUseOnCreature .
class TGameActionUseOnCreature final : public TGameAction
{
public:
    TGameActionUseOnCreature() { type = EGameAction::USE_ON_CREATURE; }
    TGameActionUseOnCreature(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionUseOnTarget .
class TGameActionUseOnTarget final : public TGameAction
{
public:
    TGameActionUseOnTarget() { type = EGameAction::USE_ON_TARGET; }
    TGameActionUseOnTarget(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionUseOnYourself .
class TGameActionUseOnYourself final : public TGameAction
{
public:
    TGameActionUseOnYourself() { type = EGameAction::USE_ON_YOURSELF; }
    TGameActionUseOnYourself(Position position, int stackPosition, uint16_t objectClientId = 0);
};
/// CIP input::TGameActionVisibleChatChannel .
class TGameActionVisibleChatChannel final : public TGameAction
{
public:
    TGameActionVisibleChatChannel() = default;
    explicit TGameActionVisibleChatChannel(EGameAction type);
};
namespace gameactions {

/// CIP input::gameactions::TGameActionGatherAll (v0 → GATHER_ALL).
class TGameActionGatherAll final : public TGameAction
{
public:
    TGameActionGatherAll() { type = EGameAction::GATHER_ALL; }
    TGameActionGatherAll(Position position, int stackPosition, uint16_t objectClientId = 0);
};

/// CIP input::gameactions::TGameActionGameEvent .
class TGameActionGameEvent final : public TGameAction
{
public:
    TGameActionGameEvent() { type = EGameAction::GAME_EVENT; }
};
/// CIP input::gameactions::TGameActionMarketBrowse .
class TGameActionMarketBrowse final : public TGameAction
{
public:
    TGameActionMarketBrowse() { type = EGameAction::MARKET_BROWSE; }
};
/// CIP input::gameactions::TGameActionOpenTaskboardDialog .
class TGameActionOpenTaskboardDialog final : public TGameAction
{
public:
    TGameActionOpenTaskboardDialog() { type = EGameAction::OPEN_TASKBOARD_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyDialog .
class TGameActionOpenWeaponProficiencyDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog .
class TGameActionOpenWeaponProficiencyShapeDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyShapeDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog .
class TGameActionOpenWeaponProficiencyShapeOptionsDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyShapeOptionsDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG; }
};
/// CIP input::gameactions::TGameActionSaveSkillWheel .
class TGameActionSaveSkillWheel final : public TGameAction
{
public:
    TGameActionSaveSkillWheel() { type = EGameAction::SAVE_SKILL_WHEEL; }
};
/// CIP input::gameactions::TGameActionShowCreateCharacterDialog .
class TGameActionShowCreateCharacterDialog final : public TGameAction
{
public:
    TGameActionShowCreateCharacterDialog() { type = EGameAction::SHOW_CREATE_CHARACTER_DIALOG; }
};
/// CIP input::gameactions::TGameActionSimple .
class TGameActionSimple final : public TGameAction
{
public:
    TGameActionSimple() = default;
    explicit TGameActionSimple(EGameAction type);
};
/// CIP input::gameactions::TGameActionSoundTrigger .
class TGameActionSoundTrigger final : public TGameAction
{
public:
    TGameActionSoundTrigger() { type = EGameAction::SOUND_TRIGGER; }
};
/// CIP input::gameactions::TSelectAtelierGemGameAction .
class TSelectAtelierGemGameAction final : public TGameAction
{
public:
    TSelectAtelierGemGameAction() { type = EGameAction::GEM_ATELIER_SELECT_GEM; }
};
/// CIP input::gameactions::TSelectFragmentWorkshopModGameAction .
class TSelectFragmentWorkshopModGameAction final : public TGameAction
{
public:
    TSelectFragmentWorkshopModGameAction() { type = EGameAction::FRAGMENT_WORKSHOP_SELECT_MOD; }
};
}

TGameActionPtr makeAction(EGameAction type);
TGameActionPtr makeObjectAction(EGameAction type, const Position& position, int stackPosition, uint16_t objectClientId = 0);
TGameActionPtr makeCreatureAction(EGameAction type, uint32_t creatureId);
TGameActionPtr makeMovementIntent(const Position& position, int pathMode = 0);
TGameActionPtr makeSmartClick(const TGameActionPtr& inner);

/// Typed mutable copy: factory dynamic type + field assign (CIP handler reshapes).
std::shared_ptr<TGameAction> mutableClone(const TGameAction& action);

/// For each type in order, the first candidate of that type (CIP:).
TGameActionList filterActions(const TGameActionList& candidates, std::span<const EGameAction> types);
inline TGameActionList filterActions(const TGameActionList& candidates, std::initializer_list<EGameAction> types)
{
    return filterActions(candidates, std::span<const EGameAction>(types.begin(), types.size()));
}

/// The candidates in context-menu order with separators between groups
/// (CIP:).
TGameActionList contextMenuActions(const TGameActionList& candidates);
/// The same with a panel's own table (CONTEXT_MENU_SEPARATOR between groups).
TGameActionList contextMenuActions(const TGameActionList& candidates, std::span<const EGameAction> order);

/// A dragged object's drop (CIP:). Shift moves everything; a
/// stack asks for the amount when Ctrl's state matches option
/// dragAndDropDefaultActionIsMoveAll.
TGameActionPtr finalizeDrop(const TGameActionPtr& move, const Position& destination, int modifiers, bool defaultIsMoveAll);

}
}
