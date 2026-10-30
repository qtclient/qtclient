#include "bountytaskscontroller.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/things.h>

#include <QString>

#include <algorithm>
#include <cmath>

namespace tibia::gamewindow {

namespace {

constexpr int BountyMaxRerollTokens = 10;

uint32_t mapU32(const std::map<std::string, uint32_t>& map, const char* key, const uint32_t fallback = 0)
{
    const auto it = map.find(key);
    return it == map.end() ? fallback : it->second;
}

QString raceCaption(const uint16_t raceId)
{
    if (raceId == 0)
        return QStringLiteral("Any Creature");
    const RaceType& race = g_things.getRaceData(raceId);
    if (race.name.empty())
        return QStringLiteral("Unknown");
    QString name = QString::fromStdString(race.name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

QString tierImage(const uint8_t rarity)
{
    switch (rarity) {
    case 2:
        return QStringLiteral("/images/taskboard/backdrop_tasksystem_gold_task.png");
    case 1:
        return QStringLiteral("/images/taskboard/backdrop_tasksystem_silver_task.png");
    default:
        return QStringLiteral("/images/taskboard/backdrop_tasksystem_normal_task.png");
    }
}

uint64_t bountyPoints()
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(Otc::RESOURCE_BOUNTY_POINTS) : 0;
}

/// CIP QML combo is 0..3; OTC Lua helpers expect 1..4.
uint16_t toLuaDifficulty(const int cipDifficulty)
{
    return static_cast<uint16_t>(std::clamp(cipDifficulty, 0, 3) + 1);
}

}

TBountyTasksController::TBountyTasksController(QObject* parent)
    : cip::tibia::gamewindow::TBountyTasksController(parent)
    , m_tasksModel(new taskboard::TBountyTasksListModel(this))
    , m_preferredModel(new taskboard::TBountyTasksPreferredMonsterListModel(this))
    , m_monsterSource(new taskboard::TMonsterListFilterModel(this))
    , m_monsterFilter(new taskboard::TMonsterListModelSortFilterProxy(this))
{
    m_bountyTasks = m_tasksModel;
    m_preferredSlotData = m_preferredModel;
    m_monsterFilter->setSourceModel(m_monsterSource);
    m_monsterList = m_monsterFilter;
    m_rerollTokenAmount = 1;
    m_bountyPointsBalance = static_cast<int>(bountyPoints());
}

void TBountyTasksController::showDialogTab()
{
    refreshBountyPoints();
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_REQUEST);
}

void TBountyTasksController::hideDialogTab() {}

void TBountyTasksController::refreshBountyPoints()
{
    const int next = static_cast<int>(bountyPoints());
    if (next == m_bountyPointsBalance)
        return;
    m_bountyPointsBalance = next;
    notifyChanged();
}

void TBountyTasksController::applyBountyData(const std::map<std::string, uint32_t>& header,
                                              const std::vector<std::map<std::string, uint32_t>>& monsters,
                                              const std::vector<std::map<std::string, uint32_t>>& talismans)
{
    m_rerollPoints = static_cast<int>(mapU32(header, "rerollPoints"));
    const bool claimDaily = mapU32(header, "claimDaily") != 0;
    // Protocol stores Lua difficulty 1..4; CIP combo uses 0..3.
    const int luaDifficulty = static_cast<int>(std::clamp<uint32_t>(mapU32(header, "difficulty", 1), 1, 4));
    m_bountyTaskDifficulty = luaDifficulty - 1;
    m_canRerollTasks = m_rerollPoints > 0 ? 1 : 0;
    m_limitOfRerollsReached = m_rerollPoints >= BountyMaxRerollTokens;
    m_canClaimRerollToken = (claimDaily && !m_limitOfRerollsReached) ? 1 : 0;
    m_noClaimRerollTokenReason = m_limitOfRerollsReached
        ? QStringLiteral("Maximum number of reroll tokens reached.")
        : (claimDaily ? QString() : QStringLiteral("Daily reroll token is not ready yet."));
    m_rerollTokenAmount = 1;
    m_bountyPointsBalance = static_cast<int>(bountyPoints());

    QVector<taskboard::BountyTaskRow> rows;
    rows.reserve(static_cast<int>(monsters.size()));
    for (const auto& monster : monsters) {
        taskboard::BountyTaskRow row;
        row.taskIndex = static_cast<int>(mapU32(monster, "taskIndex"));
        row.raceId = static_cast<int>(mapU32(monster, "raceId"));
        const int current = static_cast<int>(mapU32(monster, "currentKills"));
        const int total = static_cast<int>(mapU32(monster, "totalKills"));
        row.killText = QStringLiteral("%1 / %2 kills").arg(current).arg(total);
        row.rewardXp = static_cast<int>(mapU32(monster, "rewardXp"));
        row.rewardBountyPoints = static_cast<int>(mapU32(monster, "rewardPoints"));
        const bool isActive = mapU32(monster, "isActive") != 0;
        const bool isCompleted = mapU32(monster, "isCompleted") != 0;
        row.canAccept = !isActive && !isCompleted && row.raceId > 0;
        row.canClaim = isCompleted || (isActive && total > 0 && current >= total);
        row.caption = raceCaption(static_cast<uint16_t>(row.raceId));
        row.taskTierImageSource = tierImage(static_cast<uint8_t>(mapU32(monster, "rarity")));
        rows.push_back(std::move(row));
    }
    m_tasksModel->setRows(std::move(rows));
    applyTalismans(talismans);
    notifyChanged();
}

void TBountyTasksController::applyPreferredData(const std::vector<std::map<std::string, uint32_t>>& preferredSlots,
                                                 const std::vector<uint16_t>& raceIds)
{
    int clearCost = 0;
    int unlockCost = 0;
    bool sawLocked = false;
    QVector<taskboard::PreferredMonsterSlotRow> rows;
    rows.reserve(static_cast<int>(preferredSlots.size()));
    for (size_t i = 0; i < preferredSlots.size(); ++i) {
        const auto& slot = preferredSlots[i];
        taskboard::PreferredMonsterSlotRow row;
        // Protocol / bountyPreferredAction use 1-based slot indices (0 is rejected).
        row.slotIndex = static_cast<int>(mapU32(slot, "slot", static_cast<uint32_t>(i + 1)));
        if (row.slotIndex <= 0)
            row.slotIndex = static_cast<int>(i + 1);
        const bool locked = mapU32(slot, "locked") != 0;
        row.isUnlocked = !locked;
        row.unlockCost = static_cast<int>(mapU32(slot, "price"));
        row.preferredMonsterRaceId = static_cast<int>(mapU32(slot, "preferred"));
        row.avoidedMonsterRaceId = static_cast<int>(mapU32(slot, "unwanted"));
        row.preferredMonsterRaceName = raceCaption(static_cast<uint16_t>(row.preferredMonsterRaceId));
        row.avoidedMonsterRaceName = raceCaption(static_cast<uint16_t>(row.avoidedMonsterRaceId));
        if (locked && !sawLocked) {
            sawLocked = true;
            unlockCost = row.unlockCost;
            clearCost = row.unlockCost;
            row.canBeUnlocked = true;
        } else {
            row.canBeUnlocked = false;
        }
        if (row.isUnlocked && clearCost == 0 && row.unlockCost > 0)
            clearCost = row.unlockCost;
        rows.push_back(std::move(row));
    }
    m_preferredModel->setClearCost(clearCost > 0 ? clearCost : unlockCost);
    m_preferredModel->setRows(std::move(rows));

    QVector<taskboard::MonsterRaceRow> races;
    races.reserve(static_cast<int>(raceIds.size()));
    for (const uint16_t raceId : raceIds) {
        taskboard::MonsterRaceRow race;
        race.raceId = raceId;
        race.name = raceCaption(raceId);
        races.push_back(std::move(race));
    }
    std::sort(races.begin(), races.end(), [](const taskboard::MonsterRaceRow& a, const taskboard::MonsterRaceRow& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    m_monsterSource->setRows(std::move(races));
    notifyChanged();
}

void TBountyTasksController::applyTalismans(const std::vector<std::map<std::string, uint32_t>>& talismans)
{
    const auto valueAt = [&](const size_t index, const char* key) -> double {
        if (index >= talismans.size())
            return 0.0;
        return static_cast<double>(mapU32(talismans[index], key)) / 100.0;
    };
    const auto costAt = [&](const size_t index) -> int {
        if (index >= talismans.size())
            return 0;
        return static_cast<int>(mapU32(talismans[index], "upgradeCost"));
    };
    const auto canUpgrade = [&](const size_t index) -> bool {
        if (index >= talismans.size())
            return false;
        return mapU32(talismans[index], "nextValue") > 0 && mapU32(talismans[index], "upgradeCost") > 0;
    };
    const auto maxed = [&](const size_t index) -> bool {
        if (index >= talismans.size())
            return true;
        return mapU32(talismans[index], "nextValue") == 0;
    };

    m_bonusDamagePerCent = valueAt(0, "currentValue");
    m_nextBonusDamagePerCent = valueAt(0, "nextValue");
    m_bonusDamageUpgradeCost = costAt(0);
    m_canUpgradeBonusDamage = canUpgrade(0);
    m_maximumReachedBonusDamage = maxed(0);

    m_lifeLeechPerCent = valueAt(1, "currentValue");
    m_nextLifeLeechPerCent = valueAt(1, "nextValue");
    m_lifeLeechUpgradeCost = costAt(1);
    m_canUpgradeLifeLeech = canUpgrade(1);
    m_maximumReachedBonusLifeLeech = maxed(1);

    m_bonusLootPerCent = valueAt(2, "currentValue");
    m_nextBonusLootPerCent = valueAt(2, "nextValue");
    m_bonusLootUpgradeCost = costAt(2);
    m_canUpgradeBonusLoot = canUpgrade(2);
    m_maximumReachedBonusLoot = maxed(2);

    m_doubleBestiaryChancePerCent = valueAt(3, "currentValue");
    m_nextDoubleBestiaryChancePerCent = valueAt(3, "nextValue");
    m_doubleBestiaryChanceUpgradeCost = costAt(3);
    m_canUpgradeDoubleBestiaryChance = canUpgrade(3);
    m_maximumReachedBonusBestiary = maxed(3);
}

void TBountyTasksController::notifyChanged() { Q_EMIT bountyTaskDataChanged(); }

void TBountyTasksController::setBountyTaskDifficulty(const int Difficulty)
{
    if (Difficulty == m_bountyTaskDifficulty)
        return;
    m_bountyTaskDifficulty = std::clamp(Difficulty, 0, 3);
    notifyChanged();
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_CHANGE_DIFFICULTY, toLuaDifficulty(m_bountyTaskDifficulty));
}

void TBountyTasksController::rerollTasksAction()
{
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_REROLL);
}

void TBountyTasksController::claimRerollTokenAction()
{
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_CLAIM_DAILY);
}

void TBountyTasksController::acceptBountyTask(const int TaskIndex)
{
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_SELECT, static_cast<uint16_t>(std::max(0, TaskIndex)));
}

void TBountyTasksController::claimReward()
{
    g_game.bountyTaskAction(Otc::BOUNTY_ACTION_CLAIM_REWARD);
}

void TBountyTasksController::upgradeBountyTalisman(const int BountyTalismanIndex)
{
    if (BountyTalismanIndex < 0 || BountyTalismanIndex > 3)
        return;
    g_game.bountyTalismanUpgrade(static_cast<uint8_t>(BountyTalismanIndex));
}

void TBountyTasksController::unlockPreferredMonsterSlot()
{
    // Unlock targets the first locked slot (1-based); 0 is rejected by Game::bountyPreferredAction.
    int slot = 1;
    if (m_preferredModel) {
        for (int row = 0; row < m_preferredModel->rowCount(); ++row) {
            const QModelIndex idx = m_preferredModel->index(row, 0);
            if (!m_preferredModel->data(idx, taskboard::TBountyTasksPreferredMonsterListModel::IsUnlockedRole).toBool()) {
                slot = m_preferredModel->data(idx, taskboard::TBountyTasksPreferredMonsterListModel::SlotIndexRole).toInt();
                break;
            }
        }
    }
    g_game.bountyPreferredAction(Otc::PREFERRED_ACTION_BUY_SLOT, static_cast<uint16_t>(std::max(1, slot)), 0);
}

void TBountyTasksController::clearPreferredMonster(const int slotIndex)
{
    g_game.bountyPreferredAction(Otc::PREFERRED_ACTION_REMOVE_PREFERRED, static_cast<uint16_t>(std::max(0, slotIndex)), 0);
}

void TBountyTasksController::clearAvoidedMonster(const int slotIndex)
{
    g_game.bountyPreferredAction(Otc::PREFERRED_ACTION_REMOVE_UNWANTED, static_cast<uint16_t>(std::max(0, slotIndex)), 0);
}

void TBountyTasksController::assignPreferredMonster(const int slotIndex, const int RaceId)
{
    g_game.bountyPreferredAction(Otc::PREFERRED_ACTION_SET_PREFERRED, static_cast<uint16_t>(std::max(0, slotIndex)),
                                 static_cast<uint16_t>(std::max(0, RaceId)));
}

void TBountyTasksController::assignAvoidedMonster(const int slotIndex, const int RaceId)
{
    g_game.bountyPreferredAction(Otc::PREFERRED_ACTION_SET_UNWANTED, static_cast<uint16_t>(std::max(0, slotIndex)),
                                 static_cast<uint16_t>(std::max(0, RaceId)));
}

void TBountyTasksController::clearNameFilter()
{
    if (m_monsterFilter)
        m_monsterFilter->setNameFilter(QString());
}

bool TBountyTasksController::isAtRerollTokenLimit() { return m_limitOfRerollsReached; }

}
