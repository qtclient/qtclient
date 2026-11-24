#include "chat/chatgameactionhandler.h"

#include "chat/chatchannel.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/textstorage.h"
#include "config/clientoptions.h"
#include "spells/spellinfostorage.h"

#include "iclockservice.h"

#include <QDateTime>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tibia::chat {

namespace {

using A = ::tibia::input::EGameAction;
using ::tibia::input::TGameActionList;

/// CIP TGameActionChatChannel: the action and the channel it names.
::tibia::input::IGameActionPtr channelAction(const A type, const TChatChannelIdentifier& identifier)
{
    return std::make_shared<::tibia::input::TGameActionChatChannel>(type, std::make_shared<const TChatChannelIdentifier>(identifier));
}

/// CIP TGameActionVisibleChatChannel: the action and whether the channel is
/// in the secondary pane.
::tibia::input::IGameActionPtr visibleChannelAction(const A type, const bool secondaryShown)
{
    return std::make_shared<::tibia::input::TGameActionVisibleChatChannel>(type, secondaryShown);
}

}

namespace {

namespace P = ::tibia::protobuf::protocol;
using CastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

/// CipSoft's EChatmessageMode values the talk handlers send.
enum EChatMessageMode : int
{
    ModePrivateTo = 5,
    ModeChannel = 7,
    ModeNpcTo = 12,
    ModeGamemasterBroadcast = 13,
    ModeChannelHighlight = 14,
    ModeGamemasterPrivateTo = 16,
};

/// A Talk message with its text.
P::GameclientMessageTalk talkMessage(const QString& text)
{
    P::GameclientMessageTalk message;
    message.set_text(text.toStdString());
    return message;
}

/// qtclient builds some text actions without a colour; they are white.
QColor colorOrWhite(const QColor& color)
{
    return color.isValid() ? color : systemWhite();
}

}

int speechVolumeToChatMessageMode(const int speechVolume)
{
    switch (speechVolume) {
    case 0:
        return 2;
    case 1:
        return 1;
    case 2:
        return 3;
    default:
        throw std::runtime_error(
            QStringLiteral("ESpeechVolume %1 should not be converted to EChatmessageMode!").arg(speechVolume).toStdString());
    }
}

TChatGameActionHandler::TChatGameActionHandler(std::shared_ptr<TChatChannelStorage> chatChannelStorage,
                                               std::shared_ptr<TTextStorage> textStorage,
                                               ::tibia::spells::TSpellInfoStorage* spellInfoStorage, QObject* parent)
    : cip::tibia::chat::TChatGameActionHandler(parent)
    , m_chatChannelStorage(std::move(chatChannelStorage))
    , m_textStorage(std::move(textStorage))
    , m_spellInfoStorage(spellInfoStorage)
{
}

TChatGameActionHandler::~TChatGameActionHandler() = default;

void TChatGameActionHandler::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    namespace I = ::tibia::input;
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::GET_CHAT_CHANNEL_LIST:
        handleGetChanneListGameAction();
        return;
    case A::OPEN_CHAT_CHANNEL:
    case A::OPEN_MESSAGE_TO_CHAT_CHANNEL:
        handleOpenChatChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction), false);
        return;
    case A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT:
        handleOpenChatChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction), true);
        return;
    case A::CLOSE_CHAT_CHANNEL:
        handleCloseChatChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::CLEAR_CHAT_CHANNEL:
        handleClearChatChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG:
        handleSubscripeToServerLogGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG:
        handleUnsubscripeServerLogGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::CHAT_CHANNEL_MUTE:
        handleMuteChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::CHAT_CHANNEL_UNMUTE:
        handleUnmuteChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannel>(pGameAction));
        return;
    case A::INVITE_PLAYER_TO_CHAT_CHANNEL:
    case A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL:
        handleInvitePlayerToChannelGameAction(std::dynamic_pointer_cast<const I::TGameActionChatChannelModeration>(pGameAction));
        return;
    case A::EXCLUDE_PLAYER_FROM_CHAT_CHANNEL:
        handleExcludePlayerFromChannelGameAction(
            std::dynamic_pointer_cast<const I::TGameActionExcludePlayerFromChatChannel>(pGameAction));
        return;
    case A::SET_STATUS_LINE:
        handleSetStatusLineGameAction(std::dynamic_pointer_cast<const I::TGameActionSetStatusLine>(pGameAction));
        return;
    case A::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW:
        handleClearOldestMessageFromMapwindowGameAction();
        return;
    case A::ADD_SYSTEM_CHAT_MESSAGE:
        handleAddSystemChatMessageGameAction(std::dynamic_pointer_cast<const I::TGameActionAddTextMessage>(pGameAction));
        return;
    case A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE:
        handleAddBeneathMiddleWorldmapMessageGameAction(std::dynamic_pointer_cast<const I::TGameActionAddTextMessage>(pGameAction));
        return;
    case A::IGNORE_NAME:
        handleIgnoreNameGameAction(std::dynamic_pointer_cast<const I::TGameActionIgnoreName>(pGameAction));
        return;
    case A::UNIGNORE_NAME:
        handleUnignoreNameGameAction(std::dynamic_pointer_cast<const I::TGameActionUnignoreName>(pGameAction));
        return;
    case A::GET_GUILD_MESSAGE:
        handleGetGuildMessage();
        return;
    case A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES:
        handleToggleChannelJoinLeaveMessagesGameAction(true);
        return;
    case A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES:
        handleToggleChannelJoinLeaveMessagesGameAction(false);
        return;
    case A::SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME:
        handleSendTalkChatMessageWithSpeechVolumeGameAction(
            std::dynamic_pointer_cast<const I::TGameActionSendChatMessageWithSpeechVolume>(pGameAction));
        return;
    case A::SEND_CHAT_MESSAGE_AS_GAMEMASTER_BROADCAST:
        handleSendTalkChatMessageAsGamemeasterBroadcastGameAction(
            std::dynamic_pointer_cast<const I::TGameActionSendChatMessage>(pGameAction));
        return;
    case A::SEND_CHAT_MESSAGE_WITH_CHANNEL_ID:
        handleSendTalkChatMessageWithChannelIDGameAction(
            std::dynamic_pointer_cast<const I::TGameActionSendChatMessageWithChannelID>(pGameAction));
        return;
    case A::SEND_CHAT_MESSAGE_WITH_RECIPIENT:
        handleSendTalkChatMessageWidthRecipientGameAction(
            std::dynamic_pointer_cast<const I::TGameActionSendChatMessageWithRecipient>(pGameAction));
        return;
    case A::SEND_CHAT_MESSAGE_TO_NPC:
        handleSendTalkChatMessageToNpcGameAction(std::dynamic_pointer_cast<const I::TGameActionSendChatMessageToNpc>(pGameAction));
        return;
    case A::ADD_GUI_TUTORIAL_MARKER:
        handleAddGUITutorialMarkerGameAction(std::dynamic_pointer_cast<const I::TGameActionAddGUITutorialMarker>(pGameAction));
        return;
    case A::REMOVE_ALL_TUTORIAL_MARKERS:
        handleRemoveAllTutorialMarkersGameAction();
        return;
    default:
        return;
    }
}

void TChatGameActionHandler::handleGetChanneListGameAction()
{
    Q_EMIT sendGetChanneList(P::GameclientMessageGetChannels());
}

void TChatGameActionHandler::handleOpenChatChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction, const bool ForceMakeCurrent)
{
    if (!pGameAction)
        return;
    const TChatChannelIdentifier& identifier = pGameAction->chatChannelIdentifier();
    switch (identifier.identifierType()) {
    case TChatChannelIdentifier::Local:
    case TChatChannelIdentifier::ServerLog:
    case TChatChannelIdentifier::Loot:
        m_chatChannelStorage->open(identifier, true, ForceMakeCurrent);
        return;
    case TChatChannelIdentifier::Npc:
        m_chatChannelStorage->open(identifier, false, ForceMakeCurrent);
        return;
    case TChatChannelIdentifier::ChannelId: {
        P::GameclientMessageJoinChannel message;
        message.set_channel_id(identifier.id);
        Q_EMIT sendJoinChannel(message);
        return;
    }
    case TChatChannelIdentifier::MessageTo: {
        P::GameclientMessagePrivateChannel message;
        message.set_receiver(identifier.name.toStdString());
        Q_EMIT sendPrivateChannel(message);
        return;
    }
    case TChatChannelIdentifier::OwnPrivateChannelToOpen:
        Q_EMIT sendOpenChannel(P::GameclientMessageOpenChannel());
        return;
    }
}

void TChatGameActionHandler::handleCloseChatChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    const TChatChannelIdentifier& identifier = pGameAction->chatChannelIdentifier();
    if (identifier.identifierType() == TChatChannelIdentifier::Npc) {
        Q_EMIT sendCloseNPCChannel(P::GameclientMessageCloseNPCChannel());
    } else if (identifier.identifierType() == TChatChannelIdentifier::ChannelId) {
        // A private channel is left only while it is open (the server may
        // have closed it already); any other server channel always.
        bool leave = true;
        if (identifier.isPrivateChannelId()) {
            const TChatChannel* channel = m_chatChannelStorage->find(identifier);
            leave = channel && !channel->isClosed();
        }
        if (leave) {
            P::GameclientMessageLeaveChannel message;
            message.set_channel_id(identifier.id);
            Q_EMIT sendLeaveChannel(message);
        }
    }
    m_chatChannelStorage->removeChannel(identifier);
}

void TChatGameActionHandler::handleClearChatChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    if (TChatChannel* channel = m_chatChannelStorage->find(pGameAction->chatChannelIdentifier()))
        channel->clear();
}

void TChatGameActionHandler::handleSubscripeToServerLogGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    if (TChatChannel* channel = m_chatChannelStorage->find(pGameAction->chatChannelIdentifier()))
        channel->setSubscribedToServerLog(true);
}

void TChatGameActionHandler::handleUnsubscripeServerLogGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    if (TChatChannel* channel = m_chatChannelStorage->find(pGameAction->chatChannelIdentifier()))
        channel->setSubscribedToServerLog(false);
}

void TChatGameActionHandler::handleMuteChannelGameAction(const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    if (TChatChannel* channel = m_chatChannelStorage->find(pGameAction->chatChannelIdentifier()))
        channel->setMuted(true);
}

void TChatGameActionHandler::handleUnmuteChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    if (TChatChannel* channel = m_chatChannelStorage->find(pGameAction->chatChannelIdentifier()))
        channel->setMuted(false);
}

void TChatGameActionHandler::handleInvitePlayerToChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionChatChannelModeration>& pGameAction)
{
    if (!pGameAction)
        return;
    P::GameclientMessageInviteToChannel message;
    message.set_channel_id(pGameAction->channelId());
    message.set_name(pGameAction->playerName().toStdString());
    Q_EMIT sendInviteToChannel(message);
}

void TChatGameActionHandler::handleExcludePlayerFromChannelGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionExcludePlayerFromChatChannel>& pGameAction)
{
    if (!pGameAction)
        return;
    P::GameclientMessageExcludeFromChannel message;
    message.set_channel_id(pGameAction->channelId());
    message.set_name(pGameAction->playerName().toStdString());
    Q_EMIT sendExcludeFromChannel(message);
}

void TChatGameActionHandler::handleSetStatusLineGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSetStatusLine>& pGameAction)
{
    if (!pGameAction || !config::TClientOptions::instance()->boolOption(config::EClientOption::gameWindowShowMessages))
        return;
    m_textStorage->setStatusMessage(pGameAction->text(), colorOrWhite(pGameAction->color()), false);
}

void TChatGameActionHandler::handleAddSystemChatMessageGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionAddTextMessage>& pGameAction)
{
    if (!pGameAction)
        return;
    m_chatChannelStorage->addServerLogEntry(colorOrWhite(pGameAction->color()), pGameAction->text());
}

void TChatGameActionHandler::handleAddBeneathMiddleWorldmapMessageGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionAddTextMessage>& pGameAction)
{
    if (!pGameAction || !config::TClientOptions::instance()->boolOption(config::EClientOption::gameWindowShowMessages))
        return;
    m_textStorage->addCenterMessage(pGameAction->text(), colorOrWhite(pGameAction->color()));
}

void TChatGameActionHandler::handleClearOldestMessageFromMapwindowGameAction()
{
    m_textStorage->clearOldestMessage();
}

void TChatGameActionHandler::handleIgnoreNameGameAction(const std::shared_ptr<const ::tibia::input::TGameActionIgnoreName>& pGameAction)
{
    if (!pGameAction || !m_whiteBlacklist)
        return;
    m_whiteBlacklist->addToBlacklist(pGameAction->name(), false);
    m_chatChannelStorage->addServerLogEntry(systemWhite(), qtTrId("ignoring_player_log_message").arg(pGameAction->name()));
}

void TChatGameActionHandler::handleUnignoreNameGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionUnignoreName>& pGameAction)
{
    if (!pGameAction || !m_whiteBlacklist)
        return;
    m_whiteBlacklist->removeFromBlacklist(pGameAction->name());
    m_chatChannelStorage->addServerLogEntry(systemWhite(), qtTrId("unignoring_player_log_message").arg(pGameAction->name()));
}

void TChatGameActionHandler::handleGetGuildMessage()
{
    Q_EMIT sendGuildMessage(P::GameclientMessageGuildMessage());
}

void TChatGameActionHandler::handleSendTalkChatMessageWithSpeechVolumeGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithSpeechVolume>& pGameAction)
{
    namespace I = ::tibia::input;
    if (!pGameAction)
        return;
    const int mode = speechVolumeToChatMessageMode(pGameAction->speechVolume());
    const uint32_t protocolMode = protocolMessageMode(mode);
    P::GameclientMessageTalk message = talkMessage(pGameAction->text());
    const int spellId = m_spellInfoStorage ? m_spellInfoStorage->spellIdOfText(pGameAction->text()) : 0;
    const auto send = [&](const std::optional<uint32_t> aimMode) {
        message.set_mode(protocolMode);
        if (aimMode)
            message.set_aim_mode(*aimMode);
        Q_EMIT sendTalkMessage(message);
    };
    if (spellId == 0) {
        send(std::nullopt);
        return;
    }
    const bool crossHairTarget = m_spellInfoStorage->isCrossHairTarget(spellId);
    const bool aimAtTarget = m_spellInfoStorage->isAimAtTarget(spellId);
    const int castType = pGameAction->spellCastOptionalTargetType();
    const auto spellId16 = static_cast<uint16_t>(spellId);
    if (castType == static_cast<int>(CastType::AtMousePosition) && (crossHairTarget || aimAtTarget)) {
        auto spell = std::make_shared<I::TGameActionSpellAtMouseCursor>(spellId16);
        spell->setWords(pGameAction->text());
        spell->setTalkMode(mode);
        Q_EMIT publishGameAction(spell);
        return;
    }
    // At the target or on yourself: the cast type is the aim mode.
    if ((castType == static_cast<int>(CastType::AtTargetOrSelf) || castType == static_cast<int>(CastType::OnYourself))
        && (crossHairTarget || aimAtTarget)) {
        send(static_cast<uint32_t>(castType));
        return;
    }
    if (crossHairTarget && (castType == static_cast<int>(CastType::None) || castType == static_cast<int>(CastType::Crosshair))) {
        auto spell = std::make_shared<I::TGameActionCrossHairSpell>(spellId16);
        spell->setWords(pGameAction->text());
        spell->setTalkMode(mode);
        Q_EMIT publishGameAction(spell);
        return;
    }
    if (!crossHairTarget && aimAtTarget
        && (castType == static_cast<int>(CastType::None) || castType == static_cast<int>(CastType::Crosshair))) {
        send(0);
        return;
    }
    send(std::nullopt);
}

void TChatGameActionHandler::handleSendTalkChatMessageAsGamemeasterBroadcastGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessage>& pGameAction)
{
    if (!pGameAction)
        return;
    P::GameclientMessageTalk message = talkMessage(pGameAction->text());
    message.set_mode(ModeGamemasterBroadcast);
    Q_EMIT sendTalkMessage(message);
}

void TChatGameActionHandler::handleSendTalkChatMessageWithChannelIDGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithChannelID>& pGameAction)
{
    if (!pGameAction)
        return;
    P::GameclientMessageTalk message = talkMessage(pGameAction->text());
    message.set_channel_id(pGameAction->channelId());
    message.set_mode(pGameAction->flag() ? ModeChannelHighlight : ModeChannel);
    Q_EMIT sendTalkMessage(message);
}

void TChatGameActionHandler::handleSendTalkChatMessageWidthRecipientGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageWithRecipient>& pGameAction)
{
    if (!pGameAction)
        return;
    int mode = ModePrivateTo;
    const QColor color = consoleColor(mode);
    if (pGameAction->recipientType() == 1) {
        m_chatChannelStorage->addLocalEntry(color, pGameAction->speakerName(), pGameAction->text(), pGameAction->speakerLevel(), false, 0);
        mode = ModeGamemasterPrivateTo;
    } else {
        m_chatChannelStorage->addPrivateEntry(color, pGameAction->recipient(), pGameAction->speakerName(), pGameAction->text(),
                                              pGameAction->speakerLevel(), false, 0);
    }
    P::GameclientMessageTalk message = talkMessage(pGameAction->text());
    message.set_receiver(pGameAction->recipient().toStdString());
    message.set_mode(protocolMessageMode(mode));
    Q_EMIT sendTalkMessage(message);
}

void TChatGameActionHandler::handleSendTalkChatMessageToNpcGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionSendChatMessageToNpc>& pGameAction)
{
    if (!pGameAction)
        return;
    P::GameclientMessageTalk message = talkMessage(pGameAction->text());
    message.set_mode(ModeNpcTo);
    m_chatChannelStorage->addOwnNpcChannelEntry(consoleColor(ModeNpcTo), pGameAction->speakerName(), pGameAction->text(),
                                                pGameAction->speakerLevel());
    Q_EMIT sendTalkMessage(message);
}

void TChatGameActionHandler::handleAddGUITutorialMarkerGameAction(
    const std::shared_ptr<const ::tibia::input::TGameActionAddGUITutorialMarker>& pGameAction)
{
    if (!pGameAction)
        return;
    const auto* keyword = dynamic_cast<const ::tibia::input::TLocationNPCChatKeywordGameActionContext*>(pGameAction->location().get());
    if (!keyword || pGameAction->object())
        return;
    m_chatChannelStorage->addTutorialMarkerToNpcChannel(pGameAction->markerId(), keyword->keyword());
}

void TChatGameActionHandler::handleRemoveAllTutorialMarkersGameAction()
{
    m_chatChannelStorage->removeAllTutorialMarkers();
}

void TChatGameActionHandler::handleToggleChannelJoinLeaveMessagesGameAction(const bool ShowJoinLeaveMessages)
{
    config::TClientOptions::instance()->setBoolOption(config::EClientOption::consoleShowJoinLeaveMessages, ShowJoinLeaveMessages);
}

void TChatGameActionHandler::emitSendCloseNPCChannel()
{
    Q_EMIT sendCloseNPCChannel(P::GameclientMessageCloseNPCChannel());
}

TChatGameActionProvider::TChatGameActionProvider(TChatChannelStorage& storage, NameActions nameActions,
                                                 ShowJoinLeaveMessages showJoinLeaveMessages)
    : m_storage(storage)
    , m_nameActions(std::move(nameActions))
    , m_showJoinLeaveMessages(std::move(showJoinLeaveMessages))
{
}

TGameActionList TChatGameActionProvider::gameActions(const TChatChannelIdentifier* identifier) const
{
    TGameActionList result{ ::tibia::input::makeGameAction(A::CONTEXT_MENU) };
    if (!identifier) {
        std::vector<TChatChannelIdentifier> identifiers = m_storage.identifiers();
        std::sort(identifiers.begin(), identifiers.end(), [](const TChatChannelIdentifier& a, const TChatChannelIdentifier& b) {
            return QString::compare(a.displayName(), b.displayName(), Qt::CaseInsensitive) < 0;
        });
        for (const TChatChannelIdentifier& each : identifiers)
            result.push_back(channelAction(A::SELECT_CHAT_CHANNEL_AS_SECONDARY, each));
        return result;
    }
    result.push_back(channelAction(A::CLEAR_CHAT_CHANNEL, *identifier));
    result.push_back(visibleChannelAction(A::SAVE_CHAT_CHANNEL_TO_FILE, false));
    if (identifier->canBeClosed())
        result.push_back(channelAction(A::CLOSE_CHAT_CHANNEL, *identifier));
    const TChatChannel* channel = m_storage.find(*identifier);
    if (!channel)
        return result;
    const bool secondaryShown = channel->isShownAsSecondaryChannel();
    if (channel->canSubscribeToServerLog()) {
        result.push_back(channelAction(channel->isSubscribedToServerLog() ? A::CHAT_CHANNEL_UNSUBSCRIBE_SERVER_LOG
                                                                          : A::CHAT_CHANNEL_SUBSCRIBE_TO_SERVER_LOG,
                                       *identifier));
    }
    if (channel->isOwnPrivateChannel()) {
        result.push_back(visibleChannelAction(A::OPEN_INVITE_TO_CHAT_CHANNEL_DIALOG, secondaryShown));
        result.push_back(visibleChannelAction(A::OPEN_EXCLUDE_FROM_CHAT_CHANNEL_DIALOG, secondaryShown));
    }
    if (identifier->type == TChatChannelIdentifier::ChannelId && identifier->isGuildChannel())
        result.push_back(std::make_shared<::tibia::input::TGameActionGetGuildMessage>());
    if (identifier->type == TChatChannelIdentifier::ChannelId && (identifier->isGuildChannel() || identifier->isTeamFinderChannel())) {
        const bool shown = m_showJoinLeaveMessages && m_showJoinLeaveMessages();
        result.push_back(::tibia::input::makeGameAction(shown ? A::HIDE_CHANNEL_JOIN_LEAVE_MESSAGES : A::SHOW_CHANNEL_JOIN_LEAVE_MESSAGES));
    }
    result.push_back(channelAction(secondaryShown ? A::HIDE_SECONDARY_CHAT_CHANNEL : A::SHOW_CHAT_CHANNEL_AS_SECONDARY, *identifier));
    if (identifier->type == TChatChannelIdentifier::ServerLog)
        return result;
    result.push_back(channelAction(channel->isMuted() ? A::CHAT_CHANNEL_UNMUTE : A::CHAT_CHANNEL_MUTE, *identifier));
    return result;
}

TGameActionList TChatGameActionProvider::entryGameActions(const QString& entryText, const TChatEntry& entry, const QString& selectedText,
                                                          const QString& ownName, const bool secondaryShown) const
{
    TGameActionList result{ ::tibia::input::makeGameAction(A::CONTEXT_MENU) };
    result.push_back(visibleChannelAction(A::CHAT_CHANNEL_SELECT_ALL, secondaryShown));
    result.push_back(std::make_shared<::tibia::input::TGameActionCopyChatEntryToClipboard>(entryText));
    // Speakers with and without level carry a name.
    if (entry.kind == TChatEntry::Speaker || entry.kind == TChatEntry::SpeakerWithLevel)
        result.push_back(std::make_shared<::tibia::input::TGameActionCopyCreatureNameToClipboard>(entry.name));
    if (!selectedText.isEmpty())
        result.push_back(std::make_shared<::tibia::input::TGameActionCopyTextToClipboard>(selectedText));
    if (entry.kind == TChatEntry::System || entry.kind == TChatEntry::Speaker) {
        result.push_back(std::make_shared<::tibia::input::TGameActionReportTypo>(entry.name, entry.text));
    }
    if (entry.kind != TChatEntry::SpeakerWithLevel)
        return result;
    if (entry.name != ownName) {
        if (m_nameActions)
            m_nameActions(result, entry.name);
        result.push_back(std::make_shared<::tibia::input::TGameActionReportStatement>(entry.name, static_cast<int>(entry.statementId),
                                                                                      entry.text));
    }
    if (entry.traded)
        result.push_back(std::make_shared<::tibia::input::TGameActionRecentlyTraded>(entry.name));
    return result;
}

TClientChatEntryFactory::TClientChatEntryFactory(std::shared_ptr<::tibia::shared::IClockService> clockService)
    : m_clockService(std::move(clockService))
{
}

TChatEntry TClientChatEntryFactory::systemEntry(const QDateTime& time, const QColor& color, const QString& text)
{
    return { TChatEntry::System, time, text, {}, 0, false, color, 0 };
}

TChatEntry TClientChatEntryFactory::systemEntry(const QColor& color, const QString& text)
{
    return systemEntry(QDateTime::currentDateTime(), color, text);
}

TChatEntry TClientChatEntryFactory::speakerEntry(const QColor& color, const QString& name, const QString& text)
{
    return { TChatEntry::Speaker, QDateTime::currentDateTime(), text, name, 0, false, color, 0 };
}

TChatEntry TClientChatEntryFactory::speakerWithLevelEntry(const QColor& color, const QString& name, const QString& text,
                                                          const int level, const bool traded, const uint32_t statementId)
{
    return { TChatEntry::SpeakerWithLevel, QDateTime::currentDateTime(), text, name, level, traded, color, statementId };
}

TChatEntry TClientChatEntryFactory::lootEntry(const QColor& color, const QString& text)
{
    return { TChatEntry::Loot, QDateTime::currentDateTime(), text, {}, 0, false, color, 0 };
}

TChatEntry TClientChatEntryFactory::channelClosedEntry() const
{
    return systemEntry(m_clockService->currentDateTime(), systemRed(), qtTrId("chat_channel_closed_message"));
}

TChatEntry TClientChatEntryFactory::teamFinderChannelClosedEntry() const
{
    return systemEntry(m_clockService->currentDateTime(), systemRed(), qtTrId("chat_team_finder_channel_closed_message"));
}

TChatEntry TClientChatEntryFactory::helpChannelIntroductionEntry() const
{
    return systemEntry(m_clockService->currentDateTime(), systemWhite(), qtTrId("chat_help_channel_introduction"));
}

TChatEntry TClientChatEntryFactory::advertisingChannelIntroductionEntry() const
{
    return systemEntry(m_clockService->currentDateTime(), systemWhite(), qtTrId("chat_advertising_channel_introduction"));
}

void TChatStorage::setNpcTalkPartners(const std::vector<uint32_t>& npcIds)
{
    if (m_npcTalkPartners == npcIds)
        return;
    m_npcTalkPartners = npcIds;
    Q_EMIT npcTalkPartnersChanged();
}

void TChatStorage::setKeywordButtons(const std::vector<TKeywordButton>& buttons)
{
    if (m_keywordButtons == buttons)
        return;
    m_keywordButtons = buttons;
    Q_EMIT keywordButtonsChanged();
}

} // namespace tibia::chat
