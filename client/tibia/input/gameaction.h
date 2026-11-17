#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>

#include <QColor>
#include <QJsonDocument>
#include <QRectF>
#include <QString>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <variant>
#include <memory>
#include <span>
#include <vector>

namespace tibia {
namespace chat {
struct TChatChannelIdentifier;
}
namespace input {

using EGameAction = cip::tibia::input::EGameAction;

/// One thing the player can do (CIP: input::IGameAction, the interface of every
/// input::TGameAction* class). Actions travel as shared pointers from the
/// controllers' publishGameAction signals through the game action router to
/// every handler's handleGameAction slot; a handler checks gameActionType()
/// and reads the class of that type.
class IGameAction
{
public:
    virtual ~IGameAction() = default;
    /// CIP v0: the action's EGameAction.
    virtual EGameAction gameActionType() const = 0;
    /// CIP v3: the keyboard shortcut handler still runs this action while its
    /// shortcuts are restricted (only TGameActionTakeManualScreenshot).
    virtual bool isAllowedWhileShortcutsAreRestricted() const { return false; }
};
using IGameActionPtr = std::shared_ptr<const IGameAction>;

/// The action as `T` when it is one (CIP: the handlers' static casts after the
/// type check); null otherwise.
template<typename T>
const T* gameActionAs(const IGameAction* action)
{
    return dynamic_cast<const T*>(action);
}
template<typename T>
const T* gameActionAs(const IGameActionPtr& action)
{
    return dynamic_cast<const T*>(action.get());
}

namespace gameactions {
/// CIP input::gameactions::TGameActionSimple: an action that carries only its
/// type, and the base of the classes that store their type.
class TGameActionSimple : public IGameAction
{
public:
    explicit TGameActionSimple(EGameAction type)
        : m_type(type)
    {
    }
    EGameAction gameActionType() const override { return m_type; }

private:
    EGameAction m_type;
};

/// CIP input::gameactions::TGameActionSoundTrigger (SOUND_TRIGGER): plays a sound
/// effect once (tibia::qmlcomponents::TSoundHelperService::ESound id), or pauses
/// (true) / resumes (false) the looping sounds.
class TGameActionSoundTrigger final : public TGameActionSimple
{
public:
    struct TSoundEffect
    {
        int id = 0;
    };
    using TTrigger = std::variant<bool, TSoundEffect>;

    explicit TGameActionSoundTrigger(TTrigger trigger)
        : TGameActionSimple(EGameAction::SOUND_TRIGGER)
        , m_trigger(trigger)
    {
    }
    /// The looping sounds pause (true) or resume (false); empty for a sound effect.
    std::optional<bool> pauseLoopingSounds() const;
    /// The sound effect to play; empty when the looping sounds pause or resume.
    std::optional<int> soundEffectId() const;

private:
    TTrigger m_trigger;
};
}

/// CIP input::TGameActionOpenCyclopedia (OPEN_CYCLOPEDIA): opens a tab of the
/// Cyclopedia (0 items, 1 bestiary, 2 character, 3 charms, 4 map, 5 houses,
/// 6 bosstiary, 7 boss slots, 8 magical archive) and what it shows there.
class TGameActionOpenCyclopedia final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionOpenCyclopedia(int tab)
        : TGameActionSimple(EGameAction::OPEN_CYCLOPEDIA)
        , m_tab(tab)
    {
    }
    int tab() const { return m_tab; }
    /// Bestiary / bosstiary: the race shown (0: none).
    uint16_t raceId() const { return m_raceId; }
    TGameActionOpenCyclopedia& setRaceId(uint16_t raceId);
    /// Map: the field the map centres on (CIP TWorldMapCoordinate, 0,0,0 for none).
    const Position& coordinate() const { return m_coordinate; }
    TGameActionOpenCyclopedia& setCoordinate(const Position& coordinate);
    /// Map: the zoom at the coordinate (NaN: the map's own).
    float zoom() const { return m_zoom; }
    TGameActionOpenCyclopedia& setZoom(float zoom);
    /// Character: the TCharacterInfoDialogController::EDialogCategory shown.
    std::optional<int> category() const { return m_category; }
    TGameActionOpenCyclopedia& setCategory(int category);
    /// Items: the item type shown; magical archive: the spell (0: none).
    uint16_t typeId() const { return m_typeId; }
    TGameActionOpenCyclopedia& setTypeId(uint16_t typeId);
    /// Charms: the charm selected (0xff: none).
    uint8_t charmId() const { return m_charmId; }
    TGameActionOpenCyclopedia& setCharmId(uint8_t charmId);

private:
    int m_tab;
    uint16_t m_raceId = 0;
    Position m_coordinate{ 0, 0, 0 };
    float m_zoom = std::numeric_limits<float>::quiet_NaN();
    std::optional<int> m_category;
    uint16_t m_typeId = 0;
    uint8_t m_charmId = 0xff;
};

/// CIP input::TGameActionCreature: an action on one creature (attack, follow,
/// their stops, look at, temporary select, copy name with id, party, inspect,
/// join aggression, request skill wheel).
class TGameActionCreature : public IGameAction
{
public:
    TGameActionCreature(EGameAction type, uint32_t creatureId)
        : m_type(type)
        , m_creatureId(creatureId)
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    /// CIP v4.
    uint32_t creatureId() const { return m_creatureId; }

private:
    EGameAction m_type;
    uint32_t m_creatureId;
};

/// CIP input::TGameActionStartTalkingToNPC (START_TALKING_TO_NPC): greets the NPC.
class TGameActionStartTalkingToNPC final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionStartTalkingToNPC(uint32_t creatureId)
        : TGameActionSimple(EGameAction::START_TALKING_TO_NPC)
        , m_creatureId(creatureId)
    {
    }
    /// CIP v4.
    uint32_t creatureId() const { return m_creatureId; }

private:
    uint32_t m_creatureId;
};

/// CIP input::TGameActionAttackFirstTarget (ATTACK_FIRST_TARGET).
class TGameActionAttackFirstTarget final : public gameactions::TGameActionSimple
{
public:
    TGameActionAttackFirstTarget()
        : TGameActionSimple(EGameAction::ATTACK_FIRST_TARGET)
    {
    }
};

/// CIP input::TGameActionAttackNextTarget (ATTACK_NEXT_TARGET).
class TGameActionAttackNextTarget final : public gameactions::TGameActionSimple
{
public:
    TGameActionAttackNextTarget()
        : TGameActionSimple(EGameAction::ATTACK_NEXT_TARGET)
    {
    }
};

/// CIP input::TGameActionAttackPreviousTarget (ATTACK_PREVIOUS_TARGET).
class TGameActionAttackPreviousTarget final : public gameactions::TGameActionSimple
{
public:
    TGameActionAttackPreviousTarget()
        : TGameActionSimple(EGameAction::ATTACK_PREVIOUS_TARGET)
    {
    }
};

/// CIP input::TGameActionChangeCharacter (CHANGE_CHARACTER).
class TGameActionChangeCharacter final : public gameactions::TGameActionSimple
{
public:
    TGameActionChangeCharacter()
        : TGameActionSimple(EGameAction::CHANGE_CHARACTER)
    {
    }
};

/// CIP input::TGameActionClearOldestMessageFromMapwindow (CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW).
class TGameActionClearOldestMessageFromMapwindow final : public gameactions::TGameActionSimple
{
public:
    TGameActionClearOldestMessageFromMapwindow()
        : TGameActionSimple(EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW)
    {
    }
};

/// CIP input::TGameActionCloseCurrentChatChannel (CLOSE_CURRENT_CHAT_CHANNEL).
class TGameActionCloseCurrentChatChannel final : public gameactions::TGameActionSimple
{
public:
    TGameActionCloseCurrentChatChannel()
        : TGameActionSimple(EGameAction::CLOSE_CURRENT_CHAT_CHANNEL)
    {
    }
};

/// CIP input::TGameActionDisplayNextChatChannelTab (DISPLAY_NEXT_CHAT_CHANNEL_TAB).
class TGameActionDisplayNextChatChannelTab final : public gameactions::TGameActionSimple
{
public:
    TGameActionDisplayNextChatChannelTab()
        : TGameActionSimple(EGameAction::DISPLAY_NEXT_CHAT_CHANNEL_TAB)
    {
    }
};

/// CIP input::TGameActionDisplayPreviousChatChannelTab (DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB).
class TGameActionDisplayPreviousChatChannelTab final : public gameactions::TGameActionSimple
{
public:
    TGameActionDisplayPreviousChatChannelTab()
        : TGameActionSimple(EGameAction::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB)
    {
    }
};

/// CIP input::TGameActionGetGuildMessage (GET_GUILD_MESSAGE).
class TGameActionGetGuildMessage final : public gameactions::TGameActionSimple
{
public:
    TGameActionGetGuildMessage()
        : TGameActionSimple(EGameAction::GET_GUILD_MESSAGE)
    {
    }
};

/// CIP input::TGameActionMountPlayer (MOUNT_PLAYER).
class TGameActionMountPlayer final : public gameactions::TGameActionSimple
{
public:
    TGameActionMountPlayer()
        : TGameActionSimple(EGameAction::MOUNT_PLAYER)
    {
    }
};

/// CIP input::TGameActionOpenAddVipDialog (OPEN_ADD_VIP_DIALOG).
class TGameActionOpenAddVipDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenAddVipDialog()
        : TGameActionSimple(EGameAction::OPEN_ADD_VIP_DIALOG)
    {
    }
};

/// CIP input::TGameActionOpenAddVipGroupDialog (OPEN_ADD_VIP_GROUP_DIALOG).
class TGameActionOpenAddVipGroupDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenAddVipGroupDialog()
        : TGameActionSimple(EGameAction::OPEN_ADD_VIP_GROUP_DIALOG)
    {
    }
};

/// CIP input::TGameActionOpenHighscoresDialog (OPEN_HIGHSCORES_DIALOG).
class TGameActionOpenHighscoresDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenHighscoresDialog()
        : TGameActionSimple(EGameAction::OPEN_HIGHSCORES_DIALOG)
    {
    }
};

/// CIP input::TGameActionOpenPreyDialog (OPEN_PREY_DIALOG).
class TGameActionOpenPreyDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenPreyDialog()
        : TGameActionSimple(EGameAction::OPEN_PREY_DIALOG)
    {
    }
};

/// CIP input::TGameActionOpenTradeCharacterDialog (OPEN_TRADE_CHARACTER_DIALOG).
class TGameActionOpenTradeCharacterDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenTradeCharacterDialog()
        : TGameActionSimple(EGameAction::OPEN_TRADE_CHARACTER_DIALOG)
    {
    }
};

/// CIP input::TGameActionRemoveAllTutorialMarkers (REMOVE_ALL_TUTORIAL_MARKERS).
class TGameActionRemoveAllTutorialMarkers final : public gameactions::TGameActionSimple
{
public:
    TGameActionRemoveAllTutorialMarkers()
        : TGameActionSimple(EGameAction::REMOVE_ALL_TUTORIAL_MARKERS)
    {
    }
};

/// CIP input::TGameActionRequestCopySelectedTextToClipboard (REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD).
class TGameActionRequestCopySelectedTextToClipboard final : public gameactions::TGameActionSimple
{
public:
    TGameActionRequestCopySelectedTextToClipboard()
        : TGameActionSimple(EGameAction::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD)
    {
    }
};

/// CIP input::TGameActionSelectAllText (REQUEST_SELECT_ALL_TEXT).
class TGameActionSelectAllText final : public gameactions::TGameActionSimple
{
public:
    TGameActionSelectAllText()
        : TGameActionSimple(EGameAction::REQUEST_SELECT_ALL_TEXT)
    {
    }
};

/// CIP input::TGameActionSessiondumpPause (SESSIONDUMP_PAUSE).
class TGameActionSessiondumpPause final : public gameactions::TGameActionSimple
{
public:
    TGameActionSessiondumpPause()
        : TGameActionSimple(EGameAction::SESSIONDUMP_PAUSE)
    {
    }
};

/// CIP input::TGameActionSessiondumpResume (SESSIONDUMP_RESUME).
class TGameActionSessiondumpResume final : public gameactions::TGameActionSimple
{
public:
    TGameActionSessiondumpResume()
        : TGameActionSimple(EGameAction::SESSIONDUMP_RESUME)
    {
    }
};

/// CIP input::TGameActionStartTargetSelection (START_TARGET_SELECTION).
class TGameActionStartTargetSelection final : public gameactions::TGameActionSimple
{
public:
    TGameActionStartTargetSelection()
        : TGameActionSimple(EGameAction::START_TARGET_SELECTION)
    {
    }
};

/// CIP input::TGameActionTakeManualScreenshot (TAKE_MANUAL_SCREENSHOT).
class TGameActionTakeManualScreenshot final : public gameactions::TGameActionSimple
{
public:
    TGameActionTakeManualScreenshot()
        : TGameActionSimple(EGameAction::TAKE_MANUAL_SCREENSHOT)
    {
    }
    /// CIP v3: runs while the keyboard shortcuts are restricted.
    bool isAllowedWhileShortcutsAreRestricted() const override { return true; }
};

/// CIP input::TGameActionToggleCreatureHudsVisible (TOGGLE_CREATURE_HUDS_VISIBLE).
class TGameActionToggleCreatureHudsVisible final : public gameactions::TGameActionSimple
{
public:
    TGameActionToggleCreatureHudsVisible()
        : TGameActionSimple(EGameAction::TOGGLE_CREATURE_HUDS_VISIBLE)
    {
    }
};

/// CIP input::TGameActionToggleServerLogSubscriptionInCurrentChatChannel (TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL).
class TGameActionToggleServerLogSubscriptionInCurrentChatChannel final : public gameactions::TGameActionSimple
{
public:
    TGameActionToggleServerLogSubscriptionInCurrentChatChannel()
        : TGameActionSimple(EGameAction::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL)
    {
    }
};

/// CIP input::TGameActionUnmountPlayer (UNMOUNT_PLAYER).
class TGameActionUnmountPlayer final : public gameactions::TGameActionSimple
{
public:
    TGameActionUnmountPlayer()
        : TGameActionSimple(EGameAction::UNMOUNT_PLAYER)
    {
    }
};

/// qtclient's encoding of CIP's location and object action contexts
/// (TLocationWorldMapGameActionContext / TLocationInventoryGameActionContext /
/// TLocationContainerWithSlotGameActionContext with TObjectApperanceTypeGameActionContext):
/// the field (an inventory slot is Position(0xFFFF, slot, 0), a container slot
/// Position(0xFFFF, 0x40 | container, slot)), the stack position and the item's client id.
struct TObjectGameActionContext
{
    Position position;
    int stackPosition = -1;
    uint16_t objectClientId = 0;
};

/// The object actions CIP builds from a location and an object context.
class TObjectGameAction : public IGameAction
{
public:
    explicit TObjectGameAction(const TObjectGameActionContext& context)
        : m_context(context)
    {
    }
    const TObjectGameActionContext& context() const { return m_context; }
    const Position& position() const { return m_context.position; }
    int stackPosition() const { return m_context.stackPosition; }
    uint16_t objectClientId() const { return m_context.objectClientId; }

private:
    TObjectGameActionContext m_context;
};

/// CIP input::TGameActionLook (LOOK).
class TGameActionLook final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::LOOK; }
};

/// CIP input::TGameActionUse (USE).
class TGameActionUse final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::USE; }
};

/// CIP input::TGameActionOpen (OPEN).
class TGameActionOpen final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::OPEN; }
};

/// CIP input::TGameActionOpenInNewWindow (OPEN_IN_NEW_WINDOW).
class TGameActionOpenInNewWindow final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::OPEN_IN_NEW_WINDOW; }
};

/// CIP input::TGameActionWrap (WRAP).
class TGameActionWrap final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::WRAP; }
};

/// CIP input::TGameActionUnwrap (UNWRAP).
class TGameActionUnwrap final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::UNWRAP; }
};

/// CIP input::TGameActionRotate (ROTATE_OBJECT).
class TGameActionRotate final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::ROTATE_OBJECT; }
};

/// CIP input::TGameActionBrowseField (BROWSE_FIELD).
class TGameActionBrowseField final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::BROWSE_FIELD; }
};

/// CIP input::TGameActionMoveUp (MOVE_UP).
class TGameActionMoveUp final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::MOVE_UP; }
};

/// CIP input::TGameActionConfigureShowOffSocket (CONFIGURE_SHOW_OFF_SOCKET).
class TGameActionConfigureShowOffSocket final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::CONFIGURE_SHOW_OFF_SOCKET; }
};

/// CIP input::TGameActionShowLenshelp (LENSHELP_SHOW, 0xc4): the lenshelp text
/// of an object (its appearance's lenshelp id) at its field and the scene
/// rectangle the help points at.
class TGameActionShowLenshelp final : public IGameAction
{
public:
    TGameActionShowLenshelp(int lensHelpId, const Position& coordinate, const QRectF& rect)
        : m_lensHelpId(lensHelpId)
        , m_coordinate(coordinate)
        , m_rect(rect)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::LENSHELP_SHOW; }
    int lensHelpId() const { return m_lensHelpId; }
    const Position& coordinate() const { return m_coordinate; }
    const QRectF& rect() const { return m_rect; }

private:
    int m_lensHelpId;
    Position m_coordinate;
    QRectF m_rect;
};


// ---------------------------------------------------------------------------
// Chat

/// CIP input::TGameActionChatChannel: an action on one chat channel (open,
/// open and make current, close, clear, server log subscription, mute, show /
/// select / hide as secondary).
class TGameActionChatChannel : public gameactions::TGameActionSimple
{
public:
    TGameActionChatChannel(EGameAction type, std::shared_ptr<const chat::TChatChannelIdentifier> identifier);
    const chat::TChatChannelIdentifier& chatChannelIdentifier() const { return *m_identifier; }

private:
    std::shared_ptr<const chat::TChatChannelIdentifier> m_identifier;
};

/// CIP input::TGameActionOpenMessageToChatChannel (OPEN_MESSAGE_TO_CHAT_CHANNEL):
/// opens the private conversation with a player (TMessageToChatChannelIdentifier).
class TGameActionOpenMessageToChatChannel final : public TGameActionChatChannel
{
public:
    explicit TGameActionOpenMessageToChatChannel(const QString& name);
};

/// CIP input::TGameActionVisibleChatChannel: an action on the channel shown in
/// the primary or (true) the secondary pane (select all, save to file, invite /
/// exclude dialogs).
class TGameActionVisibleChatChannel final : public IGameAction
{
public:
    TGameActionVisibleChatChannel(EGameAction type, bool secondaryChannel)
        : m_type(type)
        , m_secondaryChannel(secondaryChannel)
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    bool isSecondaryChannel() const { return m_secondaryChannel; }

private:
    EGameAction m_type;
    bool m_secondaryChannel;
};

/// CIP input::TGameActionChatChannelModeration: invites a player to, or
/// excludes one from, a chat channel.
class TGameActionChatChannelModeration : public IGameAction
{
public:
    TGameActionChatChannelModeration(uint16_t channelId, QString playerName)
        : m_channelId(channelId)
        , m_playerName(std::move(playerName))
    {
    }
    uint16_t channelId() const { return m_channelId; }
    const QString& playerName() const { return m_playerName; }

private:
    uint16_t m_channelId;
    QString m_playerName;
};

/// CIP input::TGameActionInvitePlayerToChatChannel (INVITE_PLAYER_TO_CHAT_CHANNEL).
class TGameActionInvitePlayerToChatChannel final : public TGameActionChatChannelModeration
{
public:
    using TGameActionChatChannelModeration::TGameActionChatChannelModeration;
    EGameAction gameActionType() const override { return EGameAction::INVITE_PLAYER_TO_CHAT_CHANNEL; }
};

/// CIP input::TGameActionInvitePlayerToPrivateChatChannel (INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL).
class TGameActionInvitePlayerToPrivateChatChannel final : public TGameActionChatChannelModeration
{
public:
    using TGameActionChatChannelModeration::TGameActionChatChannelModeration;
    EGameAction gameActionType() const override { return EGameAction::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL; }
};

/// CIP input::TGameActionExcludePlayerFromChatChannel (EXCLUDE_PLAYER_FROM_CHAT_CHANNEL).
class TGameActionExcludePlayerFromChatChannel final : public TGameActionChatChannelModeration
{
public:
    using TGameActionChatChannelModeration::TGameActionChatChannelModeration;
    EGameAction gameActionType() const override { return EGameAction::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL; }
};

/// qtclient: the CIP classes whose only field is a player name (+0x8) and
/// whose v0 returns their type.
template<EGameAction Type>
class TGameActionWithName : public IGameAction
{
public:
    explicit TGameActionWithName(QString name)
        : m_name(std::move(name))
    {
    }
    EGameAction gameActionType() const override { return Type; }
    const QString& name() const { return m_name; }

private:
    QString m_name;
};

/// CIP input::TGameActionIgnoreName (IGNORE_NAME).
class TGameActionIgnoreName final : public TGameActionWithName<EGameAction::IGNORE_NAME>
{
public:
    using TGameActionWithName::TGameActionWithName;
};

/// CIP input::TGameActionUnignoreName (UNIGNORE_NAME).
class TGameActionUnignoreName final : public TGameActionWithName<EGameAction::UNIGNORE_NAME>
{
public:
    using TGameActionWithName::TGameActionWithName;
};

/// CIP input::TGameActionRecentlyTraded (RECENTLY_TRADED): the trade history
/// with a player.
class TGameActionRecentlyTraded final : public TGameActionWithName<EGameAction::RECENTLY_TRADED>
{
public:
    using TGameActionWithName::TGameActionWithName;
};

/// CIP input::TGameActionCreatureWithName: a creature action that also names
/// the creature (the creature id is 0 for a name from a chat line or the VIP list).
class TGameActionCreatureWithName final : public TGameActionCreature
{
public:
    TGameActionCreatureWithName(EGameAction type, uint32_t creatureId, QString name)
        : TGameActionCreature(type, creatureId)
        , m_name(std::move(name))
    {
    }
    /// CIP v5.
    const QString& creatureName() const { return m_name; }

private:
    QString m_name;
};

/// CIP input::TGameActionSetCurrentChatChannelInputText
/// (SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT): puts text into the chat input, or
/// sends it right away; a hotkey's text may cast a spell.
class TGameActionSetCurrentChatChannelInputText final : public IGameAction
{
public:
    TGameActionSetCurrentChatChannelInputText(QString text, bool sendAutomatically)
        : m_text(std::move(text))
        , m_sendAutomatically(sendAutomatically)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT; }
    const QString& text() const { return m_text; }
    bool sendAutomatically() const { return m_sendAutomatically; }
    /// The text casts a spell (+0x21) with this ESpellCastOptionalTargetType (+0x24).
    bool isSpell() const { return m_isSpell; }
    int spellCastOptionalTargetType() const { return m_spellCastOptionalTargetType; }
    TGameActionSetCurrentChatChannelInputText& setSpell(bool isSpell, int spellCastOptionalTargetType);

private:
    QString m_text;
    bool m_sendAutomatically;
    bool m_isSpell = false;
    int m_spellCastOptionalTargetType = 0;
};

/// CIP input::TGameActionSwitchChatMode (SWITCH_CHAT_MODE): the
/// chat::TChatModeManager::EChatMode to switch to.
class TGameActionSwitchChatMode final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionSwitchChatMode(int chatMode)
        : TGameActionSimple(EGameAction::SWITCH_CHAT_MODE)
        , m_chatMode(chatMode)
    {
    }
    int chatMode() const { return m_chatMode; }

private:
    int m_chatMode;
};

/// CIP input::TGameActionGetChatChannelList (GET_CHAT_CHANNEL_LIST).
class TGameActionGetChatChannelList final : public IGameAction
{
public:
    EGameAction gameActionType() const override { return EGameAction::GET_CHAT_CHANNEL_LIST; }
};

/// CIP input::TGameActionSendChatMessage: says a text (the base of the
/// SEND_CHAT_MESSAGE_* classes; SEND_CHAT_MESSAGE_AS_GAMEMASTER_BROADCAST uses it as is).
class TGameActionSendChatMessage : public gameactions::TGameActionSimple
{
public:
    TGameActionSendChatMessage(QString text, EGameAction type)
        : TGameActionSimple(type)
        , m_text(std::move(text))
    {
    }
    const QString& text() const { return m_text; }

private:
    QString m_text;
};

/// CIP input::TGameActionSendChatMessageWithSpeechVolume
/// (SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME): says, whispers or yells (the
/// tibia::client::MessageMode).
class TGameActionSendChatMessageWithSpeechVolume final : public TGameActionSendChatMessage
{
public:
    TGameActionSendChatMessageWithSpeechVolume(QString text, int speechVolume)
        : TGameActionSendChatMessage(std::move(text), EGameAction::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME)
        , m_speechVolume(speechVolume)
    {
    }
    int speechVolume() const { return m_speechVolume; }

private:
    int m_speechVolume;
    int m_reserved = 0; // CIP +0x2c, zeroed by the ctor
};

/// CIP input::TGameActionSendChatMessageWithChannelID (SEND_CHAT_MESSAGE_WITH_CHANNEL_ID).
class TGameActionSendChatMessageWithChannelID final : public TGameActionSendChatMessage
{
public:
    TGameActionSendChatMessageWithChannelID(QString text, uint16_t channelId, bool flag)
        : TGameActionSendChatMessage(std::move(text), EGameAction::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID)
        , m_channelId(channelId)
        , m_flag(flag)
    {
    }
    uint16_t channelId() const { return m_channelId; }
    /// CIP +0x2a (meaning not read yet).
    bool flag() const { return m_flag; }

private:
    uint16_t m_channelId;
    bool m_flag;
};

/// CIP input::TGameActionSendChatMessageWithRecipient (SEND_CHAT_MESSAGE_WITH_RECIPIENT):
/// a private message.
class TGameActionSendChatMessageWithRecipient final : public TGameActionSendChatMessage
{
public:
    TGameActionSendChatMessageWithRecipient(QString text, QString recipient, QString second, uint16_t channelId, int mode)
        : TGameActionSendChatMessage(std::move(text), EGameAction::SEND_CHAT_MESSAGE_WITH_RECIPIENT)
        , m_recipient(std::move(recipient))
        , m_second(std::move(second))
        , m_channelId(channelId)
        , m_mode(mode)
    {
    }
    const QString& recipient() const { return m_recipient; }

private:
    QString m_recipient;
    QString m_second;
    uint16_t m_channelId;
    int m_mode;
};

/// CIP input::TGameActionSendChatMessageToNpc (SEND_CHAT_MESSAGE_TO_NPC).
class TGameActionSendChatMessageToNpc final : public TGameActionSendChatMessage
{
public:
    TGameActionSendChatMessageToNpc(QString text, QString npcName, uint16_t channelId)
        : TGameActionSendChatMessage(std::move(text), EGameAction::SEND_CHAT_MESSAGE_TO_NPC)
        , m_npcName(std::move(npcName))
        , m_channelId(channelId)
    {
    }
    const QString& npcName() const { return m_npcName; }

private:
    QString m_npcName;
    uint16_t m_channelId;
};

// ---------------------------------------------------------------------------
// Clipboard

/// CIP input::TGameActionCopyTextToClipboard: copies a text (COPY_TEXT_TO_CLIPBOARD)
/// and the base of the other copy actions.
class TGameActionCopyTextToClipboard : public IGameAction
{
public:
    explicit TGameActionCopyTextToClipboard(QString text)
        : TGameActionCopyTextToClipboard(EGameAction::COPY_TEXT_TO_CLIPBOARD, std::move(text))
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const QString& text() const { return m_text; }

protected:
    TGameActionCopyTextToClipboard(EGameAction type, QString text)
        : m_type(type)
        , m_text(std::move(text))
    {
    }

private:
    EGameAction m_type;
    QString m_text;
};

/// CIP input::TGameActionCopyChatEntryToClipboard (COPY_CHAT_ENTRY_TO_CLIPBOARD).
class TGameActionCopyChatEntryToClipboard final : public TGameActionCopyTextToClipboard
{
public:
    explicit TGameActionCopyChatEntryToClipboard(QString text)
        : TGameActionCopyTextToClipboard(EGameAction::COPY_CHAT_ENTRY_TO_CLIPBOARD, std::move(text))
    {
    }
};

/// CIP input::TGameActionCopyCreatureNameToClipboard (COPY_CREATURE_NAME_TO_CLIPBOARD).
class TGameActionCopyCreatureNameToClipboard final : public TGameActionCopyTextToClipboard
{
public:
    explicit TGameActionCopyCreatureNameToClipboard(QString name)
        : TGameActionCopyTextToClipboard(EGameAction::COPY_CREATURE_NAME_TO_CLIPBOARD, std::move(name))
    {
    }
};

/// CIP input::TGameActionCopyTrustedTextToClipboard (COPY_TRUSTED_TEXT_TO_CLIPBOARD):
/// text the client wrote itself, copied without asking.
class TGameActionCopyTrustedTextToClipboard final : public TGameActionCopyTextToClipboard
{
public:
    explicit TGameActionCopyTrustedTextToClipboard(QString text)
        : TGameActionCopyTextToClipboard(EGameAction::COPY_TRUSTED_TEXT_TO_CLIPBOARD, std::move(text))
    {
    }
};

/// CIP input::TGameActionRequestConformationForCopyingTextToClipboard
/// (REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD).
class TGameActionRequestConformationForCopyingTextToClipboard final : public TGameActionCopyTextToClipboard
{
public:
    explicit TGameActionRequestConformationForCopyingTextToClipboard(QString text)
        : TGameActionCopyTextToClipboard(EGameAction::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD, std::move(text))
    {
    }
};

// ---------------------------------------------------------------------------
// VIP list

/// CIP input::TGameActionVip: an action on a VIP by name (name +0x8, type +0x20);
/// EXIVA_ADD/REMOVE_CHARACTER_TO/FROM_WHITELIST use it as is.
class TGameActionVip : public IGameAction
{
public:
    TGameActionVip(QString name, EGameAction type)
        : m_name(std::move(name))
        , m_type(type)
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const QString& vipName() const { return m_name; }

private:
    QString m_name;
    EGameAction m_type;
};

/// CIP input::TGameActionAddNewVip (ADD_NEW_VIP).
class TGameActionAddNewVip final : public TGameActionVip
{
public:
    explicit TGameActionAddNewVip(QString name)
        : TGameActionVip(std::move(name), EGameAction::ADD_NEW_VIP)
    {
    }
};

/// CIP input::TGameActionRemoveVip (REMOVE_VIP).
class TGameActionRemoveVip final : public TGameActionVip
{
public:
    explicit TGameActionRemoveVip(QString name)
        : TGameActionVip(std::move(name), EGameAction::REMOVE_VIP)
    {
    }
};

/// CIP input::TGameActionOpenEditVipDialog (OPEN_EDIT_VIP_DIALOG).
class TGameActionOpenEditVipDialog final : public TGameActionVip
{
public:
    explicit TGameActionOpenEditVipDialog(QString name)
        : TGameActionVip(std::move(name), EGameAction::OPEN_EDIT_VIP_DIALOG)
    {
    }
};

/// CIP input::TGameActionRequestVipDataUpdate (REQUST_VIP_DATA_UPDATE): the
/// edit-VIP dialog's result.
class TGameActionRequestVipDataUpdate final : public IGameAction
{
public:
    TGameActionRequestVipDataUpdate(uint32_t vipId, QString description, uint8_t icon, bool notifyOnLogin,
                                    std::vector<uint8_t> groupIds)
        : m_vipId(vipId)
        , m_description(std::move(description))
        , m_icon(icon)
        , m_notifyOnLogin(notifyOnLogin)
        , m_groupIds(std::move(groupIds))
    {
    }
    EGameAction gameActionType() const override { return EGameAction::REQUST_VIP_DATA_UPDATE; }
    uint32_t vipId() const { return m_vipId; }
    const QString& description() const { return m_description; }
    uint8_t icon() const { return m_icon; }
    bool notifyOnLogin() const { return m_notifyOnLogin; }
    const std::vector<uint8_t>& groupIds() const { return m_groupIds; }

private:
    uint32_t m_vipId;
    QString m_description;
    uint8_t m_icon;
    bool m_notifyOnLogin;
    std::vector<uint8_t> m_groupIds;
};

/// CIP input::TGameActionVipGroup: an action on a VIP group by name (type +0x8, name +0x10).
class TGameActionVipGroup : public gameactions::TGameActionSimple
{
public:
    const QString& groupName() const { return m_name; }

protected:
    TGameActionVipGroup(QString name, EGameAction type)
        : TGameActionSimple(type)
        , m_name(std::move(name))
    {
    }

private:
    QString m_name;
};

/// CIP input::TGameActionAddNewVipGroup (ADD_NEW_VIP_GROUP).
class TGameActionAddNewVipGroup final : public TGameActionVipGroup
{
public:
    explicit TGameActionAddNewVipGroup(QString name)
        : TGameActionVipGroup(std::move(name), EGameAction::ADD_NEW_VIP_GROUP)
    {
    }
};

/// CIP input::TGameActionRemoveVipGroup (REMOVE_VIP_GROUP).
class TGameActionRemoveVipGroup final : public TGameActionVipGroup
{
public:
    explicit TGameActionRemoveVipGroup(QString name)
        : TGameActionVipGroup(std::move(name), EGameAction::REMOVE_VIP_GROUP)
    {
    }
};

/// CIP input::TGameActionOpenEditVipGroupDialog (OPEN_EDIT_VIP_GROUP_DIALOG).
class TGameActionOpenEditVipGroupDialog final : public TGameActionVipGroup
{
public:
    explicit TGameActionOpenEditVipGroupDialog(QString name)
        : TGameActionVipGroup(std::move(name), EGameAction::OPEN_EDIT_VIP_GROUP_DIALOG)
    {
    }
};

/// CIP input::TGameActionRequestVipGroupUpdate (REQUEST_VIP_GROUP_UPDATE): the
/// edit-group dialog's result, a creatures::TVipStorage group.
class TGameActionRequestVipGroupUpdate final : public IGameAction
{
public:
    struct TVipGroup
    {
        uint8_t id = 0;
        QString name;
        bool editable = false;
    };
    explicit TGameActionRequestVipGroupUpdate(TVipGroup group)
        : m_group(std::move(group))
    {
    }
    EGameAction gameActionType() const override { return EGameAction::REQUEST_VIP_GROUP_UPDATE; }
    const TVipGroup& vipGroup() const { return m_group; }

private:
    TVipGroup m_group;
};

/// CIP input::TGameActionSortFilterVipList: the SORT_VIPS_BY_* and
/// SHOW/HIDE_OFFLINE_VIPS / SHOW/HIDE_VIPS_GROUPS actions.
class TGameActionSortFilterVipList final : public gameactions::TGameActionSimple
{
public:
    using TGameActionSimple::TGameActionSimple;
};

// ---------------------------------------------------------------------------
// Reports

/// CIP input::TGameActionReportCharacter: a rule violation report about a
/// player by name (REPORT_NAME, REPORT_BOT), and the base of the statement and
/// text reports.
class TGameActionReportCharacter : public IGameAction
{
public:
    TGameActionReportCharacter(EGameAction type, QString characterName)
        : m_type(type)
        , m_characterName(std::move(characterName))
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const QString& characterName() const { return m_characterName; }

private:
    EGameAction m_type;
    QString m_characterName;
};

/// CIP input::TGameActionReportStatement (REPORT_STATEMENT): a chat statement,
/// its speaker and the server's statement id.
class TGameActionReportStatement final : public TGameActionReportCharacter
{
public:
    TGameActionReportStatement(QString characterName, int statementId, QString statement)
        : TGameActionReportCharacter(EGameAction::REPORT_STATEMENT, std::move(characterName))
        , m_statementId(statementId)
        , m_statement(std::move(statement))
    {
    }
    int statementId() const { return m_statementId; }
    const QString& statement() const { return m_statement; }

private:
    int m_statementId;
    QString m_statement;
};

/// CIP input::TGameActionReportText (REPORT_TEXT): an edited text, its author
/// and the text's edit id.
class TGameActionReportText final : public TGameActionReportCharacter
{
public:
    TGameActionReportText(QString characterName, int textId, QString text)
        : TGameActionReportCharacter(EGameAction::REPORT_TEXT, std::move(characterName))
        , m_textId(textId)
        , m_text(std::move(text))
    {
    }
    int textId() const { return m_textId; }
    const QString& text() const { return m_text; }

private:
    int m_textId;
    QString m_text;
};

/// CIP input::TGameActionReportHireling (REPORT_HIRELING_NAME).
class TGameActionReportHireling final : public IGameAction
{
public:
    TGameActionReportHireling(EGameAction type, QString hirelingName, uint32_t creatureId)
        : m_type(type)
        , m_hirelingName(std::move(hirelingName))
        , m_creatureId(creatureId)
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const QString& hirelingName() const { return m_hirelingName; }
    uint32_t creatureId() const { return m_creatureId; }

private:
    EGameAction m_type;
    QString m_hirelingName;
    uint32_t m_creatureId;
};

/// CIP input::TGameActionReportMapBug (REPORT_BUG_MAP): the field reported.
class TGameActionReportMapBug final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionReportMapBug(const Position& coordinate)
        : TGameActionSimple(EGameAction::REPORT_BUG_MAP)
        , m_coordinate(coordinate)
    {
    }
    const Position& coordinate() const { return m_coordinate; }

private:
    Position m_coordinate;
};

/// CIP input::TGameActionReportTypo (REPORT_BUG_TYPO): the speaker and the text.
class TGameActionReportTypo final : public gameactions::TGameActionSimple
{
public:
    TGameActionReportTypo(QString speaker, QString text)
        : TGameActionSimple(EGameAction::REPORT_BUG_TYPO)
        , m_speaker(std::move(speaker))
        , m_text(std::move(text))
    {
    }
    const QString& speaker() const { return m_speaker; }
    const QString& text() const { return m_text; }

private:
    QString m_speaker;
    QString m_text;
};

/// CIP input::TGameActionGetTextForReport (GET_TEXT_FOR_REPORT): an object's
/// text to report.
class TGameActionGetTextForReport final : public IGameAction
{
public:
    explicit TGameActionGetTextForReport(TObjectGameActionContext object)
        : m_object(object)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::GET_TEXT_FOR_REPORT; }
    const TObjectGameActionContext& object() const { return m_object; }

private:
    TObjectGameActionContext m_object;
};

// ---------------------------------------------------------------------------
// Exiva options, loot lists

/// CIP input::TGameActionUpdateExivaOptions (EXIVA_UPADATE_OPTIONS).
class TGameActionUpdateExivaOptions final : public gameactions::TGameActionSimple
{
public:
    TGameActionUpdateExivaOptions()
        : TGameActionSimple(EGameAction::EXIVA_UPADATE_OPTIONS)
    {
    }
};

/// CIP input::TGameActionToggleBlackWhitelistStatus (LOOT_ADD_TO_BLACKWHITELIST /
/// LOOT_REMOVE_FROM_BLACKWHITELIST): the object type.
class TGameActionToggleBlackWhitelistStatus final : public gameactions::TGameActionSimple
{
public:
    TGameActionToggleBlackWhitelistStatus(EGameAction type, uint16_t typeId)
        : TGameActionSimple(type)
        , m_typeId(typeId)
    {
    }
    uint16_t typeId() const { return m_typeId; }

private:
    uint16_t m_typeId;
};



// ---------------------------------------------------------------------------
// Messages, URLs, files, dialogs

/// CIP input::TGameActionAddTextMessage: a text in the chat's current channel
/// (ADD_SYSTEM_CHAT_MESSAGE) or beneath the world map's centre
/// (ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE), in a colour (invalid: the default).
class TGameActionAddTextMessage final : public gameactions::TGameActionSimple
{
public:
    TGameActionAddTextMessage(EGameAction type, QString text, QColor color = {})
        : TGameActionSimple(type)
        , m_text(std::move(text))
        , m_color(color)
    {
    }
    const QString& text() const { return m_text; }
    const QColor& color() const { return m_color; }

private:
    QString m_text;
    QColor m_color;
};

/// CIP input::TGameActionSetStatusLine (SET_STATUS_LINE): the status line
/// beneath the world map (colour invalid: the default).
class TGameActionSetStatusLine final : public IGameAction
{
public:
    explicit TGameActionSetStatusLine(QString text, QColor color = {})
        : m_text(std::move(text))
        , m_color(color)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_STATUS_LINE; }
    const QString& text() const { return m_text; }
    const QColor& color() const { return m_color; }

private:
    QString m_text;
    QColor m_color;
};

/// CIP input::TGameActionOpenUrl (OPEN_URL).
class TGameActionOpenUrl final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionOpenUrl(QString url)
        : TGameActionSimple(EGameAction::OPEN_URL)
        , m_url(std::move(url))
    {
    }
    const QString& url() const { return m_url; }

private:
    QString m_url;
};

/// CIP input::TGameActionOpenGetPremiumUrl (OPEN_URL_GET_PREMIUM): the page's
/// source id, appended to the URL.
class TGameActionOpenGetPremiumUrl final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionOpenGetPremiumUrl(int source = 0)
        : TGameActionSimple(EGameAction::OPEN_URL_GET_PREMIUM)
        , m_source(source)
    {
    }
    int source() const { return m_source; }

private:
    int m_source;
};

/// CIP input::TGameActionOpenSurveyUrl (OPEN_URL_SURVEY): the survey and the token.
class TGameActionOpenSurveyUrl final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenSurveyUrl(int surveyId, QString token)
        : TGameActionSimple(EGameAction::OPEN_URL_SURVEY)
        , m_surveyId(surveyId)
        , m_token(std::move(token))
    {
    }
    int surveyId() const { return m_surveyId; }
    const QString& token() const { return m_token; }

private:
    int m_surveyId;
    QString m_token;
};

/// CIP input::TGameActionSaveTextToFile (SAVE_TEXT_TO_FILE).
class TGameActionSaveTextToFile final : public gameactions::TGameActionSimple
{
public:
    TGameActionSaveTextToFile(QString text, QString filePath)
        : TGameActionSimple(EGameAction::SAVE_TEXT_TO_FILE)
        , m_text(std::move(text))
        , m_filePath(std::move(filePath))
    {
    }
    const QString& text() const { return m_text; }
    const QString& filePath() const { return m_filePath; }

private:
    QString m_text;
    QString m_filePath;
};

/// CIP input::TGameActionSaveJsonDocumentToFile (SAVE_JSON_TO_FILE).
class TGameActionSaveJsonDocumentToFile final : public gameactions::TGameActionSimple
{
public:
    TGameActionSaveJsonDocumentToFile(QJsonDocument document, QString filePath)
        : TGameActionSimple(EGameAction::SAVE_JSON_TO_FILE)
        , m_document(std::move(document))
        , m_filePath(std::move(filePath))
    {
    }
    const QJsonDocument& document() const { return m_document; }
    const QString& filePath() const { return m_filePath; }

private:
    QJsonDocument m_document;
    QString m_filePath;
};

/// CIP input::TGameActionShowMessageDialog: a message dialog's text
/// (SHOW_EXALTATION_ERROR_DIALOG).
class TGameActionShowMessageDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionShowMessageDialog(EGameAction type, QString text)
        : TGameActionSimple(type)
        , m_text(std::move(text))
    {
    }
    const QString& text() const { return m_text; }

private:
    QString m_text;
};

/// CIP input::TGameActionShowNaggingDialog (SHOW_NAGGING_DIALOG): caption,
/// text and how long (seconds) the dialog cannot be closed.
class TGameActionShowNaggingDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionShowNaggingDialog(QString caption, QString text, qint64 durationSeconds)
        : TGameActionSimple(EGameAction::SHOW_NAGGING_DIALOG)
        , m_caption(std::move(caption))
        , m_text(std::move(text))
        , m_durationSeconds(durationSeconds)
    {
    }
    const QString& caption() const { return m_caption; }
    const QString& text() const { return m_text; }
    qint64 durationSeconds() const { return m_durationSeconds; }

private:
    QString m_caption;
    QString m_text;
    qint64 m_durationSeconds;
};

/// CIP input::TGameActionTakeAutomaticScreenshot (TAKE_AUTOMATIC_SCREENSHOT):
/// the screenshot::TScreenshotManager event.
class TGameActionTakeAutomaticScreenshot final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionTakeAutomaticScreenshot(int screenshotType)
        : TGameActionSimple(EGameAction::TAKE_AUTOMATIC_SCREENSHOT)
        , m_screenshotType(screenshotType)
    {
    }
    int screenshotType() const { return m_screenshotType; }

private:
    int m_screenshotType;
};

/// CIP input::TGameActionQuitGame (QUIT_GAME).
class TGameActionQuitGame final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionQuitGame(bool flag)
        : TGameActionSimple(EGameAction::QUIT_GAME)
        , m_flag(flag)
    {
    }
    /// CIP +0xc (the logout hotkey passes true).
    bool flag() const { return m_flag; }

private:
    bool m_flag;
};

struct TGameAction;
using TGameActionPtr = std::shared_ptr<const TGameAction>;
using TGameActionList = std::vector<IGameActionPtr>;

/// qtclient's field carrier for the actions whose CIP classes are not ported
/// yet: every type uses the subset of fields its CIP class carries. Ported
/// classes derive from IGameAction directly and carry only their own fields.
struct TGameAction : public IGameAction
{
    EGameAction gameActionType() const override { return type; }

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
    IGameActionPtr inner;
    /// ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE.
    QString text;
    QColor color;
    /// SAVE_TEXT_TO_FILE / SAVE_JSON_TO_FILE: where `text` is written (CIP:
    /// TGameActionSaveTextToFile / TGameActionSaveJsonToFile,).
    QString filePath;
    /// REPORT_TEXT (CIP: TGameActionReportStatement,): `text` is
    /// the author, `parameter` the text's edit id and this the reported text.
    QString statement;
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
    /// SET_PLAYER_MOVEMENT_INTENT: the path may end on an unwalkable goal
    /// (clicks on the world map or an object; not the minimap or cyclopedia map).
    bool enterUnwalkableGoal = true;
};

/// qtclient adapter while TGameAction exists: the action as the field carrier,
/// null for an action whose class is ported. Every reader still using it is
/// converted together with the classes it reads.
inline TGameActionPtr unportedGameAction(const IGameActionPtr& action)
{
    return std::dynamic_pointer_cast<const TGameAction>(action);
}



/// CIP input::TGameActionUseWith (ctor); also USE_ON_*.
class TGameActionUseWith final : public TGameAction
{
public:
    TGameActionUseWith() { type = EGameAction::USE_WITH; }
    TGameActionUseWith(Position position, int stackPosition, uint16_t objectClientId = 0);
    explicit TGameActionUseWith(const TGameAction& other);
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
    explicit TGameActionPlayerMovementIntent(Position position, int pathMode = 0, bool enterUnwalkableGoal = true);
    /// CIP `TGameActionPlayerMovementIntent::v0` → action type id `0x2b`.
    int v0() const { return static_cast<int>(type); }
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
    explicit TGameActionSmartClick(IGameActionPtr innerAction);
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

/// CIP input::TGameActionAssignSpellToActionButton .
class TGameActionAssignSpellToActionButton final : public TGameAction
{
public:
    TGameActionAssignSpellToActionButton() { type = EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON; }
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


/// CIP input::TGameActionCloseAllMultiActionButtons .
class TGameActionCloseAllMultiActionButtons final : public TGameAction
{
public:
    TGameActionCloseAllMultiActionButtons() { type = EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS; }
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


/// CIP input::TGameActionInspectObject .
class TGameActionInspectObject final : public TGameAction
{
public:
    TGameActionInspectObject() { type = EGameAction::INSPECT_OBJECT; }
    TGameActionInspectObject(Position position, int stackPosition, uint16_t objectClientId = 0);
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






/// CIP input::TGameActionOpenContainerInSpecificSidebar .
class TGameActionOpenContainerInSpecificSidebar final : public TGameAction
{
public:
    TGameActionOpenContainerInSpecificSidebar() { type = EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR; }
};



/// CIP input::TGameActionOpenLootContainer .
class TGameActionOpenLootContainer final : public TGameActionManagedContainer
{
public:
    TGameActionOpenLootContainer() { type = EGameAction::LOOT_CONTAINER_OPEN; }
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


/// CIP input::TGameActionPreySystem .
class TGameActionPreySystem final : public TGameAction
{
public:
    TGameActionPreySystem() = default;
    explicit TGameActionPreySystem(EGameAction type);
};

/// CIP input::TGameActionRefreshCompendiumNews .
class TGameActionRefreshCompendiumNews final : public TGameAction
{
public:
    TGameActionRefreshCompendiumNews() { type = EGameAction::REFRESH_COMPENDIUM_NEWS; }
};


/// CIP input::TGameActionRemoveSpriteIdentifierFromCache .
class TGameActionRemoveSpriteIdentifierFromCache final : public TGameAction
{
public:
    TGameActionRemoveSpriteIdentifierFromCache() { type = EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE; }
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
/// CIP input::TGameActionResolveSprite .
class TGameActionResolveSprite final : public TGameAction
{
public:
    TGameActionResolveSprite() { type = EGameAction::RESOLVE_SPRITE; }
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
/// CIP input::TGameActionSpriteResolved .
class TGameActionSpriteResolved final : public TGameAction
{
public:
    TGameActionSpriteResolved() { type = EGameAction::SPRITE_RESOLVED; }
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
/// CIP input::TGameActionSwitchClientOptions .
class TGameActionSwitchClientOptions final : public TGameAction
{
public:
    TGameActionSwitchClientOptions() { type = EGameAction::SWITCH_CLIENTOPTIONS_MODE; }
    /// The requested options mode (0: the user's, 1: the tutorial's).
    explicit TGameActionSwitchClientOptions(const int mode)
    {
        type = EGameAction::SWITCH_CLIENTOPTIONS_MODE;
        parameter = mode;
    }
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
/// CIP input::TGameActionToggleMounted .
class TGameActionToggleMounted final : public TGameAction
{
public:
    TGameActionToggleMounted() { type = EGameAction::TOGGLE_MOUNTED; }
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

/// The field carrier of `type` (qtclient, while TGameAction exists).
TGameActionPtr makeAction(EGameAction type);
/// An action of `type` without fields: CIP's class of the type when it carries
/// none (TGameActionSimple or its named subclass), else the field carrier.
IGameActionPtr makeGameAction(EGameAction type);
TGameActionPtr makeObjectAction(EGameAction type, const Position& position, int stackPosition, uint16_t objectClientId = 0);
/// An object action of `type`: the CIP class when ported, else the field carrier.
IGameActionPtr makeObjectGameAction(EGameAction type, const Position& position, int stackPosition, uint16_t objectClientId = 0);
/// The object an action is on (CIP: its location and object contexts); empty for none.
std::optional<TObjectGameActionContext> objectContextOf(const IGameAction& action);
/// A mutable field carrier copy of an unported action (a ported class's action gives
/// an empty carrier of its type).
std::shared_ptr<TGameAction> mutableClone(const IGameAction& action);
IGameActionPtr makeCreatureAction(EGameAction type, uint32_t creatureId);
/// The creature an action is on (CIP: the creature classes' v4); 0 for none.
uint32_t creatureIdOf(const IGameAction& action);
TGameActionPtr makeMovementIntent(const Position& position, int pathMode = 0, bool enterUnwalkableGoal = true);
TGameActionPtr makeSmartClick(const IGameActionPtr& inner);

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
TGameActionPtr finalizeDrop(const IGameActionPtr& move, const Position& destination, int modifiers, bool defaultIsMoveAll);

}
}
