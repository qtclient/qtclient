#include "chat/chatgameactionhandler.h"

#include "chat/chatchannel.h"

#include <QDateTime>

#include <algorithm>

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
    if (identifier->type == TChatChannelIdentifier::ChannelId && (identifier->isGuildChannel() || identifier->isPartyChannel())) {
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

TChatEntry TClientChatEntryFactory::systemEntry(const QColor& color, const QString& text)
{
    return { TChatEntry::System, QDateTime::currentDateTime(), text, {}, 0, false, color, 0 };
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

TChatEntry TClientChatEntryFactory::channelClosedEntry()
{
    return systemEntry(systemRed(), qtTrId("chat_channel_closed_message"));
}

TChatEntry TClientChatEntryFactory::teamFinderChannelClosedEntry()
{
    return systemEntry(systemRed(), qtTrId("chat_team_finder_channel_closed_message"));
}

TChatEntry TClientChatEntryFactory::helpChannelIntroductionEntry()
{
    return systemEntry(systemWhite(), qtTrId("chat_help_channel_introduction"));
}

TChatEntry TClientChatEntryFactory::advertisingChannelIntroductionEntry()
{
    return systemEntry(systemWhite(), qtTrId("chat_advertising_channel_introduction"));
}

} // namespace tibia::chat
