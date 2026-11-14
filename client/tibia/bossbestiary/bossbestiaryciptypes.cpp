#include "bossbestiary/bossbestiaryciptypes.h"
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"
#include "client/staticdata.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

namespace tibia::bossbestiary {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    // Session-owned CIP bosstiary storage (live notify → moc signals).
    // TBossTrackerStorage is stack-owned in main beside sidebar trackers.
    new TBosstiaryStorage(parent);
    new TBossBestiaryProtocolMessageHandler(parent);
}

} // namespace tibia::bossbestiary

namespace tibia::bossbestiary {

void TBossBestiaryProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 97:
        if (shadow->HasExtension(GameserverMessageBosstiaryStaticData::gameserver_message_bosstiary_static_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageBosstiaryStaticData::gameserver_message_bosstiary_static_data);
            std::vector<uint16_t> kills, points;
            for (const uint32_t v : m.kills_for_stage())
                kills.push_back(static_cast<uint16_t>(v));
            for (const uint32_t v : m.points_for_stage())
                points.push_back(static_cast<uint16_t>(v));
            tibia::game::notify("onBosstiaryData", kills, points);
        }
        break;
    case 98:
        if (shadow->HasExtension(GameserverMessageBossSlots::gameserver_message_boss_slots)) {
            const auto& m = shadow->GetExtension(GameserverMessageBossSlots::gameserver_message_boss_slots);
            // A locked slot carries one value where an unlocked one names its boss.
            const auto slotOf = [](const ProtocolBossSlot& slot, bool& unlocked, uint32_t& bossId, std::optional<BosstiarySlot>& data) {
                unlocked = slot.unlocked();
                bossId = unlocked ? slot.boss().boss_id() : slot.locked_value();
                if (unlocked && bossId != 0)
                    data = BosstiarySlot{ .bossRace = static_cast<uint8_t>(slot.boss().race()), .killCount = slot.kill_count(),
                                          .lootBonus = static_cast<uint16_t>(slot.loot_bonus()),
                                          .killBonus = static_cast<uint8_t>(slot.kill_bonus()),
                                          .bossRaceRepeat = static_cast<uint8_t>(slot.race_repeat()),
                                          .removePrice = slot.remove_price(), .inactive = static_cast<uint8_t>(slot.inactive() ? 1 : 0) };
            };
            BosstiarySlotsData data;
            data.playerPoints = m.points();
            data.totalPointsNextBonus = m.points_next_bonus();
            data.currentBonus = static_cast<uint16_t>(m.current_bonus());
            data.nextBonus = static_cast<uint16_t>(m.next_bonus());
            slotOf(m.first_slot(), data.isSlotOneUnlocked, data.bossIdSlotOne, data.slotOneData);
            slotOf(m.second_slot(), data.isSlotTwoUnlocked, data.bossIdSlotTwo, data.slotTwoData);
            slotOf(m.boosted_slot(), data.isTodaySlotUnlocked, data.boostedBossId, data.todaySlotData);
            data.bossesUnlocked = m.has_unlocked_bosses();
            for (const auto& boss : m.unlocked_bosses())
                data.bossesUnlockedData.push_back({ .bossId = boss.boss_id(), .bossRace = static_cast<uint8_t>(boss.race()) });
            tibia::game::processBosstiarySlots(data);
        }
        break;
    case 115:
        if (shadow->HasExtension(GameserverMessageBosstiary::gameserver_message_bosstiary)) {
            std::vector<BosstiaryData> bosses;
            for (const auto& b : shadow->GetExtension(GameserverMessageBosstiary::gameserver_message_bosstiary).bosses())
                bosses.push_back({ .raceId = b.race_id(), .category = static_cast<uint8_t>(b.category()), .kills = b.kills(),
                                   .isTrackerActived = static_cast<uint8_t>(b.tracked() ? 1 : 0) });
            tibia::game::processBosstiaryInfo(bosses);
        }
        break;
    case 230:
        if (shadow->HasExtension(GameserverMessageBosstiaryHighlight::gameserver_message_bosstiary_highlight))
            tibia::game::notify("onBosstiaryEntryChanged", shadow->GetExtension(GameserverMessageBosstiaryHighlight::gameserver_message_bosstiary_highlight).boss_id());
        break;
    case 189:
        if (shadow->HasExtension(GameserverMessageBossTracking::gameserver_message_boss_tracking)) {
            std::vector<BossCooldownData> cooldowns;
            for (const auto& e : shadow->GetExtension(GameserverMessageBossTracking::gameserver_message_boss_tracking).cooldowns())
                cooldowns.emplace_back(e.race_id(), e.cooldown());
            tibia::game::notify("onBossCooldown", cooldowns);
        }
        break;
    default:
        break;
    }
}

}
