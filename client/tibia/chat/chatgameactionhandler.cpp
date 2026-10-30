#include "chat/chatgameactionhandler.h"

#include "chat/chatchannel.h"

#include <QDateTime>

namespace tibia::chat {

TChatEntry TClientChatEntryFactory::systemEntry(const QString& text, const QColor& color)
{
    return { TChatEntry::System, QDateTime::currentDateTime(), text, {}, 0, false, color, 0 };
}

} // namespace tibia::chat
