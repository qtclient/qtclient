#include "news/newsciptypes.h"

#include "gamewindow/newsdialogcontroller.h"
#include "news/newsstorage.h"

#include <protocol.pb.h>

#include <utility>

namespace tibia::news {

namespace {

/// The handler's showGameNewsMessage: the news button opens or highlights the
/// news (nothing without a game window).
void showGameNewsMessage(const int newsType, const bool notificationOnly)
{
    if (auto* button = tibia::gamewindow::TNewsButtonController::instance())
        button->onShowGameNewsMessage(newsType, notificationOnly);
}

}

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
            showGameNewsMessage(newsType, notificationOnly);
            return;
        }
    }
    showGameNewsMessage(0, notificationOnly);
}

}
