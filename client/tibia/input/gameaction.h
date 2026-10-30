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

namespace tibia::input {

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
    /// Object actions: the object, and the world-map field and stack position
    /// it is acted on at (CIP: TGameActionContext 0xb).
    ThingPtr object;
    Position position;
    int stackPosition = -1;
    /// Creature actions.
    uint32_t creatureId = 0;
    /// SMART_CLICK: the action a click runs.
    TGameActionPtr inner;
    /// ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE.
    QString text;
    QColor color;
    /// SAVE_TEXT_TO_FILE / SAVE_JSON_TO_FILE: where `text` is written (CIP:
    /// TGameActionSaveTextToFile / TGameActionSaveJsonToFile, 0x100e5eac0 / 0x100e5ebc0).
    QString filePath;
    /// REPORT_TEXT (CIP: TGameActionReportStatement, 0x100e5a540): `text` is
    /// the author, `parameter` the text's edit id and this the reported text.
    QString statement;
    /// LENSHELP_SHOW.
    int lensHelpId = 0;
    /// MOVE_OBJECT: where the object is dropped (inventory slots are
    /// Position(0xFFFF, slot, 0), slot 0 for any) and whether Shift asked
    /// for everything.
    Position destination;
    bool moveAll = false;
    /// MOVE_OBJECT: how many of a stack move (CIP 0x100e57720, set by the
    /// split-stack dialog); 0 moves the whole stack. STASH_STOW_STACK /
    /// STASH_RETRIEVE: the count, the stack's / stored one until the amount
    /// dialog sets the chosen one (CIP 0x100e5db90 / 0x100e5dca0).
    int amount = 0;
    /// SET_SHARED_EXPERIENCE: whether it turns shared experience on.
    bool enable = false;
    /// SET_TACTICS.
    bool chase = false;
    bool secure = false;
    int pvpMode = 0;
    /// ACTION_BUTTON_TRIGGERED (CIP: TGameActionActionButtonTriggered,
    /// 0x100e5c8e0); TOGGLE_ACTION_BAR: the bar.
    int actionBarId = 0;
    int actionButtonId = 0;
    int multiActionIndex = 0;
    /// The one value of a parameterised action: TOGGLE_ACTION_BARS position, SWITCH_CHAT_MODE mode, OPEN_CYCLOPEDIA /
    /// OPEN_SOCIAL_DIALOG tab, OPEN_OPTIONS_MENU_DIALOG page (CIP FUN_100e893b0).
    int parameter = 0;
    /// TOGGLE_WIDGET / OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR /
    /// OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR / MAKE_BATTLE_LIST_PRIMARY:
    /// the container or battle list id, and the sidebar (0xff: any).
    int instance = 0;
    uint sidebarId = 0xff;
    /// Repeated by a held key (CIP: FUN_100e5c940).
    bool autoRepeat = false;
    /// SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT from a hotkey (CIP FUN_100e83f40, action +0x21 / +0x24),
    /// CROSSHAIR_SPELL, SPELL_AT_MOUSE_CURSOR: whether `text` casts a spell, its
    /// ESpellCastOptionalTargetType, and the say's Otc::MessageMode.
    bool isSpell = false;
    int spellCastType = 0;
    int talkMode = 0;
    /// USE_WITH / CROSSHAIR_SPELL: the TARGET_SELECTED action picked for it (CIP FUN_100e578b0);
    /// null starts a target selection.
    TGameActionPtr target;
    /// OPEN_CHAT_CHANNEL / OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT: the channel's
    /// chat::TChatChannelIdentifier type and server channel id; `text` the name.
    int chatChannelType = 0;
    int chatChannelId = 0;
    /// OPEN_CYCLOPEDIA: the tab's sub-page, -1 for none (CIP: the optional
    /// int at +0x38 of TGameActionOpenCyclopedia, FUN_100e5cdc0). The
    /// character tab (parameter 2) takes
    /// TCharacterInfoDialogController::EDialogCategory (1: BattleMainMenu).
    /// OPEN_STORE: `parameter` is the TStoreOpenParameters open action and
    /// `instance` its detail (CIP 0x100e5be40; open action 1 is the premium /
    /// boost page, detail 1 its XP boost).
    int category = -1;
    /// EXALTATION_FORGE_FUSION / _CONVERGENCE_FUSION / _TRANSFER (CIP: the
    /// TGameActionExaltationForge* read by 0x100acc690): the object and its
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
    /// SET_PLAYER_MOVEMENT_INTENT: Minimap / Cyclopedia / Travel Soft policy.
    int autoWalkPathMode = 0;
};

/// CIP input::TGameActionLook (v0 0x100e56ee0 → LOOK).
class TGameActionLook final : public TGameAction
{
public:
    TGameActionLook() { type = EGameAction::LOOK; }
    TGameActionLook(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionUse (v0 0x100e56e50 → USE).
class TGameActionUse final : public TGameAction
{
public:
    TGameActionUse() { type = EGameAction::USE; }
    TGameActionUse(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionUseWith (ctor 0x100e577b0); also USE_ON_*.
class TGameActionUseWith final : public TGameAction
{
public:
    TGameActionUseWith() { type = EGameAction::USE_WITH; }
    TGameActionUseWith(ThingPtr object, Position position, int stackPosition);
    explicit TGameActionUseWith(const TGameAction& other);
};

/// CIP input::TGameActionCreature (ctor 0x100e57560): ATTACK_CREATURE,
/// FOLLOW_CREATURE, LOOK_AT_CREATURE, stop variants, and other creature-id
/// actions share this carrier.
class TGameActionCreature final : public TGameAction
{
public:
    TGameActionCreature() = default;
    TGameActionCreature(EGameAction type, uint32_t creatureId);
};

/// CIP input::TGameActionMoveObject (ctor 0x100e57600).
class TGameActionMoveObject final : public TGameAction
{
public:
    TGameActionMoveObject() { type = EGameAction::MOVE_OBJECT; }
    TGameActionMoveObject(ThingPtr object, Position position, int stackPosition);
    explicit TGameActionMoveObject(const TGameAction& other);
};

/// CIP input::TGameActionPlayerMovementIntent (0x100e7b1c0) for
/// SET_PLAYER_MOVEMENT_INTENT.
class TGameActionPlayerMovementIntent final : public TGameAction
{
public:
    TGameActionPlayerMovementIntent() { type = EGameAction::SET_PLAYER_MOVEMENT_INTENT; }
    explicit TGameActionPlayerMovementIntent(Position position, int pathMode = 0);
};
/// CIP input::TGameActionManagedContainer (0x100e5f690).
class TGameActionManagedContainer : public TGameAction
{
public:
    TGameActionManagedContainer() = default;
    explicit TGameActionManagedContainer(EGameAction type);
};

/// CIP input::TGameActionForMinimapMarkerUsingCoordinate (0x100e5f5f0).
class TGameActionForMinimapMarkerUsingCoordinate : public TGameAction
{
public:
    TGameActionForMinimapMarkerUsingCoordinate() = default;
    explicit TGameActionForMinimapMarkerUsingCoordinate(EGameAction type);
};

/// CIP input::TGameActionForMinimapMarkerUsingFullMarkerData (0x100e60e80).
class TGameActionForMinimapMarkerUsingFullMarkerData : public TGameAction
{
public:
    TGameActionForMinimapMarkerUsingFullMarkerData() = default;
    explicit TGameActionForMinimapMarkerUsingFullMarkerData(EGameAction type);
};

/// CIP input::TGameActionVip (0x100e61280).
class TGameActionVip : public TGameAction
{
public:
    TGameActionVip() = default;
    explicit TGameActionVip(EGameAction type);
};

/// CIP input::TGameActionVipGroup (0x100e63980).
class TGameActionVipGroup : public TGameAction
{
public:
    TGameActionVipGroup() = default;
    explicit TGameActionVipGroup(EGameAction type);
};


/// CIP input::TGameActionOpen (v0 0x100e57100 → OPEN).
class TGameActionOpen final : public TGameAction
{
public:
    TGameActionOpen() { type = EGameAction::OPEN; }
    TGameActionOpen(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionOpenInNewWindow (v0 0x100e571b0).
class TGameActionOpenInNewWindow final : public TGameAction
{
public:
    TGameActionOpenInNewWindow() { type = EGameAction::OPEN_IN_NEW_WINDOW; }
    TGameActionOpenInNewWindow(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionWrap (v0 0x100e57370).
class TGameActionWrap final : public TGameAction
{
public:
    TGameActionWrap() { type = EGameAction::WRAP; }
    TGameActionWrap(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionUnwrap (v0 0x100e574a0).
class TGameActionUnwrap final : public TGameAction
{
public:
    TGameActionUnwrap() { type = EGameAction::UNWRAP; }
    TGameActionUnwrap(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionRotate (v0 0x100e57530 → ROTATE_OBJECT).
class TGameActionRotate final : public TGameAction
{
public:
    TGameActionRotate() { type = EGameAction::ROTATE_OBJECT; }
    TGameActionRotate(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionQuickLoot (v0 0x100e5d140).
class TGameActionQuickLoot final : public TGameAction
{
public:
    TGameActionQuickLoot() { type = EGameAction::QUICK_LOOT; }
    TGameActionQuickLoot(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionPlayerTradeItem (v0 0x100e5a290 → TRADE_ITEM).
class TGameActionPlayerTradeItem final : public TGameAction
{
public:
    TGameActionPlayerTradeItem() { type = EGameAction::TRADE_ITEM; }
    TGameActionPlayerTradeItem(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionSmartClick (v0 0x100e5b130).
class TGameActionSmartClick final : public TGameAction
{
public:
    TGameActionSmartClick() { type = EGameAction::SMART_CLICK; }
    explicit TGameActionSmartClick(TGameActionPtr innerAction);
};

/// CIP input::TGameActionContextMenu (v0 0x100e58270).
class TGameActionContextMenu final : public TGameAction
{
public:
    TGameActionContextMenu() { type = EGameAction::CONTEXT_MENU; }
};

/// CIP input::TGameActionContextMenuSeparator (v0 0x100e575a0).
class TGameActionContextMenuSeparator final : public TGameAction
{
public:
    TGameActionContextMenuSeparator() { type = EGameAction::CONTEXT_MENU_SEPARATOR; }
};

/// CIP input::TGameActionCrossHairSpell (v0 0x100e579c0).
class TGameActionCrossHairSpell final : public TGameAction
{
public:
    TGameActionCrossHairSpell() { type = EGameAction::CROSSHAIR_SPELL; }
};

/// CIP input::TGameActionSpellAtMouseCursor (v0 0x100e57c10).
class TGameActionSpellAtMouseCursor final : public TGameAction
{
public:
    TGameActionSpellAtMouseCursor() { type = EGameAction::SPELL_AT_MOUSE_CURSOR; }
};

/// CIP input::TGameActionUseAtMouseCursor (v0 0x100e57ea0); also built from
/// TGameActionUseWith fields when a crosshair use is aimed at the cursor.
class TGameActionUseAtMouseCursor final : public TGameAction
{
public:
    TGameActionUseAtMouseCursor() { type = EGameAction::USE_AT_MOUSECURSOR; }
    TGameActionUseAtMouseCursor(ThingPtr object, Position position, int stackPosition);
    explicit TGameActionUseAtMouseCursor(const TGameAction& other);
};

/// CIP input::TGameActionActionButtonTriggered (0x100e5c8e0).
class TGameActionActionButtonTriggered final : public TGameAction
{
public:
    TGameActionActionButtonTriggered() { type = EGameAction::ACTION_BUTTON_TRIGGERED; }
    TGameActionActionButtonTriggered(int actionBarId, int actionButtonId, int multiActionIndex = -1);
};


/// CIP input::TGameActionAddGUITutorialMarker (0x100e633b0).
class TGameActionAddGUITutorialMarker final : public TGameAction
{
public:
    TGameActionAddGUITutorialMarker() { type = EGameAction::ADD_GUI_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddHintAtTutorialMarker (0x100e634d0).
class TGameActionAddHintAtTutorialMarker final : public TGameAction
{
public:
    TGameActionAddHintAtTutorialMarker() { type = EGameAction::ADD_HINT_AT_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddMouseTrailHintAtTutorialMarker (0x100e635f0).
class TGameActionAddMouseTrailHintAtTutorialMarker final : public TGameAction
{
public:
    TGameActionAddMouseTrailHintAtTutorialMarker() { type = EGameAction::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER; }
};

/// CIP input::TGameActionAddNewVip (v0 0x100e59f00 → ADD_NEW_VIP).
class TGameActionAddNewVip final : public TGameActionVip
{
public:
    TGameActionAddNewVip() { type = EGameAction::ADD_NEW_VIP; }
    explicit TGameActionAddNewVip(QString name);
};

/// CIP input::TGameActionAddNewVipGroup (0x100e63b80).
class TGameActionAddNewVipGroup final : public TGameActionVipGroup
{
public:
    TGameActionAddNewVipGroup() { type = EGameAction::ADD_NEW_VIP_GROUP; }
};

/// CIP input::TGameActionAddTextMessage (0x100e62af0): ADD_SYSTEM_CHAT_MESSAGE /
/// ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE.
class TGameActionAddTextMessage final : public TGameAction
{
public:
    TGameActionAddTextMessage() = default;
    explicit TGameActionAddTextMessage(EGameAction type);
};

/// CIP input::TGameActionAssignSpellToActionButton (0x100e64fa0).
class TGameActionAssignSpellToActionButton final : public TGameAction
{
public:
    TGameActionAssignSpellToActionButton() { type = EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON; }
};

/// CIP input::TGameActionAttackFirstTarget (0x100e62b90).
class TGameActionAttackFirstTarget final : public TGameAction
{
public:
    TGameActionAttackFirstTarget() { type = EGameAction::ATTACK_FIRST_TARGET; }
};

/// CIP input::TGameActionAttackNextTarget (0x100e62bb0).
class TGameActionAttackNextTarget final : public TGameAction
{
public:
    TGameActionAttackNextTarget() { type = EGameAction::ATTACK_NEXT_TARGET; }
};

/// CIP input::TGameActionAttackPreviousTarget (0x100e62bd0).
class TGameActionAttackPreviousTarget final : public TGameAction
{
public:
    TGameActionAttackPreviousTarget() { type = EGameAction::ATTACK_PREVIOUS_TARGET; }
};

/// CIP input::TGameActionBrowseField (v0 0x100e57050 → BROWSE_FIELD).
class TGameActionBrowseField final : public TGameAction
{
public:
    TGameActionBrowseField() { type = EGameAction::BROWSE_FIELD; }
    TGameActionBrowseField(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionBuyHirelingNameOrSexChange (0x100e64910).
class TGameActionBuyHirelingNameOrSexChange final : public TGameAction
{
public:
    TGameActionBuyHirelingNameOrSexChange() { type = EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE; }
    explicit TGameActionBuyHirelingNameOrSexChange(uint32_t creatureId);
};

/// CIP input::TGameActionChangeAutoPreyExtendType (0x100e638e0).
class TGameActionChangeAutoPreyExtendType final : public TGameAction
{
public:
    TGameActionChangeAutoPreyExtendType() { type = EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE; }
};

/// CIP input::TGameActionChangeCharacter (0x100e628d0).
class TGameActionChangeCharacter final : public TGameAction
{
public:
    TGameActionChangeCharacter() { type = EGameAction::CHANGE_CHARACTER; }
};

/// CIP input::TGameActionChangeContextMenuEnabled (0x100e63750).
class TGameActionChangeContextMenuEnabled final : public TGameAction
{
public:
    TGameActionChangeContextMenuEnabled() { type = EGameAction::CHANGE_CONTEXT_MENU_ENABLED; }
};

/// CIP input::TGameActionChangeQuesttrackerOptions (0x100e63e40).
class TGameActionChangeQuesttrackerOptions final : public TGameAction
{
public:
    TGameActionChangeQuesttrackerOptions() { type = EGameAction::CHANGE_QUESTTRACKER_OPTIONS; }
};

/// CIP input::TGameActionChangeTrackedQuestFlags (0x100e63dd0).
class TGameActionChangeTrackedQuestFlags final : public TGameAction
{
public:
    TGameActionChangeTrackedQuestFlags() { type = EGameAction::CHANGE_TRACKED_QUESTFLAGS; }
};

/// CIP input::TGameActionChangeTutorialModeEnabled (0x100e63770).
class TGameActionChangeTutorialModeEnabled final : public TGameAction
{
public:
    TGameActionChangeTutorialModeEnabled() { type = EGameAction::CHANGE_TUTORIAL_MODE_ENABLED; }
};

/// CIP input::TGameActionChatChannel (0x100e60890): OPEN/CLOSE/MUTE and related
/// channel-id actions share this carrier.
class TGameActionChatChannel final : public TGameAction
{
public:
    TGameActionChatChannel() = default;
    explicit TGameActionChatChannel(EGameAction type);
    TGameActionChatChannel(EGameAction type, int chatChannelType, int chatChannelId, QString name = {});
};

/// CIP input::TGameActionChatChannelModeration (0x100e62130): invite/exclude
/// player from a channel.
class TGameActionChatChannelModeration final : public TGameAction
{
public:
    TGameActionChatChannelModeration() = default;
    explicit TGameActionChatChannelModeration(EGameAction type);
    TGameActionChatChannelModeration(EGameAction type, QString playerName);
};

/// CIP input::TGameActionClearLootContainer (0x100e63fa0 → LOOT_CONTAINER_CLEAR).
class TGameActionClearLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionClearLootContainer() { type = EGameAction::LOOT_CONTAINER_CLEAR; }
};

/// CIP input::TGameActionClearObtainContainer (0x100d2cf50 → OBTAIN_CONTAINER_CLEAR).
class TGameActionClearObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionClearObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_CLEAR; }
};

/// CIP input::TGameActionClearOldestMessageFromMapwindow (0x100e624a0).
class TGameActionClearOldestMessageFromMapwindow final : public TGameAction
{
public:
    TGameActionClearOldestMessageFromMapwindow() { type = EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW; }
};

/// CIP input::TGameActionCloseAllMultiActionButtons (0x100e65060).
class TGameActionCloseAllMultiActionButtons final : public TGameAction
{
public:
    TGameActionCloseAllMultiActionButtons() { type = EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS; }
};

/// CIP input::TGameActionCloseCurrentChatChannel (0x100e62540).
class TGameActionCloseCurrentChatChannel final : public TGameAction
{
public:
    TGameActionCloseCurrentChatChannel() { type = EGameAction::CLOSE_CURRENT_CHAT_CHANNEL; }
};

/// CIP input::TGameActionCollectPickItemsDailyReward (0x100e63d40).
class TGameActionCollectPickItemsDailyReward final : public TGameAction
{
public:
    TGameActionCollectPickItemsDailyReward() { type = EGameAction::COLLECT_PICK_ITEMS_DAILY_REWARD; }
};

/// CIP input::TGameActionConfigureShowOffSocket (v0 0x100e5e8b0).
class TGameActionConfigureShowOffSocket final : public TGameAction
{
public:
    TGameActionConfigureShowOffSocket() { type = EGameAction::CONFIGURE_SHOW_OFF_SOCKET; }
    TGameActionConfigureShowOffSocket(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionCopyChatEntryToClipboard (0x100e61ef0).
class TGameActionCopyChatEntryToClipboard final : public TGameAction
{
public:
    TGameActionCopyChatEntryToClipboard() { type = EGameAction::COPY_CHAT_ENTRY_TO_CLIPBOARD; }
    explicit TGameActionCopyChatEntryToClipboard(QString text);
};

/// CIP input::TGameActionCopyCreatureNameToClipboard (0x100e61f80).
class TGameActionCopyCreatureNameToClipboard final : public TGameAction
{
public:
    TGameActionCopyCreatureNameToClipboard() { type = EGameAction::COPY_CREATURE_NAME_TO_CLIPBOARD; }
    explicit TGameActionCopyCreatureNameToClipboard(QString name);
};

/// CIP input::TGameActionCopyTextToClipboard (v0 0x100e58500).
class TGameActionCopyTextToClipboard final : public TGameAction
{
public:
    TGameActionCopyTextToClipboard() { type = EGameAction::COPY_TEXT_TO_CLIPBOARD; }
    explicit TGameActionCopyTextToClipboard(QString text);
};

/// CIP input::TGameActionCopyTrustedTextToClipboard (0x100e62010).
class TGameActionCopyTrustedTextToClipboard final : public TGameAction
{
public:
    TGameActionCopyTrustedTextToClipboard() { type = EGameAction::COPY_TRUSTED_TEXT_TO_CLIPBOARD; }
    explicit TGameActionCopyTrustedTextToClipboard(QString text);
};

/// CIP input::TGameActionCreatureWithName (0x100e611f0): IGNORE/UNIGNORE,
/// OPEN_MESSAGE_TO_CHAT_CHANNEL, REPORT_NAME, RECENTLY_TRADED, and similar
/// name-keyed actions share this carrier.
class TGameActionCreatureWithName final : public TGameAction
{
public:
    TGameActionCreatureWithName() = default;
    explicit TGameActionCreatureWithName(EGameAction type);
    TGameActionCreatureWithName(EGameAction type, QString name);
};

/// CIP input::TGameActionCyclopediaMapSelectionItem (0x100e64860).
class TGameActionCyclopediaMapSelectionItem final : public TGameAction
{
public:
    TGameActionCyclopediaMapSelectionItem() { type = EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM; }
    explicit TGameActionCyclopediaMapSelectionItem(int subareaId);
};

/// CIP input::TGameActionDeleteMinimapMarker (0x100e62240).
class TGameActionDeleteMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    TGameActionDeleteMinimapMarker() { type = EGameAction::DELETE_MINIMAP_MARKER; }
};

/// CIP input::TGameActionDepotSearchRetrieve (0x100e64650 → DEPOT_SERACH_RETRIEVE).
class TGameActionDepotSearchRetrieve final : public TGameAction
{
public:
    TGameActionDepotSearchRetrieve() { type = EGameAction::DEPOT_SERACH_RETRIEVE; }
};

/// CIP input::TGameActionDisplayNextChatChannelTab (0x100e62580).
class TGameActionDisplayNextChatChannelTab final : public TGameAction
{
public:
    TGameActionDisplayNextChatChannelTab() { type = EGameAction::DISPLAY_NEXT_CHAT_CHANNEL_TAB; }
};

/// CIP input::TGameActionDisplayPreviousChatChannelTab (0x100e625a0).
class TGameActionDisplayPreviousChatChannelTab final : public TGameAction
{
public:
    TGameActionDisplayPreviousChatChannelTab() { type = EGameAction::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB; }
};

/// CIP input::TGameActionExaltationForgeConvergenceFusion (0x100e64f10).
class TGameActionExaltationForgeConvergenceFusion final : public TGameAction
{
public:
    TGameActionExaltationForgeConvergenceFusion() { type = EGameAction::EXALTATION_FORGE_CONVERGENCE_FUSION; }
};

/// CIP input::TGameActionExaltationForgeFusion (0x100e64ef0).
class TGameActionExaltationForgeFusion final : public TGameAction
{
public:
    TGameActionExaltationForgeFusion() { type = EGameAction::EXALTATION_FORGE_FUSION; }
};

/// CIP input::TGameActionExaltationForgeTransfer (0x100e64ed0).
class TGameActionExaltationForgeTransfer final : public TGameAction
{
public:
    TGameActionExaltationForgeTransfer() { type = EGameAction::EXALTATION_FORGE_TRANSFER; }
};

/// CIP input::TGameActionExcludePlayerFromChatChannel (v0 0x100e588d0).
class TGameActionExcludePlayerFromChatChannel final : public TGameAction
{
public:
    TGameActionExcludePlayerFromChatChannel() { type = EGameAction::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL; }
    explicit TGameActionExcludePlayerFromChatChannel(QString playerName);
};

/// CIP input::TGameActionFinishedTutorialForCharacter (0x100e63610).
class TGameActionFinishedTutorialForCharacter final : public TGameAction
{
public:
    TGameActionFinishedTutorialForCharacter() { type = EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER; }
};



/// CIP input::TGameActionFriendsDialogSearchAccount (0x100e64c20).
class TGameActionFriendsDialogSearchAccount final : public TGameAction
{
public:
    TGameActionFriendsDialogSearchAccount() { type = EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT; }
};

/// CIP input::TGameActionGetChatChannelList (0x100e58020).
class TGameActionGetChatChannelList final : public TGameAction
{
public:
    TGameActionGetChatChannelList() { type = EGameAction::GET_CHAT_CHANNEL_LIST; }
};

/// CIP input::TGameActionGetGuildMessage (0x100e62bf0).
class TGameActionGetGuildMessage final : public TGameAction
{
public:
    TGameActionGetGuildMessage() { type = EGameAction::GET_GUILD_MESSAGE; }
};

/// CIP input::TGameActionGetTextForReport (0x100e5a640).
class TGameActionGetTextForReport final : public TGameAction
{
public:
    TGameActionGetTextForReport() { type = EGameAction::GET_TEXT_FOR_REPORT; }
    explicit TGameActionGetTextForReport(QString text);
};

/// CIP input::TGameActionIgnoreName (0x100e5a960).
class TGameActionIgnoreName final : public TGameAction
{
public:
    TGameActionIgnoreName() { type = EGameAction::IGNORE_NAME; }
    explicit TGameActionIgnoreName(QString text);
};

/// CIP input::TGameActionInspectObject (0x100e61ca0).
class TGameActionInspectObject final : public TGameAction
{
public:
    TGameActionInspectObject() { type = EGameAction::INSPECT_OBJECT; }
    TGameActionInspectObject(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionInvitePlayerToChatChannel (0x100e58840).
class TGameActionInvitePlayerToChatChannel final : public TGameAction
{
public:
    TGameActionInvitePlayerToChatChannel() { type = EGameAction::INVITE_PLAYER_TO_CHAT_CHANNEL; }
    explicit TGameActionInvitePlayerToChatChannel(QString text);
};

/// CIP input::TGameActionInvitePlayerToPrivateChatChannel (0x100e58a70).
class TGameActionInvitePlayerToPrivateChatChannel final : public TGameAction
{
public:
    TGameActionInvitePlayerToPrivateChatChannel() { type = EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL; }
    explicit TGameActionInvitePlayerToPrivateChatChannel(QString text);
};

/// CIP input::TGameActionLoadDefaultMinimapTilesFromResources (0x101040890).
class TGameActionLoadDefaultMinimapTilesFromResources final : public TGameAction
{
public:
    TGameActionLoadDefaultMinimapTilesFromResources() { type = EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES; }
};

/// CIP input::TGameActionMakeBattleListPrimary (0x100e64040).
class TGameActionMakeBattleListPrimary final : public TGameAction
{
public:
    TGameActionMakeBattleListPrimary() { type = EGameAction::MAKE_BATTLE_LIST_PRIMARY; }
};


/// CIP input::TGameActionMountPlayer (0x100e624c0).
class TGameActionMountPlayer final : public TGameAction
{
public:
    TGameActionMountPlayer() { type = EGameAction::MOUNT_PLAYER; }
    explicit TGameActionMountPlayer(uint32_t creatureId);
};

/// CIP input::TGameActionMoveUp (0x100e57240).
class TGameActionMoveUp final : public TGameAction
{
public:
    TGameActionMoveUp() { type = EGameAction::MOVE_UP; }
    TGameActionMoveUp(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionOpenAddVipDialog (0x100e626a0).
class TGameActionOpenAddVipDialog final : public TGameActionVip
{
public:
    TGameActionOpenAddVipDialog() { type = EGameAction::OPEN_ADD_VIP_DIALOG; }
};

/// CIP input::TGameActionOpenAddVipGroupDialog (0x100e63a20).
class TGameActionOpenAddVipGroupDialog final : public TGameActionVipGroup
{
public:
    TGameActionOpenAddVipGroupDialog() { type = EGameAction::OPEN_ADD_VIP_GROUP_DIALOG; }
};

/// CIP input::TGameActionOpenContainerInSpecificSidebar (0x100e64000).
class TGameActionOpenContainerInSpecificSidebar final : public TGameAction
{
public:
    TGameActionOpenContainerInSpecificSidebar() { type = EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR; }
};

/// CIP input::TGameActionOpenCyclopedia (0x100e63db0).
class TGameActionOpenCyclopedia final : public TGameAction
{
public:
    TGameActionOpenCyclopedia() { type = EGameAction::OPEN_CYCLOPEDIA; }
};

/// CIP input::TGameActionOpenEditVipDialog (0x100e627e0).
class TGameActionOpenEditVipDialog final : public TGameActionVip
{
public:
    TGameActionOpenEditVipDialog() { type = EGameAction::OPEN_EDIT_VIP_DIALOG; }
    explicit TGameActionOpenEditVipDialog(QString text);
};

/// CIP input::TGameActionOpenEditVipGroupDialog (0x100e63ae0).
class TGameActionOpenEditVipGroupDialog final : public TGameActionVipGroup
{
public:
    TGameActionOpenEditVipGroupDialog() { type = EGameAction::OPEN_EDIT_VIP_GROUP_DIALOG; }
    explicit TGameActionOpenEditVipGroupDialog(QString text);
};

/// CIP input::TGameActionOpenGetPremiumUrl (0x100e62d20).
class TGameActionOpenGetPremiumUrl final : public TGameAction
{
public:
    TGameActionOpenGetPremiumUrl() { type = EGameAction::OPEN_URL_GET_PREMIUM; }
};

/// CIP input::TGameActionOpenHighscoresDialog (0x100e64d00).
class TGameActionOpenHighscoresDialog final : public TGameAction
{
public:
    TGameActionOpenHighscoresDialog() { type = EGameAction::OPEN_HIGHSCORES_DIALOG; }
};

/// CIP input::TGameActionOpenLootContainer (0x100e63fc0).
class TGameActionOpenLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionOpenLootContainer() { type = EGameAction::LOOT_CONTAINER_OPEN; }
};

/// CIP input::TGameActionOpenMessageToChatChannel (0x100e58090).
class TGameActionOpenMessageToChatChannel final : public TGameAction
{
public:
    TGameActionOpenMessageToChatChannel() { type = EGameAction::OPEN_MESSAGE_TO_CHAT_CHANNEL; }
    explicit TGameActionOpenMessageToChatChannel(QString text);
};

/// CIP input::TGameActionOpenNewsDialog (0x100e63cc0).
class TGameActionOpenNewsDialog final : public TGameAction
{
public:
    TGameActionOpenNewsDialog() { type = EGameAction::OPEN_NEWS_DIALOG; }
};

/// CIP input::TGameActionOpenNewsDialogWithCategory (0x100e64ce0).
class TGameActionOpenNewsDialogWithCategory final : public TGameAction
{
public:
    TGameActionOpenNewsDialogWithCategory() { type = EGameAction::OPEN_NEWS_CATEGORY; }
};

/// CIP input::TGameActionOpenObtainContainer (0x100d2cfe0).
class TGameActionOpenObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionOpenObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_OPEN; }
};

/// CIP input::TGameActionOpenOptionsMenuDialog (0x100e64130).
class TGameActionOpenOptionsMenuDialog final : public TGameAction
{
public:
    TGameActionOpenOptionsMenuDialog() { type = EGameAction::OPEN_OPTIONS_MENU_DIALOG; }
};

/// CIP input::TGameActionOpenParentContainer (0x100e64b50).
class TGameActionOpenParentContainer final : public TGameAction
{
public:
    TGameActionOpenParentContainer() { type = EGameAction::OPEN_PARENT_CONTAINER; }
    TGameActionOpenParentContainer(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::TGameActionOpenPreyDialog (0x100e64cc0).
class TGameActionOpenPreyDialog final : public TGameAction
{
public:
    TGameActionOpenPreyDialog() { type = EGameAction::OPEN_PREY_DIALOG; }
    explicit TGameActionOpenPreyDialog(uint32_t creatureId);
};

/// CIP input::TGameActionOpenQuestLog (0x100e63e60).
class TGameActionOpenQuestLog final : public TGameAction
{
public:
    TGameActionOpenQuestLog() { type = EGameAction::OPEN_QUESTLOG_DIALOG; }
};

/// CIP input::TGameActionOpenSecondaryBattleListInSpecificSidebar (0x100e64020).
class TGameActionOpenSecondaryBattleListInSpecificSidebar final : public TGameAction
{
public:
    TGameActionOpenSecondaryBattleListInSpecificSidebar() { type = EGameAction::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR; }
};

/// CIP input::TGameActionOpenShowImageDialog (0x100e64f50).
class TGameActionOpenShowImageDialog final : public TGameAction
{
public:
    TGameActionOpenShowImageDialog() { type = EGameAction::OPEN_SHOW_IMAGE_DIALOG; }
    explicit TGameActionOpenShowImageDialog(QString text);
};

/// CIP input::TGameActionOpenSocialDialog (0x100e64c00).
class TGameActionOpenSocialDialog final : public TGameAction
{
public:
    TGameActionOpenSocialDialog() { type = EGameAction::OPEN_SOCIAL_DIALOG; }
};

/// CIP input::TGameActionOpenStore (0x100e63790).
class TGameActionOpenStore final : public TGameAction
{
public:
    TGameActionOpenStore() { type = EGameAction::OPEN_STORE; }
};

/// CIP input::TGameActionOpenSurveyUrl (0x100e62e00).
class TGameActionOpenSurveyUrl final : public TGameAction
{
public:
    TGameActionOpenSurveyUrl() { type = EGameAction::OPEN_URL_SURVEY; }
};

/// CIP input::TGameActionOpenTradeCharacterDialog (0x100e64d20).
class TGameActionOpenTradeCharacterDialog final : public TGameAction
{
public:
    TGameActionOpenTradeCharacterDialog() { type = EGameAction::OPEN_TRADE_CHARACTER_DIALOG; }
};

/// CIP input::TGameActionOpenUrl (0x100e63c20).
class TGameActionOpenUrl final : public TGameAction
{
public:
    TGameActionOpenUrl() { type = EGameAction::OPEN_URL; }
    explicit TGameActionOpenUrl(QString text);
};

/// CIP input::TGameActionPreySystem (0x100e638a0).
class TGameActionPreySystem final : public TGameAction
{
public:
    TGameActionPreySystem() = default;
    explicit TGameActionPreySystem(EGameAction type);
};

/// CIP input::TGameActionQuitGame (0x100e61ed0).
class TGameActionQuitGame final : public TGameAction
{
public:
    TGameActionQuitGame() { type = EGameAction::QUIT_GAME; }
};

/// CIP input::TGameActionRecentlyTraded (0x100e5aaa0).
class TGameActionRecentlyTraded final : public TGameAction
{
public:
    TGameActionRecentlyTraded() { type = EGameAction::RECENTLY_TRADED; }
    explicit TGameActionRecentlyTraded(QString text);
};

/// CIP input::TGameActionRefreshCompendiumNews (0x100e64110).
class TGameActionRefreshCompendiumNews final : public TGameAction
{
public:
    TGameActionRefreshCompendiumNews() { type = EGameAction::REFRESH_COMPENDIUM_NEWS; }
};

/// CIP input::TGameActionRemoveAllTutorialMarkers (0x100e62f40).
class TGameActionRemoveAllTutorialMarkers final : public TGameAction
{
public:
    TGameActionRemoveAllTutorialMarkers() { type = EGameAction::REMOVE_ALL_TUTORIAL_MARKERS; }
};

/// CIP input::TGameActionRemoveSpriteIdentifierFromCache (0x100e64670).
class TGameActionRemoveSpriteIdentifierFromCache final : public TGameAction
{
public:
    TGameActionRemoveSpriteIdentifierFromCache() { type = EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE; }
};

/// CIP input::TGameActionRemoveVip (0x100e62750).
class TGameActionRemoveVip final : public TGameActionVip
{
public:
    TGameActionRemoveVip() { type = EGameAction::REMOVE_VIP; }
};

/// CIP input::TGameActionRemoveVipGroup (0x100e63a40).
class TGameActionRemoveVipGroup final : public TGameActionVipGroup
{
public:
    TGameActionRemoveVipGroup() { type = EGameAction::REMOVE_VIP_GROUP; }
};

/// CIP input::TGameActionReportCharacter (0x100e61530).
class TGameActionReportCharacter final : public TGameAction
{
public:
    TGameActionReportCharacter() { type = EGameAction::REPORT_NAME; }
    explicit TGameActionReportCharacter(QString text);
};

/// CIP input::TGameActionReportHireling (0x100e5a4d0).
class TGameActionReportHireling final : public TGameAction
{
public:
    TGameActionReportHireling() { type = EGameAction::REPORT_HIRELING_NAME; }
    explicit TGameActionReportHireling(uint32_t creatureId);
};

/// CIP input::TGameActionReportMapBug (0x100e62d00).
class TGameActionReportMapBug final : public TGameAction
{
public:
    TGameActionReportMapBug() { type = EGameAction::REPORT_BUG_MAP; }
};

/// CIP input::TGameActionReportStatement (0x100e5a440).
class TGameActionReportStatement final : public TGameAction
{
public:
    TGameActionReportStatement() { type = EGameAction::REPORT_STATEMENT; }
    explicit TGameActionReportStatement(QString text);
};

/// CIP input::TGameActionReportText (0x100e629f0).
class TGameActionReportText final : public TGameAction
{
public:
    TGameActionReportText() { type = EGameAction::REPORT_TEXT; }
    explicit TGameActionReportText(QString text);
};

/// CIP input::TGameActionReportTypo (0x100e62c10).
class TGameActionReportTypo final : public TGameAction
{
public:
    TGameActionReportTypo() { type = EGameAction::REPORT_BUG_TYPO; }
};

/// CIP input::TGameActionRequestConformationForCopyingTextToClipboard (0x100e620a0).
class TGameActionRequestConformationForCopyingTextToClipboard final : public TGameAction
{
public:
    TGameActionRequestConformationForCopyingTextToClipboard() { type = EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD; }
    explicit TGameActionRequestConformationForCopyingTextToClipboard(QString text);
};

/// CIP input::TGameActionRequestCopySelectedTextToClipboard (0x100e628b0).
class TGameActionRequestCopySelectedTextToClipboard final : public TGameAction
{
public:
    TGameActionRequestCopySelectedTextToClipboard() { type = EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD; }
};

/// CIP input::TGameActionRequestOutfitsFromServer (0x100e62500).
class TGameActionRequestOutfitsFromServer final : public TGameAction
{
public:
    TGameActionRequestOutfitsFromServer() { type = EGameAction::REQUEST_OUTFITS_FROM_SERVER; }
    explicit TGameActionRequestOutfitsFromServer(uint32_t creatureId);
};
/// CIP input::TGameActionRequestResourceBalance (0x100e63940).
class TGameActionRequestResourceBalance final : public TGameAction
{
public:
    TGameActionRequestResourceBalance() { type = EGameAction::REQUEST_RESOURCE_BALANCE; }
};
/// CIP input::TGameActionRequestSearchTypeInDepot (0x100e64b30).
class TGameActionRequestSearchTypeInDepot final : public TGameAction
{
public:
    TGameActionRequestSearchTypeInDepot() { type = EGameAction::REQUEST_SEARCH_TYPE_IN_DEPOT; }
};
/// CIP input::TGameActionRequestTransactionDetailsFromServer (0x100e64a00).
class TGameActionRequestTransactionDetailsFromServer final : public TGameAction
{
public:
    TGameActionRequestTransactionDetailsFromServer() { type = EGameAction::REQUEST_TRANSACTION_DETAILS; }
};
/// CIP input::TGameActionRequestVipDataUpdate (0x100e5a100).
class TGameActionRequestVipDataUpdate final : public TGameActionVip
{
public:
    TGameActionRequestVipDataUpdate() { type = EGameAction::REQUST_VIP_DATA_UPDATE; }
};
/// CIP input::TGameActionRequestVipGroupUpdate (0x100e5c5d0).
class TGameActionRequestVipGroupUpdate final : public TGameActionVipGroup
{
public:
    TGameActionRequestVipGroupUpdate() { type = EGameAction::REQUEST_VIP_GROUP_UPDATE; }
};
/// CIP input::TGameActionResolveSprite (0x100e646f0).
class TGameActionResolveSprite final : public TGameAction
{
public:
    TGameActionResolveSprite() { type = EGameAction::RESOLVE_SPRITE; }
};
/// CIP input::TGameActionSaveJsonDocumentToFile (0x100e64e30).
class TGameActionSaveJsonDocumentToFile final : public TGameAction
{
public:
    TGameActionSaveJsonDocumentToFile() { type = EGameAction::SAVE_JSON_TO_FILE; }
    explicit TGameActionSaveJsonDocumentToFile(QString filePath);
};
/// CIP input::TGameActionSaveTextToFile (0x100e64d40).
class TGameActionSaveTextToFile final : public TGameAction
{
public:
    TGameActionSaveTextToFile() { type = EGameAction::SAVE_TEXT_TO_FILE; }
    explicit TGameActionSaveTextToFile(QString filePath);
};
/// CIP input::TGameActionSelectAllText (0x100e62890).
class TGameActionSelectAllText final : public TGameAction
{
public:
    TGameActionSelectAllText() { type = EGameAction::REQUEST_SELECT_ALL_TEXT; }
};
/// CIP input::TGameActionSelectLootContainer (0x100e63e80).
class TGameActionSelectLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionSelectLootContainer() { type = EGameAction::LOOT_CONTAINER_SELECT; }
};
/// CIP input::TGameActionSelectObtainContainer (0x100d2cca0).
class TGameActionSelectObtainContainer final : public TGameActionManagedContainer
{
public:
    TGameActionSelectObtainContainer() { type = EGameAction::OBTAIN_CONTAINER_SELECT; }
};
/// CIP input::TGameActionSelectPreyMonsterFromBestiary (0x100e63900).
class TGameActionSelectPreyMonsterFromBestiary final : public TGameAction
{
public:
    TGameActionSelectPreyMonsterFromBestiary() { type = EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY; }
};
/// CIP input::TGameActionSelectPreyMonsterFromList (0x100e638c0).
class TGameActionSelectPreyMonsterFromList final : public TGameAction
{
public:
    TGameActionSelectPreyMonsterFromList() { type = EGameAction::PREY_SELECT_MONSTER_FROM_LIST; }
};
/// CIP input::TGameActionSendChatMessage (0x100e62f60).
class TGameActionSendChatMessage final : public TGameAction
{
public:
    TGameActionSendChatMessage() = default;
    explicit TGameActionSendChatMessage(EGameAction type);
};
/// CIP input::TGameActionSendChatMessageToNpc (0x100e63280).
class TGameActionSendChatMessageToNpc final : public TGameAction
{
public:
    TGameActionSendChatMessageToNpc() { type = EGameAction::SEND_CHAT_MESSAGE_TO_NPC; }
    explicit TGameActionSendChatMessageToNpc(QString text);
};
/// CIP input::TGameActionSendChatMessageWithChannelID (0x100e630a0).
class TGameActionSendChatMessageWithChannelID final : public TGameAction
{
public:
    TGameActionSendChatMessageWithChannelID() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID; }
};
/// CIP input::TGameActionSendChatMessageWithRecipient (0x100e63140).
class TGameActionSendChatMessageWithRecipient final : public TGameAction
{
public:
    TGameActionSendChatMessageWithRecipient() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_RECIPIENT; }
    explicit TGameActionSendChatMessageWithRecipient(QString text);
};
/// CIP input::TGameActionSendChatMessageWithSpeechVolume (0x100e63000).
class TGameActionSendChatMessageWithSpeechVolume final : public TGameAction
{
public:
    TGameActionSendChatMessageWithSpeechVolume() { type = EGameAction::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME; }
    explicit TGameActionSendChatMessageWithSpeechVolume(QString text);
};
/// CIP input::TGameActionSessiondumpPause (0x100e62ee0).
class TGameActionSessiondumpPause final : public TGameAction
{
public:
    TGameActionSessiondumpPause() { type = EGameAction::SESSIONDUMP_PAUSE; }
};
/// CIP input::TGameActionSessiondumpResume (0x100e62f00).
class TGameActionSessiondumpResume final : public TGameAction
{
public:
    TGameActionSessiondumpResume() { type = EGameAction::SESSIONDUMP_RESUME; }
};
/// CIP input::TGameActionSessiondumpSkipTo (0x100e62f20).
class TGameActionSessiondumpSkipTo final : public TGameAction
{
public:
    TGameActionSessiondumpSkipTo() { type = EGameAction::SESSIONDUMP_SKIP_TO; }
};
/// CIP input::TGameActionSessiondumpStart (0x100e64a20).
class TGameActionSessiondumpStart final : public TGameAction
{
public:
    TGameActionSessiondumpStart() { type = EGameAction::SESSIONDUMP_START; }
};
/// CIP input::TGameActionSetChaseMode (0x100e59b20).
class TGameActionSetChaseMode final : public TGameAction
{
public:
    TGameActionSetChaseMode() { type = EGameAction::SET_CHASE_OPPONENTS; }
};
/// CIP input::TGameActionSetCreaturePodiumConfiguration (0x100e5ef60).
class TGameActionSetCreaturePodiumConfiguration final : public TGameAction
{
public:
    TGameActionSetCreaturePodiumConfiguration() { type = EGameAction::SET_CREATURE_PODIUM_CONFIGURATION; }
};
/// CIP input::TGameActionSetCurrentChatChannelInputText (0x100e58960).
class TGameActionSetCurrentChatChannelInputText final : public TGameAction
{
public:
    TGameActionSetCurrentChatChannelInputText() { type = EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT; }
    explicit TGameActionSetCurrentChatChannelInputText(QString text);
};
/// CIP input::TGameActionSetHirelingName (0x100e62600).
class TGameActionSetHirelingName final : public TGameAction
{
public:
    TGameActionSetHirelingName() { type = EGameAction::SET_HIRELING_NAME; }
    explicit TGameActionSetHirelingName(QString text);
};
/// CIP input::TGameActionSetHirelingOutfit (0x100e625e0).
class TGameActionSetHirelingOutfit final : public TGameAction
{
public:
    TGameActionSetHirelingOutfit() { type = EGameAction::SET_HIRELING_OUTFIT; }
};
/// CIP input::TGameActionSetLootContainerFallback (0x100e63fe0).
class TGameActionSetLootContainerFallback final : public TGameActionManagedContainer
{
public:
    TGameActionSetLootContainerFallback() { type = EGameAction::LOOT_CONTAINER_FALLBACK; }
};
/// CIP input::TGameActionSetPlayerOutfit (0x100e59cf0).
class TGameActionSetPlayerOutfit final : public TGameAction
{
public:
    TGameActionSetPlayerOutfit() { type = EGameAction::SET_PLAYER_OUTFIT; }
};
/// CIP input::TGameActionSetPvPMode (0x100e59ac0).
class TGameActionSetPvPMode final : public TGameAction
{
public:
    TGameActionSetPvPMode() { type = EGameAction::SET_PVP_MODE; }
};
/// CIP input::TGameActionSetSecureMode (0x100e59b80).
class TGameActionSetSecureMode final : public TGameAction
{
public:
    TGameActionSetSecureMode() { type = EGameAction::SET_SECURE_MODE; }
};
/// CIP input::TGameActionSetSharedExperience (0x100e625c0).
class TGameActionSetSharedExperience final : public TGameAction
{
public:
    TGameActionSetSharedExperience() { type = EGameAction::SET_SHARED_EXPERIENCE; }
};
/// CIP input::TGameActionSetShowOffSocketConfiguration (0x100e5e9c0).
class TGameActionSetShowOffSocketConfiguration final : public TGameAction
{
public:
    TGameActionSetShowOffSocketConfiguration() { type = EGameAction::SET_SHOW_OFF_SOCKET_CONFIGURATION; }
};
/// CIP input::TGameActionSetStatusLine (0x100e595c0).
class TGameActionSetStatusLine final : public TGameAction
{
public:
    TGameActionSetStatusLine() { type = EGameAction::SET_STATUS_LINE; }
    explicit TGameActionSetStatusLine(QString text);
};
/// CIP input::TGameActionSetTactics (0x100e63880).
class TGameActionSetTactics final : public TGameAction
{
public:
    TGameActionSetTactics() { type = EGameAction::SET_TACTICS; }
};
/// CIP input::TGameActionShowConfirmMoveObjectToStashDialog (0x100e61e00).
class TGameActionShowConfirmMoveObjectToStashDialog final : public TGameAction
{
public:
    TGameActionShowConfirmMoveObjectToStashDialog() { type = EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG; }
    TGameActionShowConfirmMoveObjectToStashDialog(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionShowDialogCreateMinimapMarker (0x100e58d50).
class TGameActionShowDialogCreateMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    TGameActionShowDialogCreateMinimapMarker() { type = EGameAction::SHOW_DIALOG_CREATE_MINIMAP_MARKER; }
};
/// CIP input::TGameActionShowDialogEditMinimapMarker (0x100e591c0).
class TGameActionShowDialogEditMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    TGameActionShowDialogEditMinimapMarker() { type = EGameAction::SHOW_DIALOG_EDIT_MINIMAP_MARKER; }
};
/// CIP input::TGameActionShowExaltationResultDialog (0x100e64f30).
class TGameActionShowExaltationResultDialog final : public TGameAction
{
public:
    TGameActionShowExaltationResultDialog() { type = EGameAction::SHOW_EXALTATION_RESULT_DIALOG; }
};
/// CIP input::TGameActionShowLenshelp (0x100e5aee0).
class TGameActionShowLenshelp final : public TGameAction
{
public:
    TGameActionShowLenshelp() { type = EGameAction::LENSHELP_SHOW; }
};
/// CIP input::TGameActionShowMessageDialog (0x100e64a90).
class TGameActionShowMessageDialog final : public TGameAction
{
public:
    TGameActionShowMessageDialog() = default;
    explicit TGameActionShowMessageDialog(EGameAction type);
};
/// CIP input::TGameActionShowNaggingDialog (0x100e64fc0).
class TGameActionShowNaggingDialog final : public TGameAction
{
public:
    TGameActionShowNaggingDialog() { type = EGameAction::SHOW_NAGGING_DIALOG; }
};
/// CIP input::TGameActionShowSplitStackDialog (0x100e61d50).
class TGameActionShowSplitStackDialog final : public TGameAction
{
public:
    TGameActionShowSplitStackDialog() { type = EGameAction::SHOW_SPLIT_STACK_DIALOG; }
    TGameActionShowSplitStackDialog(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionShowTutorialHint (0x100e62ea0).
class TGameActionShowTutorialHint final : public TGameAction
{
public:
    TGameActionShowTutorialHint() { type = EGameAction::SHOW_TUTORIAL_HINT; }
};
/// CIP input::TGameActionSortFilterVipList (0x100e62870).
class TGameActionSortFilterVipList final : public TGameAction
{
public:
    TGameActionSortFilterVipList() = default;
    explicit TGameActionSortFilterVipList(EGameAction type);
};
/// CIP input::TGameActionSpriteResolved (0x100e64770).
class TGameActionSpriteResolved final : public TGameAction
{
public:
    TGameActionSpriteResolved() { type = EGameAction::SPRITE_RESOLVED; }
};
/// CIP input::TGameActionStartTalkingToNPC (0x100e5f5d0).
class TGameActionStartTalkingToNPC final : public TGameAction
{
public:
    TGameActionStartTalkingToNPC() { type = EGameAction::START_TALKING_TO_NPC; }
    explicit TGameActionStartTalkingToNPC(uint32_t creatureId);
};
/// CIP input::TGameActionStartTargetSelection (0x100e61eb0).
class TGameActionStartTargetSelection final : public TGameAction
{
public:
    TGameActionStartTargetSelection() { type = EGameAction::START_TARGET_SELECTION; }
};
/// CIP input::TGameActionStashOpenRetrieveDialog (0x100e645a0).
class TGameActionStashOpenRetrieveDialog final : public TGameAction
{
public:
    TGameActionStashOpenRetrieveDialog() { type = EGameAction::STASH_OPEN_RETRIEVE_DIALOG; }
};
/// CIP input::TGameActionStashOpenStowStackDialog (0x100e644d0).
class TGameActionStashOpenStowStackDialog final : public TGameAction
{
public:
    TGameActionStashOpenStowStackDialog() { type = EGameAction::STASH_OPEN_STOW_STACK_DIALOG; }
    TGameActionStashOpenStowStackDialog(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionStashRetrieve (0x100e64580).
class TGameActionStashRetrieve final : public TGameAction
{
public:
    TGameActionStashRetrieve() { type = EGameAction::STASH_RETRIEVE; }
};
/// CIP input::TGameActionStashStowContainerContent (0x100e64290).
class TGameActionStashStowContainerContent final : public TGameAction
{
public:
    TGameActionStashStowContainerContent() { type = EGameAction::STASH_STOW_CONTAINER_CONTENT; }
    TGameActionStashStowContainerContent(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionStashStowStack (0x100e643b0).
class TGameActionStashStowStack final : public TGameAction
{
public:
    TGameActionStashStowStack() { type = EGameAction::STASH_STOW_STACK; }
    TGameActionStashStowStack(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionStashStowType (0x100e64170).
class TGameActionStashStowType final : public TGameAction
{
public:
    TGameActionStashStowType() { type = EGameAction::STASH_STOW_TYPE; }
};
/// CIP input::TGameActionSwitchChatMode (0x100e63920).
class TGameActionSwitchChatMode final : public TGameAction
{
public:
    TGameActionSwitchChatMode() { type = EGameAction::SWITCH_CHAT_MODE; }
};
/// CIP input::TGameActionSwitchClientOptions (0x100e63390).
class TGameActionSwitchClientOptions final : public TGameAction
{
public:
    TGameActionSwitchClientOptions() { type = EGameAction::SWITCH_CLIENTOPTIONS_MODE; }
};
/// CIP input::TGameActionTakeAutomaticScreenshot (0x100e64150).
class TGameActionTakeAutomaticScreenshot final : public TGameAction
{
public:
    TGameActionTakeAutomaticScreenshot() { type = EGameAction::TAKE_AUTOMATIC_SCREENSHOT; }
};
/// CIP input::TGameActionTakeManualScreenshot (0x100e5f6b0).
class TGameActionTakeManualScreenshot final : public TGameAction
{
public:
    TGameActionTakeManualScreenshot() { type = EGameAction::TAKE_MANUAL_SCREENSHOT; }
};
/// CIP input::TGameActionTargetSelected (0x100e58140).
class TGameActionTargetSelected final : public TGameAction
{
public:
    TGameActionTargetSelected() { type = EGameAction::TARGET_SELECTED; }
    explicit TGameActionTargetSelected(uint32_t creatureId);
};
/// CIP input::TGameActionTeleportToCoordinate (0x100e62200).
class TGameActionTeleportToCoordinate final : public TGameAction
{
public:
    TGameActionTeleportToCoordinate() { type = EGameAction::GOD_TELEPORT; }
};
/// CIP input::TGameActionToggleActionBar (0x100e63ce0).
class TGameActionToggleActionBar final : public TGameAction
{
public:
    TGameActionToggleActionBar() { type = EGameAction::TOGGLE_ACTION_BAR; }
};
/// CIP input::TGameActionToggleActionBars (0x100e63d00).
class TGameActionToggleActionBars final : public TGameAction
{
public:
    TGameActionToggleActionBars() { type = EGameAction::TOGGLE_ACTION_BARS; }
};
/// CIP input::TGameActionToggleBlackWhitelistStatus (0x100e64060).
class TGameActionToggleBlackWhitelistStatus final : public TGameAction
{
public:
    TGameActionToggleBlackWhitelistStatus() = default;
    explicit TGameActionToggleBlackWhitelistStatus(EGameAction type);
};
/// CIP input::TGameActionToggleCreatureHudsVisible (0x100e62520).
class TGameActionToggleCreatureHudsVisible final : public TGameAction
{
public:
    TGameActionToggleCreatureHudsVisible() { type = EGameAction::TOGGLE_CREATURE_HUDS_VISIBLE; }
};
/// CIP input::TGameActionToggleMounted (0x100e58280).
class TGameActionToggleMounted final : public TGameAction
{
public:
    TGameActionToggleMounted() { type = EGameAction::TOGGLE_MOUNTED; }
};
/// CIP input::TGameActionToggleServerLogSubscriptionInCurrentChatChannel (0x100e62560).
class TGameActionToggleServerLogSubscriptionInCurrentChatChannel final : public TGameAction
{
public:
    TGameActionToggleServerLogSubscriptionInCurrentChatChannel() { type = EGameAction::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL; }
};
/// CIP input::TGameActionToggleWidget (0x100e63960).
class TGameActionToggleWidget final : public TGameAction
{
public:
    TGameActionToggleWidget() { type = EGameAction::TOGGLE_WIDGET; }
};
/// CIP input::TGameActionTryToEquip (0x100e62d40).
class TGameActionTryToEquip final : public TGameAction
{
public:
    TGameActionTryToEquip() { type = EGameAction::TRY_TO_EQUIP; }
    TGameActionTryToEquip(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionTryToSwitchHotkeySet (0x100e62d60).
class TGameActionTryToSwitchHotkeySet final : public TGameAction
{
public:
    TGameActionTryToSwitchHotkeySet() { type = EGameAction::TRY_TO_SWITCH_HOTKEYSET; }
    explicit TGameActionTryToSwitchHotkeySet(QString text);
};
/// CIP input::TGameActionTutorialHintConfirmed (0x100e62ec0).
class TGameActionTutorialHintConfirmed final : public TGameAction
{
public:
    TGameActionTutorialHintConfirmed() { type = EGameAction::CONFIRM_TUTORIAL_HINT; }
};
/// CIP input::TGameActionTutorialProgress (0x100e636b0).
class TGameActionTutorialProgress final : public TGameAction
{
public:
    TGameActionTutorialProgress() { type = EGameAction::TUTORIAL_PROGRESS; }
};
/// CIP input::TGameActionUnignoreName (0x100e5aa00).
class TGameActionUnignoreName final : public TGameAction
{
public:
    TGameActionUnignoreName() { type = EGameAction::UNIGNORE_NAME; }
    explicit TGameActionUnignoreName(QString text);
};
/// CIP input::TGameActionUnmountPlayer (0x100e624e0).
class TGameActionUnmountPlayer final : public TGameAction
{
public:
    TGameActionUnmountPlayer() { type = EGameAction::UNMOUNT_PLAYER; }
    explicit TGameActionUnmountPlayer(uint32_t creatureId);
};
/// CIP input::TGameActionUpdateExivaOptions (0x100e64080).
class TGameActionUpdateExivaOptions final : public TGameAction
{
public:
    TGameActionUpdateExivaOptions() { type = EGameAction::EXIVA_UPADATE_OPTIONS; }
};
/// CIP input::TGameActionUpdateMinimapMarker (0x100e62380).
class TGameActionUpdateMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    TGameActionUpdateMinimapMarker() { type = EGameAction::UPDATE_MINIMAP_MARKER; }
};
/// CIP input::TGameActionUseOnCreature (0x100e58210).
class TGameActionUseOnCreature final : public TGameAction
{
public:
    TGameActionUseOnCreature() { type = EGameAction::USE_ON_CREATURE; }
    TGameActionUseOnCreature(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionUseOnTarget (0x100e592c0).
class TGameActionUseOnTarget final : public TGameAction
{
public:
    TGameActionUseOnTarget() { type = EGameAction::USE_ON_TARGET; }
    TGameActionUseOnTarget(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionUseOnYourself (0x100e594f0).
class TGameActionUseOnYourself final : public TGameAction
{
public:
    TGameActionUseOnYourself() { type = EGameAction::USE_ON_YOURSELF; }
    TGameActionUseOnYourself(ThingPtr object, Position position, int stackPosition);
};
/// CIP input::TGameActionVisibleChatChannel (0x100e5d380).
class TGameActionVisibleChatChannel final : public TGameAction
{
public:
    TGameActionVisibleChatChannel() = default;
    explicit TGameActionVisibleChatChannel(EGameAction type);
};
namespace gameactions {

/// CIP input::gameactions::TGameActionGatherAll (v0 0x100e76fe0 → GATHER_ALL).
class TGameActionGatherAll final : public TGameAction
{
public:
    TGameActionGatherAll() { type = EGameAction::GATHER_ALL; }
    TGameActionGatherAll(ThingPtr object, Position position, int stackPosition);
};

/// CIP input::gameactions::TGameActionGameEvent (0x100b602d0).
class TGameActionGameEvent final : public TGameAction
{
public:
    TGameActionGameEvent() { type = EGameAction::GAME_EVENT; }
};
/// CIP input::gameactions::TGameActionMarketBrowse (0x100b120d0).
class TGameActionMarketBrowse final : public TGameAction
{
public:
    TGameActionMarketBrowse() { type = EGameAction::MARKET_BROWSE; }
};
/// CIP input::gameactions::TGameActionOpenTaskboardDialog (0x100b997f0).
class TGameActionOpenTaskboardDialog final : public TGameAction
{
public:
    TGameActionOpenTaskboardDialog() { type = EGameAction::OPEN_TASKBOARD_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyDialog (0x100b99760).
class TGameActionOpenWeaponProficiencyDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog (0x100e28cd0).
class TGameActionOpenWeaponProficiencyShapeDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyShapeDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG; }
};
/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog (0x100e2f6c0).
class TGameActionOpenWeaponProficiencyShapeOptionsDialog final : public TGameAction
{
public:
    TGameActionOpenWeaponProficiencyShapeOptionsDialog() { type = EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG; }
};
/// CIP input::gameactions::TGameActionSaveSkillWheel (0x100dfdb10).
class TGameActionSaveSkillWheel final : public TGameAction
{
public:
    TGameActionSaveSkillWheel() { type = EGameAction::SAVE_SKILL_WHEEL; }
};
/// CIP input::gameactions::TGameActionShowCreateCharacterDialog (0x1009814c0).
class TGameActionShowCreateCharacterDialog final : public TGameAction
{
public:
    TGameActionShowCreateCharacterDialog() { type = EGameAction::SHOW_CREATE_CHARACTER_DIALOG; }
};
/// CIP input::gameactions::TGameActionSimple (0x10091c6c0).
class TGameActionSimple final : public TGameAction
{
public:
    TGameActionSimple() = default;
    explicit TGameActionSimple(EGameAction type);
};
/// CIP input::gameactions::TGameActionSoundTrigger (0x100fb8320).
class TGameActionSoundTrigger final : public TGameAction
{
public:
    TGameActionSoundTrigger() { type = EGameAction::SOUND_TRIGGER; }
};
/// CIP input::gameactions::TSelectAtelierGemGameAction (0x100fa1cd0).
class TSelectAtelierGemGameAction final : public TGameAction
{
public:
    TSelectAtelierGemGameAction() { type = EGameAction::GEM_ATELIER_SELECT_GEM; }
};
/// CIP input::gameactions::TSelectFragmentWorkshopModGameAction (0x100e02bf0).
class TSelectFragmentWorkshopModGameAction final : public TGameAction
{
public:
    TSelectFragmentWorkshopModGameAction() { type = EGameAction::FRAGMENT_WORKSHOP_SELECT_MOD; }
};
}

TGameActionPtr makeAction(EGameAction type);
TGameActionPtr makeObjectAction(EGameAction type, const ThingPtr& object, const Position& position, int stackPosition);
TGameActionPtr makeCreatureAction(EGameAction type, uint32_t creatureId);
TGameActionPtr makeMovementIntent(const Position& position, int pathMode = 0);
TGameActionPtr makeSmartClick(const TGameActionPtr& inner);

/// Typed mutable copy: factory dynamic type + field assign (CIP handler reshapes).
std::shared_ptr<TGameAction> mutableClone(const TGameAction& action);

/// For each type in order, the first candidate of that type (CIP: FUN_100e6f550).
TGameActionList filterActions(const TGameActionList& candidates, std::span<const EGameAction> types);
inline TGameActionList filterActions(const TGameActionList& candidates, std::initializer_list<EGameAction> types)
{
    return filterActions(candidates, std::span<const EGameAction>(types.begin(), types.size()));
}

/// The candidates in context-menu order with separators between groups
/// (CIP: FUN_100e6f820).
TGameActionList contextMenuActions(const TGameActionList& candidates);
/// The same with a panel's own table (CONTEXT_MENU_SEPARATOR between groups).
TGameActionList contextMenuActions(const TGameActionList& candidates, std::span<const EGameAction> order);

/// A dragged object's drop (CIP: FUN_100d49560). Shift moves everything; a
/// stack asks for the amount when Ctrl's state matches option
/// dragAndDropDefaultActionIsMoveAll.
TGameActionPtr finalizeDrop(const TGameActionPtr& move, const Position& destination, int modifiers, bool defaultIsMoveAll);

}
