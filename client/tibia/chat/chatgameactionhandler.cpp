#include "chat/chatgameactionhandler.h"

#include "chat/chatchannel.h"

#include <QDateTime>

namespace tibia::chat {

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
