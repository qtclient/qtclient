#include "news/newsciptypes.h"

#include "gamewindow/newsdialogcontroller.h"
#include "news/newsstorage.h"

#include <protocol.pb.h>
#include <tibia/game/gamenotify.h>

namespace tibia::news {

namespace {

/// The handler's showGameNewsMessage: the news button (or the pack hook when
/// no game window exists) opens or highlights the news.
void showGameNewsMessage(const int newsType, const bool notificationOnly)
{
    if (auto* button = tibia::gamewindow::TNewsButtonController::instance())
        button->onShowGameNews(newsType, notificationOnly);
    else
        tibia::game::notify("onGameNews", newsType, notificationOnly);
}

}

void TNewsProtocolMessageHandler::handleShowGameNewsMessage(const ::tibia::protobuf::protocol::GameserverMessageShowGameNews& ShowGameNewsMessage)
{
    const int newsType = static_cast<int>(ShowGameNewsMessage.news_type());
    const bool notificationOnly = ShowGameNewsMessage.notification_only();
    if (newsType != 0) {
        const TNewsStorage* storage = TNewsStorage::current();
        if (storage && storage->firstEntryOfType(newsType)) {
            showGameNewsMessage(newsType, notificationOnly);
            return;
        }
    }
    showGameNewsMessage(0, notificationOnly);
}

}
