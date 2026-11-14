#include "dailyreward/dailyrewardciptypes.h"
#include "protocol/protocolciptypes.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::dailyreward {

namespace {
struct CipTypedKeep {
    TDailyRewardFixedItems tDailyRewardFixedItems;
    TDailyRewardPickItems tDailyRewardPickItems;
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

} // namespace tibia::dailyreward

namespace tibia::dailyreward {

void TDailyRewardProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 226:
        if (shadow->HasExtension(GameserverMessageOpenRewardWall::gameserver_message_open_reward_wall)) {
            const auto& m = shadow->GetExtension(GameserverMessageOpenRewardWall::gameserver_message_open_reward_wall);
            tibia::game::notify("onOpenRewardWall", static_cast<uint8_t>(m.from_shrine() ? 1 : 0), m.next_reward_time(),
                                static_cast<uint8_t>(m.day_streak_day()), static_cast<uint8_t>(m.collected()), m.message(),
                                static_cast<uint16_t>(m.jokers()), m.time_left(), static_cast<uint16_t>(m.streak_level()));
        }
        break;
    case 222:
        if (shadow->HasExtension(GameserverMessageDailyRewardCollectionState::gameserver_message_daily_reward_collection_state))
            tibia::game::notify("onDailyRewardCollectionState", static_cast<uint8_t>(shadow->GetExtension(GameserverMessageDailyRewardCollectionState::gameserver_message_daily_reward_collection_state).collected() ? 1 : 0));
        break;
    case 229:
        if (shadow->HasExtension(GameserverMessageDailyRewardHistory::gameserver_message_daily_reward_history)) {
            std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>> history;
            for (const auto& e : shadow->GetExtension(GameserverMessageDailyRewardHistory::gameserver_message_daily_reward_history).entries())
                history.emplace_back(e.timestamp(), e.premium(), e.description(), static_cast<uint16_t>(e.streak()));
            tibia::game::notify("onRewardHistory", history);
        }
        break;
    default:
        break;
    }
}

}
