#include "news/newsciptypes.h"

#include "news/newsstorage.h"

#include <protocol.pb.h>

#include <utility>

namespace tibia::news {

TNewsProtocolMessageHandler::TNewsProtocolMessageHandler()
    : cip::tibia::news::TNewsProtocolMessageHandler(nullptr)
{
}

void TNewsProtocolMessageHandler::setNewsStorage(std::shared_ptr<TNewsStorage> NewsStorage)
{
    m_pNewsStorage = std::move(NewsStorage);
}

void TNewsProtocolMessageHandler::handleShowGameNewsMessage(const ::tibia::protobuf::protocol::GameserverMessageShowGameNews& ShowGameNewsMessage)
{
    const int newsType = static_cast<int>(ShowGameNewsMessage.news_type());
    const bool notificationOnly = ShowGameNewsMessage.notification_only();
    if (newsType != 0) {
        const TNewsStorage* storage = m_pNewsStorage.get();
        if (storage && storage->firstEntryOfType(newsType)) {
            Q_EMIT showGameNewsMessage(newsType, notificationOnly);
            return;
        }
    }
    Q_EMIT showGameNewsMessage(0, notificationOnly);
}

}
