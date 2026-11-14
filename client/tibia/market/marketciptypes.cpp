#include "market/marketciptypes.h"
#include <unordered_map>
#include <ctime>
#include <string>
#include <client/const.h>

#include "protocol/protocolciptypes.h"
#include <cstdint>
#include <vector>

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::market {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TMarketProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    if (opcode == 248 && shadow->HasExtension(GameserverMessageMarketDetail::gameserver_message_market_detail)) {
        const auto& m = shadow->GetExtension(GameserverMessageMarketDetail::gameserver_message_market_detail);
        // Descriptions by attribute (1-based); an empty one is absent.
        std::unordered_map<int, std::string> descriptions;
        for (int i = 0; i < m.descriptions_size(); ++i) {
            if (!m.descriptions(i).empty())
                descriptions.try_emplace(i + 1, m.descriptions(i));
        }
        constexpr uint32_t DaySeconds = 86400;
        const uint32_t day = static_cast<uint32_t>((time(nullptr) / 1000) * DaySeconds) - DaySeconds;
        const auto stats = [day](const auto& list, const uint8_t action) {
            std::vector<std::vector<uint64_t>> out;
            for (const auto& s : list)
                out.push_back({ day, action, s.transactions(), s.total_price(), s.highest_price(), s.lowest_price() });
            return out;
        };
        tibia::game::notify("onMarketDetail", static_cast<uint16_t>(m.object().object_id()), descriptions,
                            stats(m.buy_statistics(), tibia::client::MARKETACTION_BUY), stats(m.sell_statistics(), tibia::client::MARKETACTION_SELL),
                            static_cast<uint8_t>(m.object().tier()));
        return;
    }
    if (opcode == 249 && shadow->HasExtension(GameserverMessageMarketBrowse::gameserver_message_market_browse)) {
        const auto& m = shadow->GetExtension(GameserverMessageMarketBrowse::gameserver_message_market_browse);
        // The request type, or the browsed object for an item browse.
        const auto var = static_cast<uint16_t>(m.request_type() == 3 ? m.object().object_id() : m.request_type());
        std::vector<std::vector<uint64_t>> offers;
        std::vector<std::string> names;
        const auto add = [&](const ProtocolMarketOffer& o, const uint8_t action) {
            const auto itemId = static_cast<uint16_t>(o.object().object_id());
            const auto tier = static_cast<uint8_t>(o.object().tier());
            const auto state = static_cast<uint8_t>(m.request_type() == 1 ? o.state() : tibia::client::OFFER_STATE_ACTIVE);
            tibia::game::notify("onMarketReadOffer", action, static_cast<uint16_t>(o.amount()), static_cast<uint16_t>(o.counter()), itemId,
                                o.character_name(), o.price(), state, o.timestamp(), var, tier);
            offers.push_back({ action, o.amount(), o.counter(), itemId, o.price(), state, o.timestamp(), var, tier });
            names.push_back(o.character_name());
        };
        for (const auto& o : m.buy_offers())
            add(o, tibia::client::MARKETACTION_BUY);
        for (const auto& o : m.sell_offers())
            add(o, tibia::client::MARKETACTION_SELL);
        tibia::game::notify("onMarketBrowse", offers, names);
        return;
    }
    if (opcode != 246 || !shadow->HasExtension(GameserverMessageMarketEnter::gameserver_message_market_enter))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageMarketEnter::gameserver_message_market_enter);
    std::vector<std::vector<uint16_t>> depotItems;
    for (const auto& e : m.depot_objects())
        depotItems.push_back({ static_cast<uint16_t>(e.object_id()), static_cast<uint16_t>(e.tier()), static_cast<uint16_t>(e.count()) });
    tibia::game::notify("onMarketEnter", depotItems, static_cast<uint8_t>(m.active_offers()), -1, -1);
}

} // namespace tibia::market
