#include "bountytaskscontroller.h"

#include <tibia/assets/things.h>

#include <QString>

#include <algorithm>
#include <climits>
#include <cmath>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

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
    const RaceType& race = tibia::game::raceData(raceId);
    if (race.name.empty())
        return QStringLiteral("Unknown");
    QString name = QString::fromStdString(race.name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

/// CIP External: shared panel chrome + per-tier badge (silver/gold/platinum).
QString tierBackdrop()
{
    return QStringLiteral("/images/taskboard/backdrop-bounty-tasks.png");
}

QString tierIcon(const uint8_t rarity)
{
    switch (rarity) {
    case 3:
        return QStringLiteral("/images/taskboard/icon-bounty-task-platinum.png");
    case 2:
        return QStringLiteral("/images/taskboard/icon-bounty-task-gold.png");
    case 1:
        return QStringLiteral("/images/taskboard/icon-bounty-task-silver.png");
    default:
        return {};
    }
}

uint64_t bountyPoints()
{
    return tibia::game::localPlayer()
               ? tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BOUNTY_POINTS)
               : 0;
}

uint64_t rerollTokenBalance()
{
    return tibia::game::localPlayer()
               ? tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS)
               : 0;
}

int clampedRerollTokenBalance()
{
    return static_cast<int>(std::min<uint64_t>(rerollTokenBalance(), static_cast<uint64_t>(INT_MAX)));
}

/// CIP QML combo is 0..3; OTC Lua helpers expect 1..4.
uint16_t toLuaDifficulty(const int cipDifficulty)
{
    return static_cast<uint16_t>(std::clamp(cipDifficulty, 0, 3) + 1);
}

/// Provisional elite Premium dust by CIP difficulty 0..3 × grade (1/2/4/8).
/// Matches server getEliteDustReward until External dust table is RE'd.
int eliteDustFor(const int cipDifficulty, const uint8_t rarity)
{
    static constexpr int kBaseByDifficulty[] = { 3, 7, 16, 27 };
    const int base = kBaseByDifficulty[std::clamp(cipDifficulty, 0, 3)];
    switch (rarity) {
    case 1:
        return base * 2;
    case 2:
        return base * 4;
    case 3:
        return base * 8;
    default:
        return base;
    }
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
    m_rerollTokenAmount = clampedRerollTokenBalance();
    m_bountyPointsBalance = static_cast<int>(bountyPoints());
}

void TBountyTasksController::showDialogTab()
{
    refreshBountyPoints();
    tibia::game::bountyTaskAction(tibia::client::BOUNTY_ACTION_REQUEST);
}

void TBountyTasksController::hideDialogTab() {}

void TBountyTasksController::refreshBountyPoints()
{
    bool changed = false;
    const int nextBounty = static_cast<int>(bountyPoints());
    if (nextBounty != m_bountyPointsBalance) {
        m_bountyPointsBalance = nextBounty;
        changed = true;
    }
    // CipSoft QML binds rerollTokenAmount into the currency view next to
    // "Reroll Task" — that is the player's 0x58 balance, not the spend cost.
    const int nextReroll = clampedRerollTokenBalance();
    if (nextReroll != m_rerollTokenAmount) {
        m_rerollTokenAmount = nextReroll;
        changed = true;
    }
    const bool limit = nextReroll >= BountyMaxRerollTokens;
    if (limit != m_limitOfRerollTokensReached) {
        m_limitOfRerollTokensReached = limit;
        changed = true;
    }
    const int canReroll = (m_freeDailyReroll || nextReroll > 0) ? 1 : 0;
    if (canReroll != m_canRerollTasks) {
        m_canRerollTasks = canReroll;
        changed = true;
    }
    if (changed)
        notifyChanged();
}

void TBountyTasksController::applyBountyData(const std::map<std::string, uint32_t>& header,
                                              const std::vector<std::map<std::string, uint32_t>>& monsters,
                                              const std::vector<std::map<std::string, uint32_t>>& talismans)
{
    m_rerollPoints = static_cast<int>(mapU32(header, "rerollPoints"));
    m_freeDailyReroll = mapU32(header, "claimDaily") != 0;
    // Protocol stores Lua difficulty 1..4; CIP combo uses 0..3.
    const int luaDifficulty = static_cast<int>(std::clamp<uint32_t>(mapU32(header, "difficulty", 1), 1, 4));
    m_bountyTaskDifficulty = luaDifficulty - 1;
    // Prefer ResourceBalance 0x58 (same SoT as the daily reward wall). Fall back
    // to the bounty header when the resource packet has not landed yet.
    int balance = clampedRerollTokenBalance();
    if (balance == 0 && m_rerollPoints > 0)
        balance = m_rerollPoints;
    m_rerollTokenAmount = balance;
    m_canRerollTasks = (m_freeDailyReroll || balance > 0) ? 1 : 0;
    m_limitOfRerollTokensReached = balance >= BountyMaxRerollTokens;
    m_bountyPointsBalance = static_cast<int>(bountyPoints());
    // Do not setResourceBalance here — emitting during dialog incubation
    // re-enters QML and can corrupt the heap.

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
        const uint8_t rarity = static_cast<uint8_t>(mapU32(monster, "rarity"));
        // OTC wire has no rewardDust field; when Elite checkbox is on, show provisional
        // dust so QML BountyTaskPanel can render (server grants dust only if elite was on at roll).
        const int wireDust = static_cast<int>(mapU32(monster, "rewardDust"));
        row.rewardDust = wireDust > 0 ? wireDust : (m_isElite ? eliteDustFor(m_bountyTaskDifficulty, rarity) : 0);
        const bool isActive = mapU32(monster, "isActive") != 0;
        const bool isCompleted = mapU32(monster, "isCompleted") != 0;
        row.canAccept = !isActive && !isCompleted && row.raceId > 0;
        row.canClaim = isCompleted || (isActive && total > 0 && current >= total);
        row.caption = raceCaption(static_cast<uint16_t>(row.raceId));
        row.taskTierImageSource = tierBackdrop();
        row.taskTierIconSource = tierIcon(rarity);
        // External client/RCC have no elite badge path (only silver/gold/platinum
        // tier icons). QML hides the Image when source === "".
        row.isEliteIconSource = {};
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
    tibia::game::bountyTaskAction(tibia::client::BOUNTY_ACTION_CHANGE_DIFFICULTY, toLuaDifficulty(m_bountyTaskDifficulty));
}

void TBountyTasksController::rerollTasksAction()
{
    tibia::game::bountyTaskAction(tibia::client::BOUNTY_ACTION_REROLL);
}

void TBountyTasksController::setElite(const bool isElite)
{
    if (m_isElite == isElite)
        return;
    m_isElite = isElite;
    if (m_tasksModel)
        m_tasksModel->setEliteDust(m_isElite, m_bountyTaskDifficulty);
    notifyChanged();
    // External OTC CHANGE_DIFFICULTY carries only the difficulty byte — no elite field
    // found in tip/server protocol or protobuf field names. Do not invent a wire byte.
}

void TBountyTasksController::acceptBountyTask(const int TaskIndex)
{
    tibia::game::bountyTaskAction(tibia::client::BOUNTY_ACTION_SELECT, static_cast<uint16_t>(std::max(0, TaskIndex)));
}

void TBountyTasksController::claimReward()
{
    tibia::game::bountyTaskAction(tibia::client::BOUNTY_ACTION_CLAIM_REWARD);
}

void TBountyTasksController::upgradeBountyTalisman(const int BountyTalismanIndex)
{
    if (BountyTalismanIndex < 0 || BountyTalismanIndex > 3)
        return;
    tibia::game::bountyTalismanUpgrade(static_cast<uint8_t>(BountyTalismanIndex));
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
    tibia::game::bountyPreferredAction(tibia::client::PREFERRED_ACTION_BUY_SLOT, static_cast<uint16_t>(std::max(1, slot)), 0);
}

void TBountyTasksController::clearPreferredMonster(const int slotIndex)
{
    tibia::game::bountyPreferredAction(tibia::client::PREFERRED_ACTION_REMOVE_PREFERRED, static_cast<uint16_t>(std::max(0, slotIndex)), 0);
}

void TBountyTasksController::clearAvoidedMonster(const int slotIndex)
{
    tibia::game::bountyPreferredAction(tibia::client::PREFERRED_ACTION_REMOVE_UNWANTED, static_cast<uint16_t>(std::max(0, slotIndex)), 0);
}

void TBountyTasksController::assignPreferredMonster(const int slotIndex, const int RaceId)
{
    tibia::game::bountyPreferredAction(tibia::client::PREFERRED_ACTION_SET_PREFERRED, static_cast<uint16_t>(std::max(0, slotIndex)),
                                 static_cast<uint16_t>(std::max(0, RaceId)));
}

void TBountyTasksController::assignAvoidedMonster(const int slotIndex, const int RaceId)
{
    tibia::game::bountyPreferredAction(tibia::client::PREFERRED_ACTION_SET_UNWANTED, static_cast<uint16_t>(std::max(0, slotIndex)),
                                 static_cast<uint16_t>(std::max(0, RaceId)));
}

void TBountyTasksController::clearNameFilter()
{
    if (m_monsterFilter)
        m_monsterFilter->setNameFilter(QString());
}

}
