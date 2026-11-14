#include "news/newsciptypes.h"
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>
#include "gamewindow/newsdialogcontroller.h"

namespace tibia::news {

namespace {
struct CipTypedKeep {
    TNewsDiskIOAsync tNewsDiskIOAsync;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::news

namespace tibia::news {

void TNewsProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 152:
        if (shadow->HasExtension(GameserverMessageShowGameNews::gameserver_message_show_game_news)) {
            const auto& m = shadow->GetExtension(GameserverMessageShowGameNews::gameserver_message_show_game_news);
            if (auto* news = tibia::gamewindow::TNewsButtonController::instance())
                news->onShowGameNews(static_cast<int>(m.news_type()), m.notification_only());
            else
                tibia::game::notify("onGameNews", static_cast<int>(m.news_type()), m.notification_only());
        }
        break;
    default:
        break;
    }
}

}
