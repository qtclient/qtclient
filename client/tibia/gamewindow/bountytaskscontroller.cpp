#include "bountytaskscontroller.h"

#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardciptypes.h"
#include "taskboard/taskboardcurrenciesstorage.h"

#include <tibia/assets/things.h>

#include <QString>

#include <algorithm>
#include <limits>
#include <vector>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

/// A talisman at this level cannot be upgraded any further.
constexpr int TalismanMaxLevel = 355;
/// From this many reroll tokens on no more can be gained.
constexpr int RerollTokenLimit = 10;
/// The level the adept, expert and master difficulties need.
constexpr int RequiredLevelForDifficulty[] = { 30, 150, 400 };

/// The session's task board protocol handler; null without a game session.
taskboard::TTaskboardProtocolMessageHandler* taskboardHandler(const std::shared_ptr<game::IGameSession>& session)
{
    const auto* base = dynamic_cast<const game::TGameSessionBase*>(session.get());
    return base ? base->taskboardProtocolMessageHandler() : nullptr;
}

/// The bonus of a talisman level in per cent: 2.5% plus 0.5% per level up to
/// level 15, 0.25% per level up to 55 and 0.1% per level up to 355.
double talismanPerCent(const int level)
{
    const double fromFiftyFive = level >= 55 ? static_cast<double>(std::min(level, TalismanMaxLevel) - 55) * 0.001 : 0.0;
    const double fromFifteen = level >= 15 ? static_cast<double>(std::min(level, 55) - 15) * 0.0025 : 0.0;
    return (fromFiftyFive + static_cast<double>(std::min(level, 15)) * 0.005 + 0.025 + fromFifteen) * 100.0;
}

/// The bestiary talisman doubles the bonus of the level.
double bestiaryTalismanPerCent(const int level)
{
    const double fromFiftyFive = level >= 55 ? static_cast<double>(std::min(level, TalismanMaxLevel) - 55) * 0.001 : 0.0;
    const double fromFifteen = level >= 15 ? static_cast<double>(std::min(level, 55) - 15) * 0.0025 : 0.0;
    const double value = fromFiftyFive + static_cast<double>(std::min(level, 15)) * 0.005 + 0.025 + fromFifteen;
    return (value + value) * 100.0;
}

/// The races a preferred-monster slot already holds (preferred and avoided).
std::vector<uint32_t> assignedRaceIds(const std::vector<taskboard::TBountyPreferredMonsterSlot>& slots)
{
    std::vector<uint32_t> raceIds;
    for (const taskboard::TBountyPreferredMonsterSlot& slot : slots) {
        if (slot.preferredRaceId != 0)
            raceIds.push_back(slot.preferredRaceId);
        if (slot.avoidedRaceId != 0)
            raceIds.push_back(slot.avoidedRaceId);
    }
    return raceIds;
}

QVector<taskboard::PreferredMonsterSlotRow> preferredSlotRows(const std::vector<taskboard::TBountyPreferredMonsterSlot>& slots,
                                                              int& clearCost)
{
    QVector<taskboard::PreferredMonsterSlotRow> rows;
    rows.reserve(static_cast<int>(slots.size()));
    int unlockCost = 0;
    clearCost = 0;
    for (const taskboard::TBountyPreferredMonsterSlot& slot : slots) {
        taskboard::PreferredMonsterSlotRow row;
        // The model shows 1-based slot numbers; the commands send the 0-based slot.
        row.slotIndex = slot.index + 1;
        row.isUnlocked = slot.unlocked;
        row.unlockCost = slot.unlockCost;
        row.canBeUnlocked = slot.nextToUnlock;
        row.preferredMonsterRaceId = static_cast<int>(slot.preferredRaceId);
        row.avoidedMonsterRaceId = static_cast<int>(slot.avoidedRaceId);
        if (slot.nextToUnlock && unlockCost == 0)
            unlockCost = slot.unlockCost;
        if (slot.unlocked && clearCost == 0 && slot.unlockCost > 0)
            clearCost = slot.unlockCost;
        rows.push_back(std::move(row));
    }
    if (clearCost == 0)
        clearCost = unlockCost;
    return rows;
}

/// The monster races the preferred-monster picker offers, in static data
/// order: monsters with both static data flags (6 and 7) whose bestiary
/// difficulty is 2..5.
std::vector<uint16_t> bountyMonsterRaceIds()
{
    std::vector<uint16_t> raceIds;
    for (const RaceType& race : tibia::game::getAllRaces()) {
        if (race.category != 0 && race.flag7 && race.difficulty >= 2 && race.difficulty <= 5)
            raceIds.push_back(static_cast<uint16_t>(race.raceId));
    }
    return raceIds;
}

}

TBountyTasksController::TBountyTasksController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent)
    : cip::tibia::gamewindow::TBountyTasksController(parent)
    , m_tasksModel(new taskboard::TBountyTasksListModel(this))
    , m_monsterFilter(new taskboard::TMonsterListModelSortFilterProxy(bountyMonsterRaceIds(), this))
    , m_preferredModel(new taskboard::TBountyTasksPreferredMonsterListModel(this))
    , m_rerollConfirmation(engine, dialogParentItem)
{
    m_bountyTasks = m_tasksModel;
    m_monsterList = m_monsterFilter;
    m_preferredSlotData = m_preferredModel;
    m_rerollConfirmation.setCloseOnButtonClicked(true);
    connect(&m_rerollConfirmation, &TMessageDialogController::acceptClicked, this,
            [this] { sendRequest([this] { if (auto* handler = taskboardHandler(m_gameSession)) handler->rerollBountyTasks(); }); });
}

void TBountyTasksController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_rerollConfirmation.dialog()->close();
    hideDialogTab();
    m_gameSessionConnections.disconnectAll();
    m_hasSelection = false;
    m_gameSession = GameSession;
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get())) {
        if (taskboard::TTaskboardBountyTasksStorage* storage = session->taskboardBountyTasksStorage().get())
            m_gameSessionConnections.add(connect(storage, &taskboard::TTaskboardBountyTasksStorage::bountyTaskDataChanged, this,
                                                 [this](const taskboard::TBountyTaskData& data) { onBountyTaskDataReceived(data); }));
        taskboard::TTaskboardCurrenciesStorage* currencyStorage = session->taskboardCurrenciesStorage().get();
        using Currencies = taskboard::TTaskboardCurrenciesStorage;
        using Self = TBountyTasksController;
        m_gameSessionConnections.add(connect(currencyStorage, &Currencies::bountyPointsChanged, this, &Self::bountyTaskDataChanged));
        m_gameSessionConnections.add(
            connect(currencyStorage, &Currencies::bountyTaskRerollTokensChanged, this, &Self::bountyTaskDataChanged));
        if (game::TPlayerData* player = session->playerData().get())
            m_gameSessionConnections.add(
                connect(player, &game::TPlayerData::playerDataChanged, this, &Self::updateMaxBountyTaskDifficulty));
    }
    updateMaxBountyTaskDifficulty();
}

void TBountyTasksController::updateMaxBountyTaskDifficulty()
{
    int difficulty = 0;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (const game::TPlayerData* player = session ? session->playerData().get() : nullptr) {
        const uint16_t level = player->level();
        if (level >= 400)
            difficulty = 3;
        else if (level >= 150)
            difficulty = 2;
        else if (level >= 30)
            difficulty = 1;
    }
    if (difficulty == m_maxBountyTaskDifficulty)
        return;
    m_maxBountyTaskDifficulty = difficulty;
    Q_EMIT maxBountyTaskDifficultyChanged();
}

int TBountyTasksController::getRequiredLevelForDifficulty(const int Difficulty)
{
    const unsigned index = static_cast<uint8_t>(Difficulty - 1);
    return index < 3 ? RequiredLevelForDifficulty[index] : 0;
}

void TBountyTasksController::onBountyTaskDataReceived(const taskboard::TBountyTaskData& data)
{
    std::vector<taskboard::TBountyTask> tasks = data.tasks;
    const auto available = [](const taskboard::TBountyTask& task) {
        return task.claimState == taskboard::EBountyTaskClaimState::Available;
    };
    if (std::all_of(tasks.begin(), tasks.end(), available)) {
        for (taskboard::TBountyTask& task : tasks)
            task.elite = data.elite;
    }
    m_tasksModel->setItems(tasks);
    int clearCost = 0;
    QVector<taskboard::PreferredMonsterSlotRow> slotRows = preferredSlotRows(data.preferredMonsterSlots, clearCost);
    m_preferredModel->setClearCost(clearCost);
    m_preferredModel->setRows(std::move(slotRows));
    // Host: the static data can load after the controller is made, so the
    // picker's races are built again here (CipSoft builds them once, with the proxy).
    m_monsterFilter->monsterListFilterModel()->setRaceIds(bountyMonsterRaceIds());
    m_monsterFilter->setExcludedRaceIds(assignedRaceIds(data.preferredMonsterSlots));
    // The bounty data answers the pending request; its difficulty and elite
    // mode become the selection.
    m_selectedDifficulty = data.difficulty;
    m_selectedElite = data.elite;
    m_hasSelection = true;
    m_requestPending = false;
    Q_EMIT bountyTaskDataChanged();
}

const taskboard::TBountyTaskData* TBountyTasksController::bountyTaskData() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    const taskboard::TTaskboardBountyTasksStorage* storage = session ? session->taskboardBountyTasksStorage().get() : nullptr;
    return storage ? &storage->bountyTaskData() : nullptr;
}

const taskboard::TTaskboardCurrenciesStorage* TBountyTasksController::currencies() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->taskboardCurrenciesStorage().get() : nullptr;
}

int TBountyTasksController::talismanLevel(const int talismanIndex) const
{
    const taskboard::TBountyTaskData* data = bountyTaskData();
    return data ? data->talismans[static_cast<size_t>(talismanIndex)].level : 0;
}

bool TBountyTasksController::talismanUpgradable(const int talismanIndex) const
{
    const taskboard::TBountyTaskData* data = bountyTaskData();
    return data && data->talismans[static_cast<size_t>(talismanIndex)].upgradable;
}

int TBountyTasksController::talismanUpgradeCost(const int talismanIndex) const
{
    const taskboard::TBountyTaskData* data = bountyTaskData();
    return data ? data->talismans[static_cast<size_t>(talismanIndex)].upgradeCost : 0;
}

QString TBountyTasksController::disabledTalismanTooltip(const int talismanIndex) const
{
    if (bountyTaskData() && !talismanUpgradable(talismanIndex)) {
        if (talismanLevel(talismanIndex) == TalismanMaxLevel)
            return qtTrId("taskboard_tooltip_task_ring_max_level");
        if (bountyPointsBalance() < talismanUpgradeCost(talismanIndex))
            return qtTrId("taskboard_tooltip_no_bounty_points");
    }
    return QString();
}

int TBountyTasksController::bountyTaskDifficulty() const
{
    const taskboard::TBountyTaskData* data = bountyTaskData();
    return data ? data->difficulty : 0;
}

bool TBountyTasksController::isElite() const
{
    const taskboard::TBountyTaskData* data = bountyTaskData();
    return data && data->elite;
}

int TBountyTasksController::canRerollTasks() const
{
    const taskboard::TTaskboardCurrenciesStorage* storage = currencies();
    return storage && storage->bountyTaskRerollTokens() > 0;
}

int TBountyTasksController::rerollTokenAmount() const
{
    const taskboard::TTaskboardCurrenciesStorage* storage = currencies();
    return storage ? storage->bountyTaskRerollTokens() : 0;
}

bool TBountyTasksController::limitOfRerollTokensReached() const
{
    const taskboard::TTaskboardCurrenciesStorage* storage = currencies();
    return storage && storage->bountyTaskRerollTokens() >= RerollTokenLimit;
}

int TBountyTasksController::bountyPointsBalance() const
{
    const taskboard::TTaskboardCurrenciesStorage* storage = currencies();
    return storage ? storage->bountyPoints() : 0;
}

double TBountyTasksController::bonusDamagePerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(0)) : 0.0;
}

double TBountyTasksController::nextBonusDamagePerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(0) + 1) : 0.0;
}

bool TBountyTasksController::canUpgradeBonusDamage() const { return talismanUpgradable(0); }

int TBountyTasksController::bonusDamageUpgradeCost() const { return talismanUpgradeCost(0); }

QString TBountyTasksController::disabledDamagePercentTooltip() const { return disabledTalismanTooltip(0); }

bool TBountyTasksController::maximumReachedBonusDamage() const
{
    return bountyTaskData() && talismanLevel(0) == TalismanMaxLevel;
}

double TBountyTasksController::lifeLeechPerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(1)) : 0.0;
}

double TBountyTasksController::nextLifeLeechPerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(1) + 1) : 0.0;
}

bool TBountyTasksController::canUpgradeLifeLeech() const { return talismanUpgradable(1); }

int TBountyTasksController::lifeLeechUpgradeCost() const { return talismanUpgradeCost(1); }

QString TBountyTasksController::disabledLifeLeechPercentTooltip() const { return disabledTalismanTooltip(1); }

bool TBountyTasksController::maximumReachedBonusLifeLeech() const
{
    return bountyTaskData() && talismanLevel(1) == TalismanMaxLevel;
}

double TBountyTasksController::bonusLootPerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(2)) : 0.0;
}

double TBountyTasksController::nextBonusLootPerCent() const
{
    return bountyTaskData() ? talismanPerCent(talismanLevel(2) + 1) : 0.0;
}

bool TBountyTasksController::canUpgradeBonusLoot() const { return talismanUpgradable(2); }

int TBountyTasksController::bonusLootUpgradeCost() const { return talismanUpgradeCost(2); }

QString TBountyTasksController::disabledLootPercentTooltip() const { return disabledTalismanTooltip(2); }

bool TBountyTasksController::maximumReachedBonusLoot() const
{
    return bountyTaskData() && talismanLevel(2) == TalismanMaxLevel;
}

double TBountyTasksController::doubleBestiaryChancePerCent() const
{
    return bountyTaskData() ? bestiaryTalismanPerCent(talismanLevel(3)) : 0.0;
}

double TBountyTasksController::nextDoubleBestiaryChancePerCent() const
{
    return bountyTaskData() ? bestiaryTalismanPerCent(talismanLevel(3) + 1) : 0.0;
}

bool TBountyTasksController::canUpgradeDoubleBestiaryChance() const { return talismanUpgradable(3); }

int TBountyTasksController::doubleBestiaryChanceUpgradeCost() const { return talismanUpgradeCost(3); }

QString TBountyTasksController::disabledBestiaryPercentTooltip() const { return disabledTalismanTooltip(3); }

bool TBountyTasksController::maximumReachedBonusBestiary() const
{
    return bountyTaskData() && talismanLevel(3) == TalismanMaxLevel;
}

QObject* TBountyTasksController::bountyTasks() const { return m_tasksModel; }

QObject* TBountyTasksController::monsterList() const { return m_monsterFilter; }

QObject* TBountyTasksController::preferredSlotData() const { return m_preferredModel; }

void TBountyTasksController::showDialogTab()
{
    if (auto* handler = taskboardHandler(m_gameSession))
        handler->requestBountyTasks();
}

void TBountyTasksController::hideDialogTab()
{
    m_rerollConfirmation.dialog()->close();
    clearNameFilter();
    m_showPreferredList = false;
    Q_EMIT bountyTaskDataChanged();
}

void TBountyTasksController::sendRequest(const std::function<void()>& send)
{
    if (!m_gameSession || m_requestPending)
        return;
    send();
    m_requestPending = true;
}

void TBountyTasksController::selectDifficulty(const int difficulty, const bool elite)
{
    if (m_hasSelection && m_selectedDifficulty == difficulty && m_selectedElite == elite)
        return;
    if (!m_gameSession)
        return;
    m_selectedDifficulty = difficulty;
    m_selectedElite = elite;
    m_hasSelection = true;
    if (auto* handler = taskboardHandler(m_gameSession))
        handler->setBountyTaskDifficulty(difficulty, elite);
}

void TBountyTasksController::setBountyTaskDifficulty(const int Difficulty)
{
    if (Difficulty > m_maxBountyTaskDifficulty)
        return;
    // The elite mode stays the one of the current bounty data.
    const taskboard::TBountyTaskData* data = bountyTaskData();
    selectDifficulty(Difficulty, data && data->elite);
}

void TBountyTasksController::setElite(const bool isElite)
{
    // The difficulty stays the one of the current bounty data.
    const taskboard::TBountyTaskData* data = bountyTaskData();
    selectDifficulty(data ? data->difficulty : 0, isElite);
}

/// While a task is taken the player confirms first (Yes sends the reroll).
void TBountyTasksController::rerollTasksAction()
{
    if (!m_gameSession || m_requestPending)
        return;
    const taskboard::TBountyTaskData* data = bountyTaskData();
    const bool taskTaken = data && std::any_of(data->tasks.begin(), data->tasks.end(), [](const taskboard::TBountyTask& task) {
        return task.claimState == taskboard::EBountyTaskClaimState::Active;
    });
    if (taskTaken) {
        m_rerollConfirmation.setYesNoButtons();
        m_rerollConfirmation.showMessage(qtTrId("taskboard_reroll_confirmation_caption"), qtTrId("taskboard_reroll_confirmation_text"));
        return;
    }
    sendRequest([this] { if (auto* handler = taskboardHandler(m_gameSession)) handler->rerollBountyTasks(); });
}

void TBountyTasksController::acceptBountyTask(const int TaskIndex)
{
    sendRequest([this, TaskIndex] { if (auto* handler = taskboardHandler(m_gameSession)) handler->acceptBountyTask(TaskIndex); });
}

void TBountyTasksController::claimReward()
{
    sendRequest([this] { if (auto* handler = taskboardHandler(m_gameSession)) handler->claimBountyReward(); });
}

void TBountyTasksController::upgradeBountyTalisman(const int BountyTalismanIndex)
{
    sendRequest([this, BountyTalismanIndex] { if (auto* handler = taskboardHandler(m_gameSession)) handler->upgradeBountyTalisman(BountyTalismanIndex); });
}

void TBountyTasksController::unlockPreferredMonsterSlot()
{
    sendRequest([this] { if (auto* handler = taskboardHandler(m_gameSession)) handler->unlockPreferredMonsterSlot(); });
}

void TBountyTasksController::clearPreferredMonster(const int slotIndex)
{
    sendRequest([this, slotIndex] { if (auto* handler = taskboardHandler(m_gameSession)) handler->clearPreferredMonster(slotIndex); });
}

void TBountyTasksController::clearAvoidedMonster(const int slotIndex)
{
    sendRequest([this, slotIndex] { if (auto* handler = taskboardHandler(m_gameSession)) handler->clearAvoidedMonster(slotIndex); });
}

void TBountyTasksController::assignPreferredMonster(const int slotIndex, const int RaceId)
{
    sendRequest([this, slotIndex, RaceId] { if (auto* handler = taskboardHandler(m_gameSession)) handler->assignPreferredMonster(slotIndex, RaceId); });
}

void TBountyTasksController::assignAvoidedMonster(const int slotIndex, const int RaceId)
{
    sendRequest([this, slotIndex, RaceId] { if (auto* handler = taskboardHandler(m_gameSession)) handler->assignAvoidedMonster(slotIndex, RaceId); });
}

/// Also forgets the creature picked for a preferred / avoided slot.
void TBountyTasksController::clearNameFilter()
{
    m_monsterFilter->setNameFilter(QString());
    m_preferredModel->clearAssignableMonster();
}

}
