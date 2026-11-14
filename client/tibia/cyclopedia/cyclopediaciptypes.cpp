#include "cyclopedia/cyclopediaciptypes.h"
#include <cmath>
#include "protocol/protocolciptypes.h"
#include "client/staticdata.h"
#include "container/containerciptypes.h"
#include "creatures/creatureprotocolapply.h"
#include <tibia/assets/thingtype.h>
#include <tibia/client/outfit.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include <client/const.h>

#include <any>
#include <vector>
#include <tibia/game/gamenotify.h>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

struct CipTypedKeep {
    TCyclopediaMapGameActionProvider tCyclopediaMapGameActionProvider;
    TGeneralPlayerStatisticsProvider tGeneralPlayerStatisticsProvider;
    TPlayerOffenceStatsProvider tPlayerOffenceStatsProvider;
    TPlayerDefenceStatsProvider tPlayerDefenceStatsProvider;
    TPlayerMiscStatsProvider tPlayerMiscStatsProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

}

TGeneralPlayerStatisticsProvider::TGeneralPlayerStatisticsProvider()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseCyclopediaCharacterCombatStats", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterCombatStats>(&args[0]))
            m_combat = *data;
        if (const auto* mitigation = args.size() > 1 ? std::any_cast<double>(&args[1]) : nullptr)
            m_mitigation = *mitigation;
        else if (args.size() > 1)
            m_mitigation = static_cast<double>(client::gamecore::argInt(args, 1));
        return std::any{};
    });
}

TPlayerOffenceStatsProvider::TPlayerOffenceStatsProvider()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterOffenceStats", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterOffenceStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

TPlayerDefenceStatsProvider::TPlayerDefenceStatsProvider()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterDefenceStats", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterDefenceStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

TPlayerMiscStatsProvider::TPlayerMiscStatsProvider()
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaCharacterMiscStats", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterMiscStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::cyclopedia

namespace tibia::cyclopedia {

void TCyclopediaProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 175:
        if (shadow->HasExtension(GameserverMessageXpChanged::gameserver_message_xp_changed)) {
            const auto& m = shadow->GetExtension(GameserverMessageXpChanged::gameserver_message_xp_changed);
            tibia::game::notify("onUpdateExperience", m.raw_experience(), m.final_experience());
        }
        break;
    case 206:
        if (shadow->HasExtension(GameserverMessageItemWasted::gameserver_message_item_wasted))
            tibia::game::notify("onSupplyTracker", static_cast<uint16_t>(shadow->GetExtension(GameserverMessageItemWasted::gameserver_message_item_wasted).object_id()));
        break;
    case 217:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaNewDetails::gameserver_message_monster_cyclopedia_new_details))
            tibia::game::notify("onBestiaryEntryChanged", static_cast<uint16_t>(shadow->GetExtension(GameserverMessageMonsterCyclopediaNewDetails::gameserver_message_monster_cyclopedia_new_details).race_id()));
        break;
    case 238:
        if (shadow->HasExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance)) {
            const auto& m = shadow->GetExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance);
            const auto type = static_cast<tibia::client::ResourceTypes_t>(m.resource_type());
            tibia::game::localPlayerSetResourceBalance(
                type, m.has_small_value() ? static_cast<uint64_t>(static_cast<uint32_t>(m.small_value())) : m.value());
        }
        break;
    case 195:
        if (shadow->HasExtension(GameserverMessageCyclopediaHouseActionResult::gameserver_message_cyclopedia_house_action_result)) {
            const auto& m = shadow->GetExtension(GameserverMessageCyclopediaHouseActionResult::gameserver_message_cyclopedia_house_action_result);
            tibia::game::notify("onCyclopediaHouseAuctionMessage", m.house_id(), static_cast<uint8_t>(m.action_type()),
                                static_cast<uint8_t>(m.error_code()), m.outbidden());
        }
        break;
    case 199:
        if (shadow->HasExtension(GameserverMessageCyclopediaCurrentHouseData::gameserver_message_cyclopedia_current_house_data)) {
            std::vector<CyclopediaHouseEntry> houses;
            for (const auto& h : shadow->GetExtension(GameserverMessageCyclopediaCurrentHouseData::gameserver_message_cyclopedia_current_house_data).houses()) {
                CyclopediaHouseEntry& house = houses.emplace_back();
                house.houseId = h.house_id();
                house.isRentable = h.rentable();
                house.state = static_cast<uint8_t>(h.state());
                house.bidderName = h.bidder_name();
                house.isBidder = h.is_bidder();
                house.bidDisableReason = static_cast<uint8_t>(h.bid_disable_reason());
                house.bidEnd = h.bid_end();
                house.highestBid = h.highest_bid();
                house.bidLimit = h.bid_limit();
                house.ownerName = h.owner_name();
                house.paidUntil = h.paid_until();
                house.isOwner = h.is_owner();
                house.moveOutDisableReason = static_cast<uint8_t>(h.move_out_disable_reason());
                house.transferDisableReason = static_cast<uint8_t>(h.transfer_disable_reason());
                house.transferDate = h.transfer_date();
                house.newOwnerName = h.new_owner_name();
                house.newOwnerAccepted = h.new_owner_accepted();
                house.transferPrice = h.transfer_price();
                house.isNewOwner = h.is_new_owner();
                house.acceptTransferDisableReason = static_cast<uint8_t>(h.accept_transfer_disable_reason());
                house.rejectTransferDisableReason = static_cast<uint8_t>(h.reject_transfer_disable_reason());
                house.cancelTransferDisableReason = static_cast<uint8_t>(h.cancel_transfer_disable_reason());
                house.moveOutDate = h.move_out_date();
                house.cancelMoveOutDisableReason = static_cast<uint8_t>(h.cancel_move_out_disable_reason());
            }
            tibia::game::notify("onCyclopediaHouseList", houses);
        }
        break;
    case 43:
        if (shadow->HasExtension(GameserverMessagePartyHuntAnalyser::gameserver_message_party_hunt_analyser)) {
            const auto& m = shadow->GetExtension(GameserverMessagePartyHuntAnalyser::gameserver_message_party_hunt_analyser);
            std::vector<PartyMemberData> members;
            for (const auto& member : m.members())
                members.push_back({ member.member_id(), static_cast<uint8_t>(member.highlighted() ? 1 : 0), member.loot(),
                                    member.supplies(), member.damage(), member.healing() });
            std::vector<PartyMemberName> names;
            for (const auto& name : m.names())
                names.emplace_back(name.member_id(), name.name());
            tibia::game::notify("onPartyAnalyzerUpdate", m.session_start(), m.leader_id(), static_cast<uint8_t>(m.price_type()), members,
                                names);
        }
        break;
    case 198:
        if (shadow->HasExtension(GameserverMessageCyclopediaStaticHouseData::gameserver_message_cyclopedia_static_house_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageCyclopediaStaticHouseData::gameserver_message_cyclopedia_static_house_data);
            const std::vector<uint8_t> counts{ static_cast<uint8_t>(m.first_count()),  static_cast<uint8_t>(m.second_count()),
                                               static_cast<uint8_t>(m.third_count()),  static_cast<uint8_t>(m.fourth_count()),
                                               static_cast<uint8_t>(m.fifth_count()),  static_cast<uint8_t>(m.sixth_count()) };
            const std::vector<uint32_t> shown(m.shown_houses().begin(), m.shown_houses().end());
            const std::vector<uint32_t> others(m.other_houses().begin(), m.other_houses().end());
            tibia::game::notify("onCyclopediaHousesInfo", m.excluded_until(), counts, shown, others);
        }
        break;
    case 215:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaRace::gameserver_message_monster_cyclopedia_race)) {
            const auto& m = shadow->GetExtension(GameserverMessageMonsterCyclopediaRace::gameserver_message_monster_cyclopedia_race);
            BestiaryMonsterData data{};
            data.id = static_cast<uint16_t>(m.race_id());
            data.bestClass = m.class_name();
            data.currentLevel = static_cast<uint8_t>(m.progress());
            data.AnimusMasteryBonus = static_cast<uint16_t>(std::lround(m.animus_mastery_bonus() * 1000.0));
            data.AnimusMasteryPoints = static_cast<uint16_t>(m.animus_mastery_points());
            data.killCounter = m.kills();
            data.thirdDifficulty = static_cast<uint16_t>(m.first_unlock());
            data.secondUnlock = static_cast<uint16_t>(m.second_unlock());
            data.lastProgressKillCount = static_cast<uint16_t>(m.last_unlock());
            data.difficulty = m.difficulty() ? 1 : 0;
            data.ocorrence = static_cast<uint8_t>(m.occurrence());
            data.leaderKilled = m.leader_killed() != 0;
            for (const auto& l : m.loot())
                data.loot.push_back({ .itemId = static_cast<uint16_t>(l.object_id()), .diffculty = static_cast<uint8_t>(l.rarity()),
                                      .specialEvent = static_cast<uint8_t>(l.special_event() ? 1 : 0), .name = l.name(),
                                      .amount = static_cast<uint8_t>(l.amount()) });
            data.charmValue = static_cast<uint16_t>(m.charm_value());
            data.attackMode = static_cast<uint8_t>(m.attack_mode());
            data.castsSpells = m.casts_spells();
            data.maxHealth = m.max_health();
            data.experience = m.experience();
            data.speed = static_cast<uint16_t>(m.speed());
            data.armor = static_cast<uint16_t>(m.armor());
            data.mitigation = m.mitigation();
            for (const auto& e : m.elements())
                data.combat[static_cast<uint8_t>(e.element())] = static_cast<uint16_t>(e.value());
            if (m.locations_size() > 0)
                data.location = m.locations(0);
            tibia::game::processUpdateBestiaryMonsterData(data);
        }
        break;
    case 221:
        if (shadow->HasExtension(GameserverMessageCyclopediaMapData::gameserver_message_cyclopedia_map_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageCyclopediaMapData::gameserver_message_cyclopedia_map_data);
            const Position pos(static_cast<int>(m.position().x()), static_cast<int>(m.position().y()), static_cast<short>(m.position().z()));
            switch (m.data_type()) {
            case 0:
                tibia::game::processAddAutomapFlag(pos, static_cast<uint8_t>(m.icon()), m.text());
                break;
            case 1: {
                std::vector<std::vector<int>> mains;
                for (const auto& a : m.main_areas())
                    mains.push_back({ static_cast<int>(a.area_id()), static_cast<int>(a.status()), static_cast<int>(a.progress()) });
                const std::vector<int> discovered(m.discovered_areas().begin(), m.discovered_areas().end());
                const std::vector<int> discoverable(m.discoverable_areas().begin(), m.discoverable_areas().end());
                tibia::game::notify("onCyclopediaMapDiscoveryData", mains, discovered, discoverable);
                break;
            }
            case 2:
                tibia::game::notify("onCyclopediaMapRaid", static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z));
                break;
            case 5: {
                std::vector<std::vector<int>> points;
                for (const auto& p : m.points())
                    points.push_back({ static_cast<int>(p.position().x()), static_cast<int>(p.position().y()), static_cast<int>(p.position().z()),
                                       static_cast<int>(p.state()) });
                tibia::game::notify("onCyclopediaMapDiscoveryArea", static_cast<int>(m.area_id()), static_cast<int>(m.flag() ? 1 : 0),
                                    static_cast<int>(m.target()), points);
                break;
            }
            case 6: {
                std::vector<int> passage{ static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z), m.race_ids_size() };
                for (const uint32_t race : m.race_ids())
                    passage.push_back(static_cast<int>(race));
                tibia::game::notify("onCyclopediaMapPassage", passage);
                break;
            }
            case 9: {
                std::vector<std::vector<double>> areas;
                for (const auto& d : m.donations())
                    areas.push_back({ static_cast<double>(d.area_id()), d.improved_respawn() ? 1.0 : 0.0, static_cast<double>(d.donated()) });
                tibia::game::notify("onCyclopediaMapDonations", m.donation_goal(), areas);
                break;
            }
            case 10:
                tibia::game::notify("onCyclopediaMapCurrentArea", static_cast<int>(m.area_id()));
                break;
            case 11:
                tibia::game::notify("onCyclopediaMapExploringArea", static_cast<int>(m.area_id()));
                break;
            default: // imminent raids, sub area monsters, monster bestiary: no consumer yet
                break;
            }
        }
        break;
    case 214:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaMonsters::gameserver_message_monster_cyclopedia_monsters)) {
            const auto& m = shadow->GetExtension(GameserverMessageMonsterCyclopediaMonsters::gameserver_message_monster_cyclopedia_monsters);
            std::vector<BestiaryOverviewMonsters> monsters;
            for (const auto& e : m.monsters())
                monsters.push_back({ .id = static_cast<uint16_t>(e.race_id()), .currentLevel = static_cast<uint8_t>(e.progress()),
                                     .occurrence = static_cast<uint8_t>(e.occurrence()),
                                     .creatureAnimusMasteryBonus = static_cast<uint16_t>(std::lround(e.animus_mastery_bonus() * 1000.0)) });
            tibia::game::processParseBestiaryOverview(m.class_name(), monsters, static_cast<uint16_t>(m.animus_mastery_points()));
        }
        break;
    case 213:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopedia::gameserver_message_monster_cyclopedia)) {
            std::vector<CyclopediaBestiaryRace> classes;
            uint8_t index = 0;
            for (const auto& c : shadow->GetExtension(GameserverMessageMonsterCyclopedia::gameserver_message_monster_cyclopedia).classes()) {
                CyclopediaBestiaryRace& entry = classes.emplace_back();
                entry.race = index++;
                entry.bestClass = c.name();
                entry.count = static_cast<uint16_t>(c.total());
                entry.unlockedCount = static_cast<uint16_t>(c.known());
            }
            tibia::game::processParseBestiaryRaces(classes);
        }
        break;
    case 204:
        if (shadow->HasExtension(GameserverMessageImpactTracking::gameserver_message_impact_tracking)) {
            const auto& m = shadow->GetExtension(GameserverMessageImpactTracking::gameserver_message_impact_tracking);
            const auto type = static_cast<uint8_t>(m.type());
            // Damage dealt splits into its physical part and its elemental part.
            const auto element = static_cast<uint8_t>(type == 1 ? m.elemental_type() : m.element());
            tibia::game::notify("onImpactTracker", type, m.amount(), element, m.source(), m.elemental_amount());
        }
        break;
    case 205:
        if (shadow->HasExtension(GameserverMessageMarketStatistics::gameserver_message_market_statistics)) {
            // The average prices of untiered objects.
            std::map<uint16_t, uint64_t> prices;
            for (const auto& e : shadow->GetExtension(GameserverMessageMarketStatistics::gameserver_message_market_statistics).entries()) {
                if (e.tier() == 0)
                    prices[static_cast<uint16_t>(e.object_id())] = e.price();
            }
            tibia::game::notify("onItemsPrice", prices);
        }
        break;
    case 207:
        if (shadow->HasExtension(GameserverMessageItemLooted::gameserver_message_item_looted)) {
            const auto& m = shadow->GetExtension(GameserverMessageItemLooted::gameserver_message_item_looted);
            const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(m.item());
            const uint16_t count = slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
            tibia::game::notify("onLootStats", slot.typeId, count, m.name());
        }
        break;
    case 209:
        if (shadow->HasExtension(GameserverMessageKillTracking::gameserver_message_kill_tracking)) {
            const auto& m = shadow->GetExtension(GameserverMessageKillTracking::gameserver_message_kill_tracking);
            std::vector<::tibia::container::TInventorySlot> items;
            items.reserve(static_cast<size_t>(m.items_size()));
            for (const auto& item : m.items())
                items.push_back(::tibia::container::TInventorySlot::fromProtocolInventoryItem(item));
            tibia::game::notify("onKillTracker", m.name(), creatures::outfitFromProtocol(m.outfit(), nullptr), items);
        }
        break;
    default:
        break;
    }
}

void TBestiaryTrackerProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/,
                                                                     const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow || opcode != 185 || !shadow->HasExtension(GameserverMessageBestiaryTracker::gameserver_message_bestiary_tracker))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageBestiaryTracker::gameserver_message_bestiary_tracker);
    std::vector<std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>> entries;
    for (const auto& e : m.entries())
        entries.emplace_back(static_cast<uint16_t>(e.race_id()), e.kills(), static_cast<uint16_t>(e.first_unlock()),
                             static_cast<uint16_t>(e.second_unlock()), static_cast<uint16_t>(e.last_unlock()),
                             static_cast<uint8_t>(e.completed() ? 1 : 0));
    tibia::game::notify("onParseCyclopediaTracker", static_cast<uint8_t>(m.tracker_type()), entries);
}

}
