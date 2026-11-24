
#include "bossbestiary/bossbestiaryciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"
#include "client/staticdata.h"
#include "input/gameaction.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

#include <QCoreApplication>

#include <stdexcept>

namespace tibia::bossbestiary {

TBossBestiaryProtocolMessageHandler::TBossBestiaryProtocolMessageHandler(std::shared_ptr<cyclopedia::TMonsterRaceStorage> pMonsterRaceStorage,
                                                                         std::shared_ptr<TBossTrackerStorage> pBossTrackerStorage,
                                                                         std::shared_ptr<TBosstiaryStorage> pBosstiaryStorage, QObject* parent)
    : cip::tibia::bossbestiary::TBossBestiaryProtocolMessageHandler(parent)
    , m_pMonsterRaceStorage(std::move(pMonsterRaceStorage))
    , m_pBossTrackerStorage(std::move(pBossTrackerStorage))
    , m_pBosstiaryStorage(std::move(pBosstiaryStorage))
{
}

} // namespace tibia::bossbestiary

namespace tibia::bossbestiary {

/// The protocol's boss rarity (bane, archfoe, nemesis); any other value throws.
int bossCategory(const uint32_t protocolValue)
{
    if (protocolValue > 2)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(protocolValue);
}

/// The protocol's race repeat value of a boss slot (0..3); any other value throws.
int bossRaceRepeat(const uint32_t protocolValue)
{
    if (protocolValue > 3)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(protocolValue);
}

void TBossBestiaryProtocolMessageHandler::handleBosstiaryStaticDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryStaticData& BosstiaryStaticDataMessage)
{
    TBosstiaryStorage* storage = m_pBosstiaryStorage.get();
    const auto& m = BosstiaryStaticDataMessage;
    if (m.kills_for_stage_size() < 9 || m.points_for_stage_size() < 9)
        return;
    // Three stages a rarity: bane, archfoe, nemesis.
    TBossStageValues kills{};
    TBossStageValues points{};
    for (int i = 0; i < 9; ++i) {
        kills[i / 3][i % 3] = static_cast<int>(m.kills_for_stage(i));
        points[i / 3][i % 3] = static_cast<int>(m.points_for_stage(i));
    }
    storage->setKillsForStage(kills);
    storage->setPointsForStage(points);
}

void TBossBestiaryProtocolMessageHandler::handleShowMessageDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& ShowMessageDialogMessage)
{
    constexpr uint32_t FreePodiumDialogType = 60;
    if (ShowMessageDialogMessage.dialog_type() != FreePodiumDialogType)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionShowBosstiaryFreePodiumDialog>(
        QString::fromStdString(ShowMessageDialogMessage.message())));
}

void TBossBestiaryProtocolMessageHandler::handleBossSlotsMessage(const ::tibia::protobuf::protocol::GameserverMessageBossSlots& BossSlotsMessage)
{
    using namespace ::tibia::protobuf::protocol;
    const auto& m = BossSlotsMessage;
    // A locked slot carries the boss points that unlock it; an unlocked one
    // always carries its boss details (race 0 when the slot is empty).
    const auto slotOf = [](const ProtocolBossSlot& protocolSlot) {
        TBossSlot slot;
        slot.unlocked = protocolSlot.unlocked();
        if (!slot.unlocked) {
            slot.unlockBosspoints = static_cast<int>(protocolSlot.locked_value());
            return slot;
        }
        const ProtocolBossSlotBoss& boss = protocolSlot.boss();
        slot.raceId = static_cast<int>(static_cast<uint16_t>(boss.boss_id()));
        if (slot.raceId != 0)
            slot.name = cyclopedia::bossName(slot.raceId);
        slot.category = bossCategory(boss.race());
        slot.kills = protocolSlot.kill_count();
        slot.lootBonus = static_cast<int>(protocolSlot.loot_bonus());
        slot.killBonus = static_cast<int>(protocolSlot.kill_bonus());
        slot.raceRepeat = bossRaceRepeat(protocolSlot.race_repeat());
        slot.removePrice = protocolSlot.remove_price();
        slot.inactive = protocolSlot.inactive();
        slot.hasBoss = true;
        return slot;
    };
    TBossSlotsData slots;
    slots.currentBosspoints = static_cast<int>(m.points());
    slots.maximumBosspoints = static_cast<int>(m.points_next_bonus());
    slots.currentLootBonus = static_cast<int>(m.current_bonus());
    slots.nextLootBonus = static_cast<int>(m.next_bonus());
    slots.first = slotOf(m.first_slot());
    slots.second = slotOf(m.second_slot());
    slots.boosted = slotOf(m.boosted_slot());
    slots.hasUnlockedBosses = m.unlocked_bosses_size() != 0;
    for (const auto& boss : m.unlocked_bosses()) {
        const int raceId = static_cast<int>(static_cast<uint16_t>(boss.boss_id()));
        slots.assignable.push_back({ raceId, bossCategory(boss.race()), cyclopedia::bossName(raceId) });
    }
    m_pBosstiaryStorage->setBossSlots(std::move(slots));
}

void TBossBestiaryProtocolMessageHandler::handleBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage)
{
    std::vector<TBosstiaryEntry> entries;
    std::vector<BosstiaryData> bosses;
    for (const auto& b : BosstiaryMessage.bosses()) {
        TBosstiaryEntry& entry = entries.emplace_back();
        entry.raceId = static_cast<int>(b.race_id());
        entry.name = cyclopedia::bossName(entry.raceId);
        entry.category = bossCategory(b.category());
        entry.kills = b.kills();
        entry.tracked = b.tracked();
        bosses.push_back({ .raceId = b.race_id(), .category = static_cast<uint8_t>(b.category()), .kills = b.kills(),
                           .isTrackerActived = static_cast<uint8_t>(b.tracked() ? 1 : 0) });
    }
    m_pBosstiaryStorage->setEntries(std::move(entries));
    tibia::game::processBosstiaryInfo(bosses);
}

void TBossBestiaryProtocolMessageHandler::handleBosstiaryHighlightMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryHighlight& BosstiaryHighlightMessage)
{
    Q_EMIT requestBosstiaryButtonHighlight(static_cast<quint16>(BosstiaryHighlightMessage.boss_id()));
}

void TBossBestiaryProtocolMessageHandler::handleBossTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageBossTracking& BossTrackingMessage)
{
    using namespace ::tibia::protobuf::protocol;
    std::vector<BossCooldownData> cooldowns;
    for (const auto& e : BossTrackingMessage.cooldowns())
        cooldowns.emplace_back(e.race_id(), e.cooldown());
    tibia::game::notify("onBossCooldown", cooldowns);
}

}
