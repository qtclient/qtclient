#pragma once

#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/taskboardmodels.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace tibia::game {
class IGameSession;
}
namespace tibia::taskboard {
struct TBountyTaskData;
class TTaskboardCurrenciesStorage;
}

namespace tibia {
namespace gamewindow {

/// The task board's bounty tab. Its properties read the session's bounty task
/// data and task board currencies; a bounty board message refills the task,
/// preferred-slot and monster lists. A request to the server waits for the
/// next bounty data before another one goes out. The player's level limits
/// the difficulties that can be selected; rerolling while a task is taken
/// asks for confirmation first.
class TBountyTasksController final : public cip::tibia::gamewindow::TBountyTasksController
{
    Q_OBJECT
public:
    /// The reroll confirmation is made with the engine and the dialog parent item.
    TBountyTasksController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);

    /// Takes the session; its bounty data refills the lists, its bounty
    /// point and reroll token balances notify the properties and its player
    /// data updates the highest selectable difficulty.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    int bountyTaskDifficulty() const override;
    bool isElite() const override;
    int canRerollTasks() const override;
    int rerollTokenAmount() const override;
    bool limitOfRerollTokensReached() const override;
    double bonusDamagePerCent() const override;
    double nextBonusDamagePerCent() const override;
    bool canUpgradeBonusDamage() const override;
    int bonusDamageUpgradeCost() const override;
    QString disabledDamagePercentTooltip() const override;
    bool maximumReachedBonusDamage() const override;
    double lifeLeechPerCent() const override;
    double nextLifeLeechPerCent() const override;
    bool canUpgradeLifeLeech() const override;
    int lifeLeechUpgradeCost() const override;
    QString disabledLifeLeechPercentTooltip() const override;
    bool maximumReachedBonusLifeLeech() const override;
    double bonusLootPerCent() const override;
    double nextBonusLootPerCent() const override;
    bool canUpgradeBonusLoot() const override;
    int bonusLootUpgradeCost() const override;
    QString disabledLootPercentTooltip() const override;
    bool maximumReachedBonusLoot() const override;
    double doubleBestiaryChancePerCent() const override;
    double nextDoubleBestiaryChancePerCent() const override;
    bool canUpgradeDoubleBestiaryChance() const override;
    int doubleBestiaryChanceUpgradeCost() const override;
    QString disabledBestiaryPercentTooltip() const override;
    bool maximumReachedBonusBestiary() const override;
    QObject* bountyTasks() const override;
    QObject* monsterList() const override;
    QObject* preferredSlotData() const override;
    int bountyPointsBalance() const override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;

public:
    /// Ignored above the highest difficulty the player's level allows.
    Q_INVOKABLE void setBountyTaskDifficulty(int Difficulty) override;
    /// The level a difficulty needs (adept 30, expert 150, master 400; 0
    /// otherwise).
    Q_INVOKABLE int getRequiredLevelForDifficulty(int Difficulty) override;
    Q_INVOKABLE void setElite(bool isElite) override;
    Q_INVOKABLE void rerollTasksAction() override;
    Q_INVOKABLE void acceptBountyTask(int TaskIndex) override;
    Q_INVOKABLE void claimReward() override;
    Q_INVOKABLE void upgradeBountyTalisman(int BountyTalismanIndex) override;
    Q_INVOKABLE void unlockPreferredMonsterSlot() override;
    Q_INVOKABLE void clearPreferredMonster(int slotIndex) override;
    Q_INVOKABLE void clearAvoidedMonster(int slotIndex) override;
    Q_INVOKABLE void assignPreferredMonster(int slotIndex, int RaceId) override;
    Q_INVOKABLE void assignAvoidedMonster(int slotIndex, int RaceId) override;
    Q_INVOKABLE void clearNameFilter() override;

private:
    /// The session's bounty task data (null without a session).
    const taskboard::TBountyTaskData* bountyTaskData() const;
    /// The session's task board currencies (null without a session).
    const taskboard::TTaskboardCurrenciesStorage* currencies() const;
    /// The bounty data's talisman level of one talisman (0 damage, 1 life
    /// leech, 2 loot, 3 bestiary), 0 without data.
    int talismanLevel(int talismanIndex) const;
    bool talismanUpgradable(int talismanIndex) const;
    int talismanUpgradeCost(int talismanIndex) const;
    /// The tooltip of a talisman that cannot be upgraded: its maximum level,
    /// or too few bounty points for the upgrade.
    QString disabledTalismanTooltip(int talismanIndex) const;
    /// Applies one bounty board message: the lists, the selection, and the
    /// answer to a pending request. While no task is taken every task shows
    /// the board's elite mode.
    void onBountyTaskDataReceived(const taskboard::TBountyTaskData& data);
    /// The highest difficulty the player's level allows (level 30 adept,
    /// 150 expert, 400 master); 0 without a session or player data.
    void updateMaxBountyTaskDifficulty();
    /// Sends a bounty request when a session is connected and no other
    /// request waits for its answer (the next bounty data).
    void sendRequest(const std::function<void()>& send);
    /// Sends the difficulty and elite mode unless they are the selection.
    void selectDifficulty(int difficulty, bool elite);

    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    taskboard::TBountyTasksListModel* m_tasksModel = nullptr;
    taskboard::TMonsterListModelSortFilterProxy* m_monsterFilter = nullptr;
    taskboard::TBountyTasksPreferredMonsterListModel* m_preferredModel = nullptr;
    /// A request was sent and the bounty data has not answered it yet.
    bool m_requestPending = false;
    /// The difficulty and elite mode last selected or received.
    int m_selectedDifficulty = 0;
    bool m_selectedElite = false;
    bool m_hasSelection = false;
    /// Asks before a reroll replaces a taken task; Yes sends the reroll.
    TMessageDialogController m_rerollConfirmation;
};

}
}
