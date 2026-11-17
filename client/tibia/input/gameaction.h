#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>

#include "gameactioncontext.h"

#include <QColor>
#include <QJsonDocument>
#include <QRectF>
#include <QString>
#include <QUrl>

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

/// CIP's location and object context pair of an object action (the two
/// shared pointers at +0x8 / +0x18 of TGameActionLook, TGameActionUse, ...).
class TObjectGameActionContext
{
public:
    TObjectGameActionContext() = default;
    TObjectGameActionContext(TGameActionContextPtr location, TGameActionContextPtr object)
        : m_location(std::move(location))
        , m_object(std::move(object))
    {
    }
    /// From qtclient's classic object position (see locationContextFromPosition):
    /// an appearance type object, or the creature when `creatureId` is set.
    TObjectGameActionContext(const Position& position, int stackPosition, uint16_t objectClientId,
                             uint32_t creatureId = 0);

    const TGameActionContextPtr& location() const { return m_location; }
    const TGameActionContextPtr& object() const { return m_object; }
    EGameActionContextType locationType() const
    {
        return m_location ? m_location->gameActionContextType() : EGameActionContextType::LOCATION_UNKNOWN;
    }

    /// The classic position and stack position of the location.
    Position position() const { return protocolPosition(m_location.get()); }
    int stackPosition() const { return protocolStackPosition(m_location.get()); }
    /// The object's appearance type (0 for a creature).
    uint16_t objectClientId() const { return appearanceTypeIdOf(m_object.get()); }
    /// The creature (0 for an appearance type object).
    uint32_t creatureId() const { return input::creatureIdOf(m_object.get()); }

private:
    TGameActionContextPtr m_location;
    TGameActionContextPtr m_object;
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
    const TGameActionContextPtr& location() const { return m_context.location(); }
    const TGameActionContextPtr& object() const { return m_context.object(); }
    Position position() const { return m_context.position(); }
    int stackPosition() const { return m_context.stackPosition(); }
    uint16_t objectClientId() const { return m_context.objectClientId(); }

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


// ---------------------------------------------------------------------------
// Sidebars, widgets, action bars, tactics, dialog openers

/// qtclient: the CIP classes that carry one value of type `T` at +0xc after
/// their type (+0x8).
template<EGameAction Type, typename T>
class TGameActionWithValue : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionWithValue(T value)
        : TGameActionSimple(Type)
        , m_value(value)
    {
    }

protected:
    T m_value;
};

/// CIP input::TGameActionToggleWidget (TOGGLE_WIDGET): the
/// gamewindow::TSidebarWidgetsManager widget type, and the sidebar (0xff: any).
class TGameActionToggleWidget final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionToggleWidget(int widgetType, uint8_t sidebarId = 0xff)
        : TGameActionSimple(EGameAction::TOGGLE_WIDGET)
        , m_widgetType(widgetType)
        , m_sidebarId(sidebarId)
    {
    }
    int widgetType() const { return m_widgetType; }
    uint8_t sidebarId() const { return m_sidebarId; }

private:
    int m_widgetType;
    uint8_t m_sidebarId;
};

/// CIP input::TGameActionOpenContainerInSpecificSidebar (OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR).
class TGameActionOpenContainerInSpecificSidebar final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenContainerInSpecificSidebar(uint8_t containerId, uint8_t sidebarId)
        : TGameActionSimple(EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR)
        , m_containerId(containerId)
        , m_sidebarId(sidebarId)
    {
    }
    uint8_t containerId() const { return m_containerId; }
    uint8_t sidebarId() const { return m_sidebarId; }

private:
    uint8_t m_containerId;
    uint8_t m_sidebarId;
};

/// CIP input::TGameActionOpenSecondaryBattleListInSpecificSidebar
/// (OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR): the list (0xff: a new one)
/// and the sidebar (0xff: the first with room).
class TGameActionOpenSecondaryBattleListInSpecificSidebar final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenSecondaryBattleListInSpecificSidebar(uint8_t battleListId, uint8_t sidebarId)
        : TGameActionSimple(EGameAction::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR)
        , m_battleListId(battleListId)
        , m_sidebarId(sidebarId)
    {
    }
    uint8_t battleListId() const { return m_battleListId; }
    uint8_t sidebarId() const { return m_sidebarId; }

private:
    uint8_t m_battleListId;
    uint8_t m_sidebarId;
};

/// CIP input::TGameActionMakeBattleListPrimary (MAKE_BATTLE_LIST_PRIMARY).
class TGameActionMakeBattleListPrimary final : public TGameActionWithValue<EGameAction::MAKE_BATTLE_LIST_PRIMARY, uint8_t>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    uint8_t battleListId() const { return m_value; }
};

/// CIP input::TGameActionToggleActionBar (TOGGLE_ACTION_BAR): the bar.
class TGameActionToggleActionBar final : public TGameActionWithValue<EGameAction::TOGGLE_ACTION_BAR, uint8_t>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    uint8_t actionBarId() const { return m_value; }
};

/// CIP input::TGameActionToggleActionBars (TOGGLE_ACTION_BARS): the bars' position.
class TGameActionToggleActionBars final : public TGameActionWithValue<EGameAction::TOGGLE_ACTION_BARS, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int position() const { return m_value; }
};

/// CIP input::TGameActionSetTactics (SET_TACTICS): attack mode, chase, secure
/// mode and PvP mode in one (+0xc, 8 bytes).
class TGameActionSetTactics final : public gameactions::TGameActionSimple
{
public:
    struct TTactics
    {
        bool chase = false;
        bool secure = false;
        int pvpMode = 0;
    };
    explicit TGameActionSetTactics(TTactics tactics)
        : TGameActionSimple(EGameAction::SET_TACTICS)
        , m_tactics(tactics)
    {
    }
    const TTactics& tactics() const { return m_tactics; }

private:
    TTactics m_tactics;
};

/// CIP input::TGameActionSetChaseMode (SET_CHASE_OPPONENTS).
class TGameActionSetChaseMode final : public IGameAction
{
public:
    explicit TGameActionSetChaseMode(bool chase)
        : m_chase(chase)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_CHASE_OPPONENTS; }
    bool chase() const { return m_chase; }

private:
    bool m_chase;
};

/// CIP input::TGameActionSetPvPMode (SET_PVP_MODE).
class TGameActionSetPvPMode final : public IGameAction
{
public:
    explicit TGameActionSetPvPMode(int pvpMode)
        : m_pvpMode(pvpMode)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_PVP_MODE; }
    int pvpMode() const { return m_pvpMode; }

private:
    int m_pvpMode;
};

/// CIP input::TGameActionSetSecureMode (SET_SECURE_MODE).
class TGameActionSetSecureMode final : public IGameAction
{
public:
    explicit TGameActionSetSecureMode(bool secure)
        : m_secure(secure)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_SECURE_MODE; }
    bool secure() const { return m_secure; }

private:
    bool m_secure;
};

/// CIP input::TGameActionSetSharedExperience (SET_SHARED_EXPERIENCE).
class TGameActionSetSharedExperience final : public TGameActionWithValue<EGameAction::SET_SHARED_EXPERIENCE, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    bool enable() const { return m_value; }
};

/// CIP input::TGameActionToggleMounted (TOGGLE_MOUNTED).
class TGameActionToggleMounted final : public IGameAction
{
public:
    EGameAction gameActionType() const override { return EGameAction::TOGGLE_MOUNTED; }
};

/// CIP input::TGameActionTryToSwitchHotkeySet (TRY_TO_SWITCH_HOTKEYSET): the
/// set's name, and whether the switch is announced in the status line.
class TGameActionTryToSwitchHotkeySet final : public gameactions::TGameActionSimple
{
public:
    TGameActionTryToSwitchHotkeySet(QString hotkeySetName, bool showStatusLine)
        : TGameActionSimple(EGameAction::TRY_TO_SWITCH_HOTKEYSET)
        , m_hotkeySetName(std::move(hotkeySetName))
        , m_showStatusLine(showStatusLine)
    {
    }
    const QString& hotkeySetName() const { return m_hotkeySetName; }
    bool showStatusLine() const { return m_showStatusLine; }

private:
    QString m_hotkeySetName;
    bool m_showStatusLine;
};

/// CIP input::TGameActionSwitchClientOptions (SWITCH_CLIENTOPTIONS_MODE).
class TGameActionSwitchClientOptions final : public TGameActionWithValue<EGameAction::SWITCH_CLIENTOPTIONS_MODE, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int mode() const { return m_value; }
};

/// CIP input::TGameActionOpenOptionsMenuDialog (OPEN_OPTIONS_MENU_DIALOG): the page.
class TGameActionOpenOptionsMenuDialog final : public TGameActionWithValue<EGameAction::OPEN_OPTIONS_MENU_DIALOG, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int page() const { return m_value; }
};

/// qtclient's subset of CIP's store::TStoreOpenParameters: the open action
/// (store::EStoreOpenAction), its detail (category / offer) and a search text.
struct TStoreOpenParameters
{
    int openAction = 0;
    int detail = 0;
    QString text;
};

/// CIP input::TGameActionOpenStore (OPEN_STORE): the store's open parameters.
class TGameActionOpenStore final : public gameactions::TGameActionSimple
{
public:
    using TStoreOpenParameters = input::TStoreOpenParameters;
    TGameActionOpenStore()
        : TGameActionSimple(EGameAction::OPEN_STORE)
    {
    }
    explicit TGameActionOpenStore(TStoreOpenParameters parameters)
        : TGameActionSimple(EGameAction::OPEN_STORE)
        , m_parameters(std::move(parameters))
    {
    }
    explicit TGameActionOpenStore(int openAction, int detail = 0, QString text = {})
        : TGameActionOpenStore(TStoreOpenParameters{ openAction, detail, std::move(text) })
    {
    }
    const TStoreOpenParameters& storeOpenParameters() const { return m_parameters; }

private:
    TStoreOpenParameters m_parameters;
};

/// CIP input::TGameActionOpenNewsDialog (OPEN_NEWS_DIALOG): optionally the
/// news to show (+0xc) or the category (+0x14).
class TGameActionOpenNewsDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionOpenNewsDialog()
        : TGameActionSimple(EGameAction::OPEN_NEWS_DIALOG)
    {
    }
    std::optional<uint32_t> newsId() const { return m_newsId; }
    std::optional<uint32_t> category() const { return m_category; }
    static std::shared_ptr<TGameActionOpenNewsDialog> withNews(uint32_t newsId);
    static std::shared_ptr<TGameActionOpenNewsDialog> withCategory(uint32_t category);

private:
    std::optional<uint32_t> m_newsId;
    std::optional<uint32_t> m_category;
};

/// CIP input::TGameActionOpenNewsDialogWithCategory (OPEN_NEWS_CATEGORY).
class TGameActionOpenNewsDialogWithCategory final : public TGameActionWithValue<EGameAction::OPEN_NEWS_CATEGORY, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int category() const { return m_value; }
};

/// CIP input::TGameActionOpenSocialDialog (OPEN_SOCIAL_DIALOG): the tab.
class TGameActionOpenSocialDialog final : public TGameActionWithValue<EGameAction::OPEN_SOCIAL_DIALOG, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int tab() const { return m_value; }
};

/// CIP input::TGameActionOpenQuestLog (OPEN_QUESTLOG_DIALOG): the quest (0: the list).
class TGameActionOpenQuestLog final : public TGameActionWithValue<EGameAction::OPEN_QUESTLOG_DIALOG, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int questId() const { return m_value; }
};

/// CIP input::TGameActionRefreshCompendiumNews (REFRESH_COMPENDIUM_NEWS).
class TGameActionRefreshCompendiumNews final : public TGameActionWithValue<EGameAction::REFRESH_COMPENDIUM_NEWS, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    /// CIP +0xc.
    bool flag() const { return m_value; }
};

/// CIP input::TGameActionRequestResourceBalance (REQUEST_RESOURCE_BALANCE): the resource type.
class TGameActionRequestResourceBalance final : public TGameActionWithValue<EGameAction::REQUEST_RESOURCE_BALANCE, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int resourceType() const { return m_value; }
};

/// CIP input::TGameActionCloseAllMultiActionButtons (CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS):
/// the bar and button whose multi action list stays as it is.
class TGameActionCloseAllMultiActionButtons final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionCloseAllMultiActionButtons(uint8_t actionBarId = 0, int actionButtonId = 0)
        : TGameActionSimple(EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS)
        , m_actionBarId(actionBarId)
        , m_actionButtonId(actionButtonId)
    {
    }
    uint8_t actionBarId() const { return m_actionBarId; }
    int actionButtonId() const { return m_actionButtonId; }

private:
    uint8_t m_actionBarId;
    int m_actionButtonId;
};


// ---------------------------------------------------------------------------
// Minimap markers, managed containers

/// CIP input::TGameActionForMinimapMarkerUsingCoordinate: a marker action by
/// field (SHOW_DIALOG_CREATE_MINIMAP_MARKER, DELETE_MINIMAP_MARKER).
class TGameActionForMinimapMarkerUsingCoordinate : public IGameAction
{
public:
    TGameActionForMinimapMarkerUsingCoordinate(EGameAction type, const Position& coordinate)
        : m_type(type)
        , m_coordinate(coordinate)
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const Position& coordinate() const { return m_coordinate; }

private:
    EGameAction m_type;
    Position m_coordinate;
};

/// CIP input::TGameActionShowDialogCreateMinimapMarker (SHOW_DIALOG_CREATE_MINIMAP_MARKER).
class TGameActionShowDialogCreateMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    explicit TGameActionShowDialogCreateMinimapMarker(const Position& coordinate)
        : TGameActionForMinimapMarkerUsingCoordinate(EGameAction::SHOW_DIALOG_CREATE_MINIMAP_MARKER, coordinate)
    {
    }
};

/// CIP input::TGameActionDeleteMinimapMarker (DELETE_MINIMAP_MARKER).
class TGameActionDeleteMinimapMarker final : public TGameActionForMinimapMarkerUsingCoordinate
{
public:
    explicit TGameActionDeleteMinimapMarker(const Position& coordinate)
        : TGameActionForMinimapMarkerUsingCoordinate(EGameAction::DELETE_MINIMAP_MARKER, coordinate)
    {
    }
};

/// CIP minimap::TMinimapMarker as the marker actions carry it: field, icon, description.
struct TMinimapMarkerData
{
    Position coordinate;
    uint8_t icon = 0;
    QString description;
};

/// CIP input::TGameActionForMinimapMarkerUsingFullMarkerData: a marker action
/// with the whole marker (SHOW_DIALOG_EDIT_MINIMAP_MARKER, UPDATE_MINIMAP_MARKER).
class TGameActionForMinimapMarkerUsingFullMarkerData : public IGameAction
{
public:
    TGameActionForMinimapMarkerUsingFullMarkerData(EGameAction type, TMinimapMarkerData marker)
        : m_type(type)
        , m_marker(std::move(marker))
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    const TMinimapMarkerData& marker() const { return m_marker; }

private:
    EGameAction m_type;
    TMinimapMarkerData m_marker;
};

/// CIP input::TGameActionShowDialogEditMinimapMarker (SHOW_DIALOG_EDIT_MINIMAP_MARKER).
class TGameActionShowDialogEditMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    explicit TGameActionShowDialogEditMinimapMarker(TMinimapMarkerData marker)
        : TGameActionForMinimapMarkerUsingFullMarkerData(EGameAction::SHOW_DIALOG_EDIT_MINIMAP_MARKER, std::move(marker))
    {
    }
};

/// CIP input::TGameActionUpdateMinimapMarker (UPDATE_MINIMAP_MARKER).
class TGameActionUpdateMinimapMarker final : public TGameActionForMinimapMarkerUsingFullMarkerData
{
public:
    explicit TGameActionUpdateMinimapMarker(TMinimapMarkerData marker)
        : TGameActionForMinimapMarkerUsingFullMarkerData(EGameAction::UPDATE_MINIMAP_MARKER, std::move(marker))
    {
    }
};

/// CIP input::TGameActionManagedContainer: a loot / obtain container action on
/// one category (v4); the base of the select / clear / open classes.
class TGameActionManagedContainer : public gameactions::TGameActionSimple
{
public:
    TGameActionManagedContainer(EGameAction type, int category)
        : TGameActionSimple(type)
        , m_category(category)
    {
    }
    /// CIP v4.
    int category() const { return m_category; }

private:
    int m_category;
};

/// CIP input::TGameActionSelectLootContainer / TGameActionSelectObtainContainer
/// (LOOT_CONTAINER_SELECT / OBTAIN_CONTAINER_SELECT): the container picked for the category.
class TGameActionSelectManagedContainer final : public TGameActionManagedContainer
{
public:
    TGameActionSelectManagedContainer(EGameAction type, int category, TObjectGameActionContext container)
        : TGameActionManagedContainer(type, category)
        , m_container(container)
    {
    }
    const TObjectGameActionContext& container() const { return m_container; }

private:
    TObjectGameActionContext m_container;
};

/// CIP input::TGameActionSetLootContainerFallback (LOOT_CONTAINER_FALLBACK).
class TGameActionSetLootContainerFallback final : public TGameActionWithValue<EGameAction::LOOT_CONTAINER_FALLBACK, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    bool useMainContainerAsFallback() const { return m_value; }
};


// ---------------------------------------------------------------------------
// Tutorial, session dumps

/// CIP input::TGameActionShowTutorialHint (SHOW_TUTORIAL_HINT).
class TGameActionShowTutorialHint final : public TGameActionWithValue<EGameAction::SHOW_TUTORIAL_HINT, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int tutorialHintId() const { return m_value; }
};

/// CIP input::TGameActionTutorialHintConfirmed (CONFIRM_TUTORIAL_HINT).
class TGameActionTutorialHintConfirmed final : public TGameActionWithValue<EGameAction::CONFIRM_TUTORIAL_HINT, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int tutorialHintId() const { return m_value; }
};

/// CIP input::TGameActionTutorialProgress (TUTORIAL_PROGRESS).
class TGameActionTutorialProgress final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionTutorialProgress(QString tutorialProgress)
        : TGameActionSimple(EGameAction::TUTORIAL_PROGRESS)
        , m_tutorialProgress(std::move(tutorialProgress))
    {
    }
    const QString& tutorialProgress() const { return m_tutorialProgress; }

private:
    QString m_tutorialProgress;
};

/// CIP input::TGameActionFinishedTutorialForCharacter (FINISHED_TUTORIAL_FOR_CHARACTER).
class TGameActionFinishedTutorialForCharacter final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionFinishedTutorialForCharacter(QString characterName)
        : TGameActionSimple(EGameAction::FINISHED_TUTORIAL_FOR_CHARACTER)
        , m_characterName(std::move(characterName))
    {
    }
    const QString& characterName() const { return m_characterName; }

private:
    QString m_characterName;
};

/// CIP input::TGameActionChangeTutorialModeEnabled (CHANGE_TUTORIAL_MODE_ENABLED).
class TGameActionChangeTutorialModeEnabled final : public TGameActionWithValue<EGameAction::CHANGE_TUTORIAL_MODE_ENABLED, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    bool enabled() const { return m_value; }
};

/// CIP input::TGameActionChangeContextMenuEnabled (CHANGE_CONTEXT_MENU_ENABLED).
class TGameActionChangeContextMenuEnabled final : public TGameActionWithValue<EGameAction::CHANGE_CONTEXT_MENU_ENABLED, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    bool enabled() const { return m_value; }
};

/// CIP input::TGameActionSessiondumpSkipTo (SESSIONDUMP_SKIP_TO): the timestamp (+0x10).
class TGameActionSessiondumpSkipTo final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionSessiondumpSkipTo(int64_t timestamp)
        : TGameActionSimple(EGameAction::SESSIONDUMP_SKIP_TO)
        , m_timestamp(timestamp)
    {
    }
    int64_t timestamp() const { return m_timestamp; }

private:
    int64_t m_timestamp;
};

/// CIP input::TGameActionSessiondumpStart (SESSIONDUMP_START): the session dump to play.
class TGameActionSessiondumpStart final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionSessiondumpStart(QUrl url)
        : TGameActionSimple(EGameAction::SESSIONDUMP_START)
        , m_url(std::move(url))
    {
    }
    const QUrl& url() const { return m_url; }

private:
    QUrl m_url;
};

/// CIP input::TGameActionAddGUITutorialMarker (ADD_GUI_TUTORIAL_MARKER). qtclient
/// carries the marker id, the field and the creature its world map overlay marks
/// (CIP: an item reference and a rectangle; not read yet).
class TGameActionAddGUITutorialMarker final : public gameactions::TGameActionSimple
{
public:
    TGameActionAddGUITutorialMarker(int markerId, const Position& position, uint32_t creatureId)
        : TGameActionSimple(EGameAction::ADD_GUI_TUTORIAL_MARKER)
        , m_markerId(markerId)
        , m_position(position)
        , m_creatureId(creatureId)
    {
    }
    int markerId() const { return m_markerId; }
    const Position& position() const { return m_position; }
    uint32_t creatureId() const { return m_creatureId; }

private:
    int m_markerId;
    Position m_position;
    uint32_t m_creatureId;
};

/// CIP input::TGameActionAddHintAtTutorialMarker (ADD_HINT_AT_TUTORIAL_MARKER). qtclient
/// carries the hint dialog's text, image and caption at a marker (CIP's hint struct is
/// not read yet).
class TGameActionAddHintAtTutorialMarker final : public gameactions::TGameActionSimple
{
public:
    TGameActionAddHintAtTutorialMarker(QString text, QString image, QString caption, int markerId)
        : TGameActionSimple(EGameAction::ADD_HINT_AT_TUTORIAL_MARKER)
        , m_text(std::move(text))
        , m_image(std::move(image))
        , m_caption(std::move(caption))
        , m_markerId(markerId)
    {
    }
    const QString& text() const { return m_text; }
    const QString& image() const { return m_image; }
    const QString& caption() const { return m_caption; }
    int markerId() const { return m_markerId; }

private:
    QString m_text;
    QString m_image;
    QString m_caption;
    int m_markerId;
};

/// CIP input::TGameActionAddMouseTrailHintAtTutorialMarker
/// (ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER): three values at +0xc, +0x10, +0x14
/// (CIP ctor order: +0x14, +0xc, +0x10; meaning not read yet).
class TGameActionAddMouseTrailHintAtTutorialMarker final : public gameactions::TGameActionSimple
{
public:
    TGameActionAddMouseTrailHintAtTutorialMarker(int value14, int valueC, int value10)
        : TGameActionSimple(EGameAction::ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER)
        , m_valueC(valueC)
        , m_value10(value10)
        , m_value14(value14)
    {
    }
    int markerId() const { return m_valueC; }

private:
    int m_valueC;
    int m_value10;
    int m_value14;
};


// ---------------------------------------------------------------------------
// Outfits, hirelings, podiums, exaltation forge, prey

/// CIP input::TGameActionSetPlayerOutfit (SET_PLAYER_OUTFIT): the chosen look,
/// whether the character rides, and a random mount.
class TGameActionSetPlayerOutfit final : public IGameAction
{
public:
    TGameActionSetPlayerOutfit(const Outfit& outfit, bool mountCharacter, bool randomMount)
        : m_outfit(outfit)
        , m_mountCharacter(mountCharacter)
        , m_randomMount(randomMount)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_PLAYER_OUTFIT; }
    const Outfit& outfit() const { return m_outfit; }
    bool mountCharacter() const { return m_mountCharacter; }
    bool randomMount() const { return m_randomMount; }

private:
    Outfit m_outfit;
    bool m_mountCharacter;
    bool m_randomMount;
};

/// CIP input::TGameActionSetHirelingOutfit (SET_HIRELING_OUTFIT).
class TGameActionSetHirelingOutfit final : public gameactions::TGameActionSimple
{
public:
    TGameActionSetHirelingOutfit(const Outfit& outfit, uint32_t creatureId)
        : TGameActionSimple(EGameAction::SET_HIRELING_OUTFIT)
        , m_outfit(outfit)
        , m_creatureId(creatureId)
    {
    }
    const Outfit& outfit() const { return m_outfit; }
    uint32_t creatureId() const { return m_creatureId; }

private:
    Outfit m_outfit;
    uint32_t m_creatureId;
};

/// CIP input::TGameActionSetHirelingName (SET_HIRELING_NAME): the name and two
/// values at +0x28 / +0x2c (meaning not read yet).
class TGameActionSetHirelingName final : public gameactions::TGameActionSimple
{
public:
    TGameActionSetHirelingName(QString name, int value28, int value2c)
        : TGameActionSimple(EGameAction::SET_HIRELING_NAME)
        , m_name(std::move(name))
        , m_value28(value28)
        , m_value2c(value2c)
    {
    }
    const QString& name() const { return m_name; }

private:
    QString m_name;
    int m_value28;
    int m_value2c;
};

/// The podium an outfit or creature shows on (CIP: the world map coordinate
/// with the podium's stack position and type).
struct TPodiumLocation
{
    Position position;
    uint16_t itemId = 0;
    uint8_t stackPosition = 0;
};

/// CIP input::TGameActionSetShowOffSocketConfiguration (SET_SHOW_OFF_SOCKET_CONFIGURATION):
/// the outfit shown on a podium, its look direction and whether the podium shows.
class TGameActionSetShowOffSocketConfiguration final : public IGameAction
{
public:
    TGameActionSetShowOffSocketConfiguration(TPodiumLocation podium, const Outfit& outfit, uint8_t lookDirection, bool showSocket)
        : m_podium(podium)
        , m_outfit(outfit)
        , m_lookDirection(lookDirection)
        , m_showSocket(showSocket)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_SHOW_OFF_SOCKET_CONFIGURATION; }
    const TPodiumLocation& podium() const { return m_podium; }
    const Outfit& outfit() const { return m_outfit; }
    uint8_t lookDirection() const { return m_lookDirection; }
    bool showSocket() const { return m_showSocket; }

private:
    TPodiumLocation m_podium;
    Outfit m_outfit;
    uint8_t m_lookDirection;
    bool m_showSocket;
};

/// CIP input::TGameActionSetCreaturePodiumConfiguration (SET_CREATURE_PODIUM_CONFIGURATION):
/// the race shown on a podium (0: none), its look direction, whether the podium
/// and the creature show.
class TGameActionSetCreaturePodiumConfiguration final : public IGameAction
{
public:
    TGameActionSetCreaturePodiumConfiguration(uint32_t raceId, TPodiumLocation podium, uint8_t lookDirection, bool showPodium,
                                              bool showCreature)
        : m_raceId(raceId)
        , m_podium(podium)
        , m_lookDirection(lookDirection)
        , m_showPodium(showPodium)
        , m_showCreature(showCreature)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_CREATURE_PODIUM_CONFIGURATION; }
    uint32_t raceId() const { return m_raceId; }
    const TPodiumLocation& podium() const { return m_podium; }
    uint8_t lookDirection() const { return m_lookDirection; }
    bool showPodium() const { return m_showPodium; }
    bool showCreature() const { return m_showCreature; }

private:
    uint32_t m_raceId;
    TPodiumLocation m_podium;
    uint8_t m_lookDirection;
    bool m_showPodium;
    bool m_showCreature;
};

/// CIP input::TGameActionRequestOutfitsFromServer (REQUEST_OUTFITS_FROM_SERVER): the
/// outfit window for the player (mode 0), a store try-on (mode, object type) or a
/// hireling (mode 2, creature).
class TGameActionRequestOutfitsFromServer final : public gameactions::TGameActionSimple
{
public:
    TGameActionRequestOutfitsFromServer()
        : TGameActionSimple(EGameAction::REQUEST_OUTFITS_FROM_SERVER)
    {
    }
    TGameActionRequestOutfitsFromServer(int mode, uint16_t typeId)
        : TGameActionSimple(EGameAction::REQUEST_OUTFITS_FROM_SERVER)
        , m_mode(mode)
        , m_typeId(typeId)
    {
    }
    static std::shared_ptr<TGameActionRequestOutfitsFromServer> forHireling(uint32_t creatureId);
    int mode() const { return m_mode; }
    uint16_t typeId() const { return m_typeId; }
    uint32_t creatureId() const { return m_creatureId; }

private:
    int m_mode = 0;
    uint16_t m_typeId = 0;
    uint32_t m_creatureId = 0;
};

/// CIP input::TGameActionBuyHirelingNameOrSexChange (BUY_HIRELING_NAME_OR_SEX_CHANGE):
/// the store's open parameters (qtclient opens the store's front page; CIP
/// passes open action 1, detail 7).
class TGameActionBuyHirelingNameOrSexChange final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionBuyHirelingNameOrSexChange(uint32_t creatureId)
        : TGameActionSimple(EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE)
        , m_creatureId(creatureId)
    {
    }
    const TStoreOpenParameters& storeOpenParameters() const { return m_parameters; }
    uint32_t creatureId() const { return m_creatureId; }

private:
    TStoreOpenParameters m_parameters;
    uint32_t m_creatureId;
};

/// CIP input::TGameActionExaltationForgeFusion (EXALTATION_FORGE_FUSION).
class TGameActionExaltationForgeFusion final : public gameactions::TGameActionSimple
{
public:
    TGameActionExaltationForgeFusion(uint16_t typeId, uint8_t tier, uint16_t resourceTypeId, bool successRateIncreased,
                                     bool tierLossReduced)
        : TGameActionSimple(EGameAction::EXALTATION_FORGE_FUSION)
        , m_typeId(typeId)
        , m_tier(tier)
        , m_resourceTypeId(resourceTypeId)
        , m_successRateIncreased(successRateIncreased)
        , m_tierLossReduced(tierLossReduced)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    uint8_t tier() const { return m_tier; }
    uint16_t resourceTypeId() const { return m_resourceTypeId; }
    bool successRateIncreased() const { return m_successRateIncreased; }
    bool tierLossReduced() const { return m_tierLossReduced; }

private:
    uint16_t m_typeId;
    uint8_t m_tier;
    uint16_t m_resourceTypeId;
    bool m_successRateIncreased;
    bool m_tierLossReduced;
};

/// CIP input::TGameActionExaltationForgeConvergenceFusion (EXALTATION_FORGE_CONVERGENCE_FUSION).
class TGameActionExaltationForgeConvergenceFusion final : public gameactions::TGameActionSimple
{
public:
    TGameActionExaltationForgeConvergenceFusion(uint16_t typeId, uint8_t tier, uint16_t resourceTypeId)
        : TGameActionSimple(EGameAction::EXALTATION_FORGE_CONVERGENCE_FUSION)
        , m_typeId(typeId)
        , m_tier(tier)
        , m_resourceTypeId(resourceTypeId)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    uint8_t tier() const { return m_tier; }
    uint16_t resourceTypeId() const { return m_resourceTypeId; }

private:
    uint16_t m_typeId;
    uint8_t m_tier;
    uint16_t m_resourceTypeId;
};

/// CIP input::TGameActionExaltationForgeTransfer (EXALTATION_FORGE_TRANSFER).
class TGameActionExaltationForgeTransfer final : public gameactions::TGameActionSimple
{
public:
    TGameActionExaltationForgeTransfer(uint16_t sourceTypeId, uint8_t tier, uint16_t targetTypeId, bool convergence)
        : TGameActionSimple(EGameAction::EXALTATION_FORGE_TRANSFER)
        , m_sourceTypeId(sourceTypeId)
        , m_tier(tier)
        , m_targetTypeId(targetTypeId)
        , m_convergence(convergence)
    {
    }
    uint16_t sourceTypeId() const { return m_sourceTypeId; }
    uint8_t tier() const { return m_tier; }
    uint16_t targetTypeId() const { return m_targetTypeId; }
    bool convergence() const { return m_convergence; }

private:
    uint16_t m_sourceTypeId;
    uint8_t m_tier;
    uint16_t m_targetTypeId;
    bool m_convergence;
};

/// CIP input::TGameActionShowExaltationResultDialog (SHOW_EXALTATION_RESULT_DIALOG).
/// qtclient: the result travels in the exaltation storage (CIP's action carries it).
class TGameActionShowExaltationResultDialog final : public gameactions::TGameActionSimple
{
public:
    TGameActionShowExaltationResultDialog()
        : TGameActionSimple(EGameAction::SHOW_EXALTATION_RESULT_DIALOG)
    {
    }
};

/// CIP input::TGameActionPreySystem: a prey action on one slot (+0xc); the list /
/// bonus rerolls and the bestiary list use it as is.
class TGameActionPreySystem : public gameactions::TGameActionSimple
{
public:
    TGameActionPreySystem(uint8_t preyId, EGameAction type)
        : TGameActionSimple(type)
        , m_preyId(preyId)
    {
    }
    uint8_t preyId() const { return m_preyId; }

private:
    uint8_t m_preyId;
};

/// CIP input::TGameActionSelectPreyMonsterFromList (PREY_SELECT_MONSTER_FROM_LIST).
class TGameActionSelectPreyMonsterFromList final : public TGameActionPreySystem
{
public:
    TGameActionSelectPreyMonsterFromList(uint8_t preyId, uint8_t listIndex)
        : TGameActionPreySystem(preyId, EGameAction::PREY_SELECT_MONSTER_FROM_LIST)
        , m_listIndex(listIndex)
    {
    }
    uint8_t listIndex() const { return m_listIndex; }

private:
    uint8_t m_listIndex;
};

/// CIP input::TGameActionSelectPreyMonsterFromBestiary (PREY_SELECT_MONSTER_FROM_BESTIARY).
class TGameActionSelectPreyMonsterFromBestiary final : public TGameActionPreySystem
{
public:
    TGameActionSelectPreyMonsterFromBestiary(uint8_t preyId, uint16_t raceId)
        : TGameActionPreySystem(preyId, EGameAction::PREY_SELECT_MONSTER_FROM_BESTIARY)
        , m_raceId(raceId)
    {
    }
    uint16_t raceId() const { return m_raceId; }

private:
    uint16_t m_raceId;
};

/// CIP input::TGameActionChangeAutoPreyExtendType (PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE).
class TGameActionChangeAutoPreyExtendType final : public TGameActionPreySystem
{
public:
    TGameActionChangeAutoPreyExtendType(uint8_t preyId, int extendType)
        : TGameActionPreySystem(preyId, EGameAction::PREY_REQUEST_CHANGE_AUTO_EXPAND_TYPE)
        , m_extendType(extendType)
    {
    }
    int extendType() const { return m_extendType; }

private:
    int m_extendType;
};


// ---------------------------------------------------------------------------
// Quests, sprites, depot search, store details, spells, teleport, dialogs

/// CIP input::TGameActionChangeQuesttrackerOptions (CHANGE_QUESTTRACKER_OPTIONS).
class TGameActionChangeQuesttrackerOptions final : public gameactions::TGameActionSimple
{
public:
    TGameActionChangeQuesttrackerOptions(int option, bool enabled)
        : TGameActionSimple(EGameAction::CHANGE_QUESTTRACKER_OPTIONS)
        , m_option(option)
        , m_enabled(enabled)
    {
    }
    int option() const { return m_option; }
    bool enabled() const { return m_enabled; }

private:
    int m_option;
    bool m_enabled;
};

/// CIP input::TGameActionChangeTrackedQuestFlags (CHANGE_TRACKED_QUESTFLAGS): quest
/// ids (+0x10) and whether they are tracked (+0x28).
class TGameActionChangeTrackedQuestFlags final : public gameactions::TGameActionSimple
{
public:
    TGameActionChangeTrackedQuestFlags(std::vector<int> questIds, bool track)
        : TGameActionSimple(EGameAction::CHANGE_TRACKED_QUESTFLAGS)
        , m_questIds(std::move(questIds))
        , m_track(track)
    {
    }
    const std::vector<int>& questIds() const { return m_questIds; }
    bool track() const { return m_track; }

private:
    std::vector<int> m_questIds;
    bool m_track;
};

/// CIP input::TGameActionRemoveSpriteIdentifierFromCache / TGameActionResolveSprite /
/// TGameActionSpriteResolved: an action on one sprite. qtclient carries the sprite
/// id (CIP: a shared TSpriteIdentifier).
class TGameActionSprite final : public gameactions::TGameActionSimple
{
public:
    TGameActionSprite(EGameAction type, uint32_t spriteId)
        : TGameActionSimple(type)
        , m_spriteId(spriteId)
    {
    }
    uint32_t spriteId() const { return m_spriteId; }

private:
    uint32_t m_spriteId;
};

/// CIP input::TGameActionCyclopediaMapSelectionItem (CYCLOPEDIA_MAP_SELECT_ITEM).
/// qtclient carries the subarea (CIP: two coordinates and an extent).
class TGameActionCyclopediaMapSelectionItem final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionCyclopediaMapSelectionItem(int subareaId)
        : TGameActionSimple(EGameAction::CYCLOPEDIA_MAP_SELECT_ITEM)
        , m_subareaId(subareaId)
    {
    }
    int subareaId() const { return m_subareaId; }

private:
    int m_subareaId;
};

/// CIP input::TGameActionRequestTransactionDetailsFromServer (REQUEST_TRANSACTION_DETAILS).
class TGameActionRequestTransactionDetailsFromServer final
    : public TGameActionWithValue<EGameAction::REQUEST_TRANSACTION_DETAILS, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    TGameActionRequestTransactionDetailsFromServer()
        : TGameActionWithValue(0)
    {
    }
    int transactionId() const { return m_value; }
};

/// CIP input::TGameActionRequestSearchTypeInDepot (REQUEST_SEARCH_TYPE_IN_DEPOT).
class TGameActionRequestSearchTypeInDepot final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionRequestSearchTypeInDepot(uint16_t typeId, uint16_t tier = 0)
        : TGameActionSimple(EGameAction::REQUEST_SEARCH_TYPE_IN_DEPOT)
        , m_typeId(typeId)
        , m_tier(tier)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    uint16_t tier() const { return m_tier; }

private:
    uint16_t m_typeId;
    uint16_t m_tier;
};

/// CIP input::TGameActionDepotSearchRetrieve (DEPOT_SERACH_RETRIEVE): the type,
/// tier and where it is taken from.
class TGameActionDepotSearchRetrieve final : public gameactions::TGameActionSimple
{
public:
    TGameActionDepotSearchRetrieve(uint16_t typeId, uint16_t tier, int location)
        : TGameActionSimple(EGameAction::DEPOT_SERACH_RETRIEVE)
        , m_typeId(typeId)
        , m_tier(tier)
        , m_location(location)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    uint16_t tier() const { return m_tier; }
    int location() const { return m_location; }

private:
    uint16_t m_typeId;
    uint16_t m_tier;
    int m_location;
};

/// CIP input::TGameActionAssignSpellToActionButton (ASSIGN_SPELL_TO_ACTION_BUTTON):
/// bar (+0xc), button (+0x10), multi action (+0x14), spell (+0x18). qtclient also
/// sends REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON with it (CIP: a TGameActionSimple).
class TGameActionAssignSpellToActionButton final : public gameactions::TGameActionSimple
{
public:
    TGameActionAssignSpellToActionButton(uint8_t actionBarId, int actionButtonId, int multiActionIndex, uint16_t spellId,
                                         EGameAction type = EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON)
        : TGameActionSimple(type)
        , m_actionBarId(actionBarId)
        , m_actionButtonId(actionButtonId)
        , m_multiActionIndex(multiActionIndex)
        , m_spellId(spellId)
    {
    }
    uint8_t actionBarId() const { return m_actionBarId; }
    int actionButtonId() const { return m_actionButtonId; }
    int multiActionIndex() const { return m_multiActionIndex; }
    uint16_t spellId() const { return m_spellId; }

private:
    uint8_t m_actionBarId;
    int m_actionButtonId;
    int m_multiActionIndex;
    uint16_t m_spellId;
};

/// CIP input::TGameActionTeleportToCoordinate (GOD_TELEPORT).
class TGameActionTeleportToCoordinate final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionTeleportToCoordinate(const Position& coordinate)
        : TGameActionSimple(EGameAction::GOD_TELEPORT)
        , m_coordinate(coordinate)
    {
    }
    const Position& coordinate() const { return m_coordinate; }

private:
    Position m_coordinate;
};

/// CIP input::TGameActionOpenShowImageDialog (OPEN_SHOW_IMAGE_DIALOG): the image.
class TGameActionOpenShowImageDialog final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionOpenShowImageDialog(QString imagePath)
        : TGameActionSimple(EGameAction::OPEN_SHOW_IMAGE_DIALOG)
        , m_imagePath(std::move(imagePath))
    {
    }
    const QString& imagePath() const { return m_imagePath; }

private:
    QString m_imagePath;
};

namespace gameactions {
/// CIP input::gameactions::TGameActionOpenTaskboardDialog (OPEN_TASKBOARD_DIALOG): the tab
/// (0 bounty tasks, 1 weekly tasks).
class TGameActionOpenTaskboardDialog final : public TGameActionWithValue<EGameAction::OPEN_TASKBOARD_DIALOG, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int tab() const { return m_value; }
};

/// CIP input::gameactions::TGameActionOpenWeaponProficiencyDialog (OPEN_WEAPON_PROFICIENCY_DIALOG).
class TGameActionOpenWeaponProficiencyDialog final
    : public TGameActionWithValue<EGameAction::OPEN_WEAPON_PROFICIENCY_DIALOG, uint16_t>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    uint16_t typeId() const { return m_value; }
};

/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog
/// (OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG): the weapon, perk level and perk.
class TGameActionOpenWeaponProficiencyShapeDialog final : public TGameActionSimple
{
public:
    TGameActionOpenWeaponProficiencyShapeDialog(uint16_t typeId, uint8_t level, uint8_t perk, bool fromPerkTree)
        : TGameActionSimple(EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG)
        , m_typeId(typeId)
        , m_level(level)
        , m_perk(perk)
        , m_fromPerkTree(fromPerkTree)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    uint8_t level() const { return m_level; }
    uint8_t perk() const { return m_perk; }
    bool fromPerkTree() const { return m_fromPerkTree; }

private:
    uint16_t m_typeId;
    uint8_t m_level;
    uint8_t m_perk;
    bool m_fromPerkTree;
};

/// CIP input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog
/// (OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG): the profession.
class TGameActionOpenWeaponProficiencyShapeOptionsDialog final
    : public TGameActionWithValue<EGameAction::OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int profession() const { return m_value; }
};

/// CIP input::gameactions::TSelectAtelierGemGameAction (GEM_ATELIER_SELECT_GEM).
class TSelectAtelierGemGameAction final : public TGameActionWithValue<EGameAction::GEM_ATELIER_SELECT_GEM, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int gemId() const { return m_value; }
};

/// CIP input::gameactions::TSelectFragmentWorkshopModGameAction (FRAGMENT_WORKSHOP_SELECT_MOD).
class TSelectFragmentWorkshopModGameAction final : public TGameActionSimple
{
public:
    TSelectFragmentWorkshopModGameAction(bool supremeMod, int modId)
        : TGameActionSimple(EGameAction::FRAGMENT_WORKSHOP_SELECT_MOD)
        , m_supremeMod(supremeMod)
        , m_modId(modId)
    {
    }
    bool supremeMod() const { return m_supremeMod; }
    int modId() const { return m_modId; }

private:
    bool m_supremeMod;
    int m_modId;
};

/// SHOW_BOSSTIARY_FREEPODIUM_DIALOG: the free podium message's text (CIP: a
/// string action, its text at +0x10).
class TGameActionShowBosstiaryFreePodiumDialog final
    : public TGameActionWithValue<EGameAction::SHOW_BOSSTIARY_FREEPODIUM_DIALOG, QString>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    const QString& text() const { return m_value; }
};

/// CIP input::gameactions::TGameActionShowCreateCharacterDialog (SHOW_CREATE_CHARACTER_DIALOG).
class TGameActionShowCreateCharacterDialog final
    : public TGameActionWithValue<EGameAction::SHOW_CREATE_CHARACTER_DIALOG, bool>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    /// The account has no characters yet.
    bool accountIsEmpty() const { return m_value; }
};

/// CIP input::gameactions::TGameActionGameEvent (GAME_EVENT).
class TGameActionGameEvent final : public TGameActionWithValue<EGameAction::GAME_EVENT, int>
{
public:
    using TGameActionWithValue::TGameActionWithValue;
    int gameEvent() const { return m_value; }
};
}


// ---------------------------------------------------------------------------
// Menus, smart click, action buttons, movement

/// CIP input::TGameActionContextMenu (CONTEXT_MENU).
class TGameActionContextMenu final : public IGameAction
{
public:
    EGameAction gameActionType() const override { return EGameAction::CONTEXT_MENU; }
};

/// CIP input::TGameActionContextMenuSeparator (CONTEXT_MENU_SEPARATOR).
class TGameActionContextMenuSeparator final : public IGameAction
{
public:
    EGameAction gameActionType() const override { return EGameAction::CONTEXT_MENU_SEPARATOR; }
};

/// CIP input::TGameActionLoadDefaultMinimapTilesFromResources (LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES).
class TGameActionLoadDefaultMinimapTilesFromResources final : public gameactions::TGameActionSimple
{
public:
    TGameActionLoadDefaultMinimapTilesFromResources()
        : TGameActionSimple(EGameAction::LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES)
    {
    }
};

/// CIP input::TGameActionSmartClick (SMART_CLICK): the action a click runs.
class TGameActionSmartClick final : public IGameAction
{
public:
    explicit TGameActionSmartClick(IGameActionPtr innerGameAction)
        : m_inner(std::move(innerGameAction))
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SMART_CLICK; }
    const IGameActionPtr& innerGameAction() const { return m_inner; }

private:
    IGameActionPtr m_inner;
};

/// CIP input::TGameActionActionButtonTriggered (ACTION_BUTTON_TRIGGERED): bar
/// (+0xc), button (+0x10), held key repeat (+0x14). qtclient also carries the
/// multi action slot clicked (-1: the button's current slot).
class TGameActionActionButtonTriggered final : public gameactions::TGameActionSimple
{
public:
    TGameActionActionButtonTriggered(uint8_t actionBarId, int actionButtonId, bool autoRepeat = false, int multiActionIndex = -1)
        : TGameActionSimple(EGameAction::ACTION_BUTTON_TRIGGERED)
        , m_actionBarId(actionBarId)
        , m_actionButtonId(actionButtonId)
        , m_autoRepeat(autoRepeat)
        , m_multiActionIndex(multiActionIndex)
    {
    }
    uint8_t actionBarId() const { return m_actionBarId; }
    int actionButtonId() const { return m_actionButtonId; }
    bool autoRepeat() const { return m_autoRepeat; }
    int multiActionIndex() const { return m_multiActionIndex; }

private:
    uint8_t m_actionBarId;
    int m_actionButtonId;
    bool m_autoRepeat;
    int m_multiActionIndex;
};

/// CIP input::TGameActionPlayerMovementIntent (SET_PLAYER_MOVEMENT_INTENT): walk to a
/// field (CIP v4: the TPlayerMovementIntent). qtclient adds the path mode (minimap /
/// cyclopedia / travel) and whether the path may end on an unwalkable goal (clicks on
/// the world map or an object; not the minimap or cyclopedia map).
class TGameActionPlayerMovementIntent final : public IGameAction
{
public:
    TGameActionPlayerMovementIntent(const Position& target, int autoWalkPathMode = 0, bool enterUnwalkableGoal = true)
        : m_target(target)
        , m_autoWalkPathMode(autoWalkPathMode)
        , m_enterUnwalkableGoal(enterUnwalkableGoal)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::SET_PLAYER_MOVEMENT_INTENT; }
    const Position& target() const { return m_target; }
    int autoWalkPathMode() const { return m_autoWalkPathMode; }
    bool enterUnwalkableGoal() const { return m_enterUnwalkableGoal; }

private:
    Position m_target;
    int m_autoWalkPathMode;
    bool m_enterUnwalkableGoal;
};


// ---------------------------------------------------------------------------
// Object actions: use with, targets, crosshair spells, moves, stash, equip

/// CIP input::TGameActionTargetSelected (TARGET_SELECTED): the object or creature
/// picked for a pending use or spell.
class TGameActionTargetSelected final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::TARGET_SELECTED; }
    uint32_t creatureId() const { return context().creatureId(); }
};
using TTargetSelectedPtr = std::shared_ptr<const TGameActionTargetSelected>;

/// CIP input::TGameActionUseWith (USE_WITH): an object used with a target
/// (none yet: the target is selected first).
class TGameActionUseWith final : public TObjectGameAction
{
public:
    explicit TGameActionUseWith(const TObjectGameActionContext& object, std::optional<TObjectGameActionContext> target = {})
        : TObjectGameAction(object)
        , m_target(std::move(target))
    {
    }
    EGameAction gameActionType() const override { return EGameAction::USE_WITH; }
    /// CIP: the target location and object are set.
    bool hasTarget() const { return m_target.has_value(); }
    const std::optional<TObjectGameActionContext>& target() const { return m_target; }

private:
    std::optional<TObjectGameActionContext> m_target;
};

/// CIP input::TGameActionUseOnCreature (USE_ON_CREATURE): an object used on a creature (+0x28).
class TGameActionUseOnCreature final : public TObjectGameAction
{
public:
    TGameActionUseOnCreature(const TObjectGameActionContext& object, uint32_t targetCreatureId)
        : TObjectGameAction(object)
        , m_targetCreatureId(targetCreatureId)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::USE_ON_CREATURE; }
    uint32_t targetCreatureId() const { return m_targetCreatureId; }

private:
    uint32_t m_targetCreatureId;
};

/// CIP input::TGameActionUseOnTarget (USE_ON_TARGET): on the attack target.
class TGameActionUseOnTarget final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::USE_ON_TARGET; }
};

/// CIP input::TGameActionUseOnYourself (USE_ON_YOURSELF).
class TGameActionUseOnYourself final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::USE_ON_YOURSELF; }
};

/// CIP input::TGameActionUseAtMouseCursor (USE_AT_MOUSECURSOR): an object used on
/// what is under the mouse (the target once a window found it).
class TGameActionUseAtMouseCursor final : public TObjectGameAction
{
public:
    explicit TGameActionUseAtMouseCursor(const TObjectGameActionContext& object, std::optional<TObjectGameActionContext> target = {})
        : TObjectGameAction(object)
        , m_target(std::move(target))
    {
    }
    EGameAction gameActionType() const override { return EGameAction::USE_AT_MOUSECURSOR; }
    const std::optional<TObjectGameActionContext>& target() const { return m_target; }

private:
    std::optional<TObjectGameActionContext> m_target;
};

/// CIP input::TGameActionPlayerTradeItem (TRADE_ITEM).
class TGameActionPlayerTradeItem final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::TRADE_ITEM; }
};

/// CIP input::TGameActionCrossHairSpell / TGameActionSpellAtMouseCursor: a spell (+0x38)
/// cast at a selected target or the mouse cursor. qtclient carries the words, the
/// talk mode (tibia::client::MessageMode) and the ESpellCastOptionalTargetType.
class TGameActionTargetedSpell : public IGameAction
{
public:
    TGameActionTargetedSpell(EGameAction type, uint16_t spellId, QString words, int talkMode, int spellCastType,
                             std::optional<TObjectGameActionContext> target = {})
        : m_type(type)
        , m_spellId(spellId)
        , m_words(std::move(words))
        , m_talkMode(talkMode)
        , m_spellCastType(spellCastType)
        , m_target(std::move(target))
    {
    }
    EGameAction gameActionType() const override { return m_type; }
    uint16_t spellId() const { return m_spellId; }
    const QString& words() const { return m_words; }
    int talkMode() const { return m_talkMode; }
    int spellCastType() const { return m_spellCastType; }
    const std::optional<TObjectGameActionContext>& target() const { return m_target; }

private:
    EGameAction m_type;
    uint16_t m_spellId;
    QString m_words;
    int m_talkMode;
    int m_spellCastType;
    std::optional<TObjectGameActionContext> m_target;
};

/// CIP input::TGameActionCrossHairSpell (CROSSHAIR_SPELL).
class TGameActionCrossHairSpell final : public TGameActionTargetedSpell
{
public:
    TGameActionCrossHairSpell(uint16_t spellId, QString words, int talkMode, int spellCastType,
                              std::optional<TObjectGameActionContext> target = {})
        : TGameActionTargetedSpell(EGameAction::CROSSHAIR_SPELL, spellId, std::move(words), talkMode, spellCastType, std::move(target))
    {
    }
};

/// CIP input::TGameActionSpellAtMouseCursor (SPELL_AT_MOUSE_CURSOR).
class TGameActionSpellAtMouseCursor final : public TGameActionTargetedSpell
{
public:
    TGameActionSpellAtMouseCursor(uint16_t spellId, QString words, int talkMode, int spellCastType)
        : TGameActionTargetedSpell(EGameAction::SPELL_AT_MOUSE_CURSOR, spellId, std::move(words), talkMode, spellCastType)
    {
    }
};

/// CIP input::TGameActionMoveObject (MOVE_OBJECT): an object, where it goes (+0x30),
/// whether everything moves (+0x28) and how many (qtclient: the split dialog's
/// amount, 0 for the whole stack).
class TGameActionMoveObject final : public TObjectGameAction
{
public:
    explicit TGameActionMoveObject(const TObjectGameActionContext& object, std::optional<Position> destination = {},
                                   bool moveAll = false, int amount = 0)
        : TObjectGameAction(object)
        , m_destination(std::move(destination))
        , m_moveAll(moveAll)
        , m_amount(amount)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::MOVE_OBJECT; }
    const std::optional<Position>& destination() const { return m_destination; }
    bool moveAll() const { return m_moveAll; }
    int amount() const { return m_amount; }

private:
    std::optional<Position> m_destination;
    bool m_moveAll;
    int m_amount;
};
using TMoveObjectPtr = std::shared_ptr<const TGameActionMoveObject>;

/// CIP input::TGameActionShowSplitStackDialog (SHOW_SPLIT_STACK_DIALOG): the move
/// whose amount the dialog asks for.
class TGameActionShowSplitStackDialog final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionShowSplitStackDialog(TMoveObjectPtr moveObject)
        : TGameActionSimple(EGameAction::SHOW_SPLIT_STACK_DIALOG)
        , m_moveObject(std::move(moveObject))
    {
    }
    const TMoveObjectPtr& moveObject() const { return m_moveObject; }

private:
    TMoveObjectPtr m_moveObject;
};

/// CIP input::TGameActionShowConfirmMoveObjectToStashDialog (SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG).
class TGameActionShowConfirmMoveObjectToStashDialog final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionShowConfirmMoveObjectToStashDialog(TMoveObjectPtr moveObject)
        : TGameActionSimple(EGameAction::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG)
        , m_moveObject(std::move(moveObject))
    {
    }
    const TMoveObjectPtr& moveObject() const { return m_moveObject; }

private:
    TMoveObjectPtr m_moveObject;
};

/// CIP input::TGameActionStashStowType (STASH_STOW_TYPE).
class TGameActionStashStowType final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::STASH_STOW_TYPE; }
};

/// CIP input::TGameActionStashStowContainerContent (STASH_STOW_CONTAINER_CONTENT).
class TGameActionStashStowContainerContent final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::STASH_STOW_CONTAINER_CONTENT; }
};

/// CIP input::TGameActionStashStowStack (STASH_STOW_STACK): a stack and how many
/// of it are stowed.
class TGameActionStashStowStack final : public TObjectGameAction
{
public:
    TGameActionStashStowStack(const TObjectGameActionContext& object, int amount)
        : TObjectGameAction(object)
        , m_amount(amount)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::STASH_STOW_STACK; }
    int amount() const { return m_amount; }

private:
    int m_amount;
};

/// CIP input::TGameActionStashRetrieve (STASH_RETRIEVE): type (+0xc), amount (+0x10)
/// and +0x14 (qtclient: 0).
class TGameActionStashRetrieve final : public gameactions::TGameActionSimple
{
public:
    TGameActionStashRetrieve(uint16_t typeId, int amount, int value14 = 0)
        : TGameActionSimple(EGameAction::STASH_RETRIEVE)
        , m_typeId(typeId)
        , m_amount(amount)
        , m_value14(value14)
    {
    }
    uint16_t typeId() const { return m_typeId; }
    int amount() const { return m_amount; }

private:
    uint16_t m_typeId;
    int m_amount;
    int m_value14;
};

/// CIP input::TGameActionStashOpenStowStackDialog (STASH_OPEN_STOW_STACK_DIALOG).
class TGameActionStashOpenStowStackDialog final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionStashOpenStowStackDialog(std::shared_ptr<const TGameActionStashStowStack> stowStack)
        : TGameActionSimple(EGameAction::STASH_OPEN_STOW_STACK_DIALOG)
        , m_stowStack(std::move(stowStack))
    {
    }
    const std::shared_ptr<const TGameActionStashStowStack>& stowStack() const { return m_stowStack; }

private:
    std::shared_ptr<const TGameActionStashStowStack> m_stowStack;
};

/// CIP input::TGameActionStashOpenRetrieveDialog (STASH_OPEN_RETRIEVE_DIALOG).
class TGameActionStashOpenRetrieveDialog final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionStashOpenRetrieveDialog(std::shared_ptr<const TGameActionStashRetrieve> retrieve)
        : TGameActionSimple(EGameAction::STASH_OPEN_RETRIEVE_DIALOG)
        , m_retrieve(std::move(retrieve))
    {
    }
    const std::shared_ptr<const TGameActionStashRetrieve>& retrieve() const { return m_retrieve; }

private:
    std::shared_ptr<const TGameActionStashRetrieve> m_retrieve;
};

/// CIP input::TGameActionTryToEquip (TRY_TO_EQUIP).
class TGameActionTryToEquip final : public TObjectGameAction
{
public:
    /// CIP takes the object (type, liquid or tier) and the hotkey's smart mode.
    explicit TGameActionTryToEquip(const TObjectGameActionContext& object, bool smartMode = false)
        : TObjectGameAction(object)
        , m_smartMode(smartMode)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::TRY_TO_EQUIP; }
    bool smartMode() const { return m_smartMode; }

private:
    bool m_smartMode;
};

/// CIP input::TGameActionInspectObject (INSPECT_OBJECT).
class TGameActionInspectObject final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::INSPECT_OBJECT; }
};

/// CIP input::TGameActionOpenParentContainer (OPEN_PARENT_CONTAINER).
class TGameActionOpenParentContainer final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::OPEN_PARENT_CONTAINER; }
};

/// CIP input::TGameActionQuickLoot (QUICK_LOOT).
class TGameActionQuickLoot final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::QUICK_LOOT; }
};

namespace gameactions {
/// CIP input::gameactions::TGameActionGatherAll (GATHER_ALL).
class TGameActionGatherAll final : public TObjectGameAction
{
public:
    using TObjectGameAction::TObjectGameAction;
    EGameAction gameActionType() const override { return EGameAction::GATHER_ALL; }
};

/// CIP input::gameactions::TGameActionMarketBrowse (MARKET_BROWSE): the object's type
/// (and tier) in the market.
class TGameActionMarketBrowse final : public TObjectGameAction
{
public:
    explicit TGameActionMarketBrowse(const TObjectGameActionContext& object, uint8_t tier = 0)
        : TObjectGameAction(object)
        , m_tier(tier)
    {
    }
    EGameAction gameActionType() const override { return EGameAction::MARKET_BROWSE; }
    uint8_t tier() const { return m_tier; }

private:
    uint8_t m_tier;
};
}

using TGameActionList = std::vector<IGameActionPtr>;























/// CIP input::TGameActionFriendsDialogSearchAccount (FRIENDS_DIALOG_SEARCH_ACCOUNT):
/// the character name to search.
class TGameActionFriendsDialogSearchAccount final : public gameactions::TGameActionSimple
{
public:
    explicit TGameActionFriendsDialogSearchAccount(QString characterName)
        : TGameActionSimple(EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT)
        , m_characterName(std::move(characterName))
    {
    }
    const QString& characterName() const { return m_characterName; }

private:
    QString m_characterName;
};













namespace gameactions {

}

/// An action of `type` without fields: CIP's class of the type when it carries
/// none (TGameActionSimple or its named subclass).
IGameActionPtr makeGameAction(EGameAction type);
/// The object action of `type` on the object (CIP: its location and object contexts).
IGameActionPtr makeObjectGameAction(EGameAction type, const Position& position, int stackPosition, uint16_t objectClientId = 0);
IGameActionPtr makeObjectGameAction(EGameAction type, const TObjectGameActionContext& context);
/// The creature an object context stands for: its creature context, or the
/// creature at its world map stack position (CIP passes creatures on the map
/// as their outfit's appearance type); 0 for any other object.
uint32_t creatureOfObject(const TObjectGameActionContext& object);
/// The object an action is on (CIP: its location and object contexts); empty for none.
std::optional<TObjectGameActionContext> objectContextOf(const IGameAction& action);
IGameActionPtr makeCreatureAction(EGameAction type, uint32_t creatureId);
/// The creature an action is on (CIP: the creature classes' v4); 0 for none.
uint32_t creatureIdOf(const IGameAction& action);
std::shared_ptr<const TGameActionPlayerMovementIntent> makeMovementIntent(const Position& position, int pathMode = 0, bool enterUnwalkableGoal = true);
std::shared_ptr<const TGameActionSmartClick> makeSmartClick(const IGameActionPtr& inner);


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
IGameActionPtr finalizeDrop(const IGameActionPtr& move, const Position& destination, int modifiers, bool defaultIsMoveAll);

}
}
