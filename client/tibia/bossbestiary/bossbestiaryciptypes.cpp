
#include "bossbestiary/bossbestiaryciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"
#include "client/staticdata.h"
#include "input/gameaction.h"
#include <tibia/game/gamenotify.h>
#include <protocol.pb.h>

#include <QCoreApplication>

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
    TBosstiaryStorage* storage = m_pBosstiaryStorage.get();
    const auto& m = BossSlotsMessage;
    // A locked slot carries the boss points that unlock it where an unlocked one names its boss.
    const auto slotOf = [](const ProtocolBossSlot& protocolSlot) {
        TBossSlot slot;
        slot.unlocked = protocolSlot.unlocked();
        if (!slot.unlocked) {
            slot.unlockBosspoints = static_cast<int>(protocolSlot.locked_value());
            return slot;
        }
        const uint32_t bossId = protocolSlot.boss().boss_id();
        if (bossId == 0)
            return slot;
        slot.hasBoss = true;
        slot.raceId = static_cast<int>(bossId);
        slot.name = cyclopedia::bossName(slot.raceId);
        slot.category = static_cast<int>(protocolSlot.boss().race());
        slot.kills = protocolSlot.kill_count();
        slot.lootBonus = static_cast<int>(protocolSlot.loot_bonus());
        slot.killBonus = static_cast<int>(protocolSlot.kill_bonus());
        slot.removePrice = protocolSlot.remove_price();
        slot.inactive = protocolSlot.inactive();
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
    for (const auto& boss : m.unlocked_bosses())
        slots.assignable.push_back({ static_cast<int>(boss.boss_id()), static_cast<int>(boss.race()),
                                     cyclopedia::bossName(static_cast<int>(boss.boss_id())) });
    storage->setBossSlots(std::move(slots));
}

void TBossBestiaryProtocolMessageHandler::handleBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage)
{
    std::vector<TBosstiaryEntry> entries;
    std::vector<BosstiaryData> bosses;
    for (const auto& b : BosstiaryMessage.bosses()) {
        TBosstiaryEntry& entry = entries.emplace_back();
        entry.raceId = static_cast<int>(b.race_id());
        entry.name = cyclopedia::bossName(entry.raceId);
        entry.category = static_cast<int>(b.category());
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
