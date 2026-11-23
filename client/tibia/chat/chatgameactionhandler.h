#pragma once

#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QColor>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace tibia {
namespace spells {
class TSpellInfoStorage;
}
namespace chat {

struct TChatEntry;
struct TChatChannelIdentifier;
class TChatChannelStorage;
class TTextStorage;
class TWhiteBlacklist;

/// CipSoft's EChatmessageMode of an ESpeechVolume: whisper (0) is 2, say (1)
/// is 1, yell (2) is 3; any other volume throws.
int speechVolumeToChatMessageMode(int speechVolume);

/// CIP chat::TChatGameActionHandler: the chat's game actions. Channel actions
/// go to the chat channel storage (open, close, clear, mute, server log),
/// server channels and private conversations are asked for with client
/// messages, talk actions become Talk messages (and the player's own line in
/// the NPCs channel or a private conversation), and the status line and the
/// centre messages go to the text storage. Each client message is handed to
/// the session through a send* signal.
///
/// Host concerns: the client options and the clock are qtclient singletons
/// (CipSoft passes them in); the white/blacklist belongs to the game client
/// and is set when the session gets it.
class TChatGameActionHandler final : public cip::tibia::chat::TChatGameActionHandler
{
    Q_OBJECT
public:
    TChatGameActionHandler(std::shared_ptr<TChatChannelStorage> chatChannelStorage, std::shared_ptr<TTextStorage> textStorage,
                           ::tibia::spells::TSpellInfoStorage* spellInfoStorage, QObject* parent = nullptr);
    ~TChatGameActionHandler() override;
    TChatGameActionHandler(const TChatGameActionHandler&) = delete;
    TChatGameActionHandler& operator=(const TChatGameActionHandler&) = delete;

    /// The list IGNORE_NAME / UNIGNORE_NAME change (none: they do nothing).
    void setWhiteBlacklist(TWhiteBlacklist* whiteBlacklist) { m_whiteBlacklist = whiteBlacklist; }

Q_SIGNALS:
    // publishGameAction is the generated base's.
    void sendGetChanneList(const ::tibia::protobuf::protocol::GameclientMessageGetChannels& GetChannelListMessage);
    void sendJoinChannel(const ::tibia::protobuf::protocol::GameclientMessageJoinChannel& JoinChannelMessage);
    void sendOpenChannel(const ::tibia::protobuf::protocol::GameclientMessageOpenChannel& OpenChannelMessage);
    void sendPrivateChannel(const ::tibia::protobuf::protocol::GameclientMessagePrivateChannel& PrivateChannelMessage);
    void sendCloseNPCChannel(const ::tibia::protobuf::protocol::GameclientMessageCloseNPCChannel& CloseNPCChannelMessage);
    void sendLeaveChannel(const ::tibia::protobuf::protocol::GameclientMessageLeaveChannel& LeaveChannelMessage);
    void sendInviteToChannel(const ::tibia::protobuf::protocol::GameclientMessageInviteToChannel& InviteToChannelMessage);
    void sendExcludeFromChannel(const ::tibia::protobuf::protocol::GameclientMessageExcludeFromChannel& ExcludeFromChannelMessage);
    void sendGuildMessage(const ::tibia::protobuf::protocol::GameclientMessageGuildMessage& GuildMessageMessage);
    void sendTalkMessage(const ::tibia::protobuf::protocol::GameclientMessageTalk& TalkMessage);

public Q_SLOTS:
    /// Hands an action of a type it handles to its handler; connected to the
    /// router's publishGameActionInternal.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void handleGetChanneListGameAction() override;
    /// Channels of the client (local chat, server log, loot, NPCs) are opened
    /// in the storage; server channels are joined, private conversations and
    /// the own private channel are asked for.
    void handleOpenChatChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction,
                                         bool ForceMakeCurrent);
    /// Leaves the channel on the server (the NPCs: ends the conversation; a
    /// private channel only while it is open) and removes it.
    void handleCloseChatChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleClearChatChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleSubscripeToServerLogGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleUnsubscripeServerLogGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleMuteChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleUnmuteChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction);
    void handleInvitePlayerToChannelGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionChatChannelModeration>& pGameAction);
    void handleExcludePlayerFromChannelGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionExcludePlayerFromChatChannel>& pGameAction);
    /// The status line, while the option gameWindowShowMessages is on.
    void handleSetStatusLineGameAction(const std::shared_ptr<const ::tibia::input::TGameActionSetStatusLine>& pGameAction);
    /// A system line in the channels that show the server log.
    void handleAddSystemChatMessageGameAction(const std::shared_ptr<const ::tibia::input::TGameActionAddTextMessage>& pGameAction);
    /// A centre message, while the option gameWindowShowMessages is on.
    void handleAddBeneathMiddleWorldmapMessageGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionAddTextMessage>& pGameAction);
    void handleClearOldestMessageFromMapwindowGameAction() override;
    void handleIgnoreNameGameAction(const std::shared_ptr<const ::tibia::input::TGameActionIgnoreName>& pGameAction);
    void handleUnignoreNameGameAction(const std::shared_ptr<const ::tibia::input::TGameActionUnignoreName>& pGameAction);
    void handleGetGuildMessage() override;
    /// A say, whisper or yell. A spell that aims (crosshair or at a target)
    /// is cast as the action's cast type asks: crosshair spells without a
    /// cast type and with the crosshair one become CROSSHAIR_SPELL, the mouse
    /// cast type SPELL_AT_MOUSE_CURSOR, the target cast type a Talk aimed at
    /// the target or the player.
    void handleSendTalkChatMessageWithSpeechVolumeGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithSpeechVolume>& pGameAction);
    void handleSendTalkChatMessageAsGamemeasterBroadcastGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessage>& pGameAction);
    void handleSendTalkChatMessageWithChannelIDGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithChannelID>& pGameAction);
    /// The own line goes to the conversation (a gamemaster's red message to
    /// the local chat), then the message is sent.
    void handleSendTalkChatMessageWidthRecipientGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithRecipient>& pGameAction);
    /// Sent, and the own line goes to the NPCs channel.
    void handleSendTalkChatMessageToNpcGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageToNpc>& pGameAction);
    /// A marker on a keyword of the NPCs channel.
    void handleAddGUITutorialMarkerGameAction(
        const std::shared_ptr<const ::tibia::input::TGameActionAddGUITutorialMarker>& pGameAction);
    void handleRemoveAllTutorialMarkersGameAction() override;
    /// Stores the option consoleShowJoinLeaveMessages.
    void handleToggleChannelJoinLeaveMessagesGameAction(bool ShowJoinLeaveMessages) override;
    /// Ends the conversation with the NPCs on the server.
    void emitSendCloseNPCChannel() override;

private:
    std::shared_ptr<TChatChannelStorage> m_chatChannelStorage;
    std::shared_ptr<TTextStorage> m_textStorage;
    ::tibia::spells::TSpellInfoStorage* m_spellInfoStorage = nullptr;
    TWhiteBlacklist* m_whiteBlacklist = nullptr;
};

/// An NPC keyword button: the KEYWORDBUTTONICON_* value and the keyword it says.
struct TKeywordButton
{
    int icon = 0;
    QString keyword;

    bool operator==(const TKeywordButton&) const = default;
};

/// The NPC conversation's partners and keyword buttons (CIP: chat::TChatStorage).
class TChatStorage final : public cip::tibia::chat::TChatStorage
{
    Q_OBJECT
public:
    explicit TChatStorage(QObject* parent = nullptr)
        : cip::tibia::chat::TChatStorage(parent)
    {
    }

    /// v12: npcTalkPartnersChanged when they differ.
    void setNpcTalkPartners(const std::vector<uint32_t>& npcIds);
    /// v13.
    std::vector<uint32_t> npcTalkPartners() const { return m_npcTalkPartners; }
    /// v14: keywordButtonsChanged when they differ.
    void setKeywordButtons(const std::vector<TKeywordButton>& buttons);
    /// v15.
    std::vector<TKeywordButton> keywordButtons() const { return m_keywordButtons; }

private:
    std::vector<uint32_t> m_npcTalkPartners;
    std::vector<TKeywordButton> m_keywordButtons;
};

/// The chat's game action candidates (CIP: chat::TChatGameActionProvider):
/// v2 for a channel tab (or the empty tab bar), v3 for an entry of a channel.
class TChatGameActionProvider final : public ::tibia::CipTyped
{
public:
    /// The name actions of a player entry (CIP +0x18, the generic chat
    /// provider): message, invite, VIP, ignore and report name.
    using NameActions = std::function<void(::tibia::input::TGameActionList& actions, const QString& name)>;
    /// The console option that shows join / leave messages (CIP +0x28, option 0x1b).
    using ShowJoinLeaveMessages = std::function<bool()>;

    TChatGameActionProvider(TChatChannelStorage& storage, NameActions nameActions, ShowJoinLeaveMessages showJoinLeaveMessages);

    /// v2: the tab's actions; without a channel, every open channel to pick as
    /// the secondary one, by name.
    ::tibia::input::TGameActionList gameActions(const TChatChannelIdentifier* identifier) const;
    /// v3: an entry's actions. `entryText` is the line as the pane shows it,
    /// `selectedText` the selection, `secondaryShown` whether the entry is in
    /// the secondary pane.
    ::tibia::input::TGameActionList entryGameActions(const QString& entryText, const TChatEntry& entry, const QString& selectedText,
                                                     const QString& ownName, bool secondaryShown) const;


private:
    TChatChannelStorage& m_storage;
    NameActions m_nameActions;
    ShowJoinLeaveMessages m_showJoinLeaveMessages;
};

/// Builds chat entries (CIP: chat::TClientChatEntryFactory). Every entry
/// carries the time it was made.
class TClientChatEntryFactory final
{
public:
    /// A system line.
    static TChatEntry systemEntry(const QColor& color, const QString& text);
    /// A speaker without level (an NPC).
    static TChatEntry speakerEntry(const QColor& color, const QString& name, const QString& text);
    /// A player with level.
    static TChatEntry speakerWithLevelEntry(const QColor& color, const QString& name, const QString& text, int level,
                                            bool traded, uint32_t statementId);
    /// A loot line.
    static TChatEntry lootEntry(const QColor& color, const QString& text);
    /// v2: "the channel was closed", red.
    static TChatEntry channelClosedEntry();
    /// v3: the Team Finder channel's closing line, red.
    static TChatEntry teamFinderChannelClosedEntry();
    /// v4: the help channel's introduction, white.
    static TChatEntry helpChannelIntroductionEntry();
    /// v5: the advertising channels' introduction, white.
    static TChatEntry advertisingChannelIntroductionEntry();
};

}
}
