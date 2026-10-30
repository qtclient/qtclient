#pragma once

#include "client/gamecore.h"
#include "gamewindow/taskboardmodels.h"

#include <cip_gen.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tibia::gamewindow {

class TBountyTasksController final : public cip::tibia::gamewindow::TBountyTasksController
{
    Q_OBJECT
public:
    explicit TBountyTasksController(QObject* parent = nullptr);

    void applyBountyData(const std::map<std::string, uint32_t>& header,
                         const std::vector<std::map<std::string, uint32_t>>& monsters,
                         const std::vector<std::map<std::string, uint32_t>>& talismans);
    /// Preferred / avoided monster slots + race catalogue for the CipSoft filter list.
    void applyPreferredData(const std::vector<std::map<std::string, uint32_t>>& preferredSlots,
                            const std::vector<uint16_t>& raceIds);
    void refreshBountyPoints();

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;

public:
    Q_INVOKABLE void setBountyTaskDifficulty(int Difficulty) override;
    Q_INVOKABLE void rerollTasksAction() override;
    Q_INVOKABLE void claimRerollTokenAction() override;
    Q_INVOKABLE void acceptBountyTask(int TaskIndex) override;
    Q_INVOKABLE void claimReward() override;
    Q_INVOKABLE void upgradeBountyTalisman(int BountyTalismanIndex) override;
    Q_INVOKABLE void unlockPreferredMonsterSlot() override;
    Q_INVOKABLE void clearPreferredMonster(int slotIndex) override;
    Q_INVOKABLE void clearAvoidedMonster(int slotIndex) override;
    Q_INVOKABLE void assignPreferredMonster(int slotIndex, int RaceId) override;
    Q_INVOKABLE void assignAvoidedMonster(int slotIndex, int RaceId) override;
    Q_INVOKABLE void clearNameFilter() override;
    Q_INVOKABLE bool isAtRerollTokenLimit() override;

private:
    void applyTalismans(const std::vector<std::map<std::string, uint32_t>>& talismans);
    void notifyChanged();

    taskboard::TBountyTasksListModel* m_tasksModel = nullptr;
    taskboard::TBountyTasksPreferredMonsterListModel* m_preferredModel = nullptr;
    taskboard::TMonsterListFilterModel* m_monsterSource = nullptr;
    taskboard::TMonsterListModelSortFilterProxy* m_monsterFilter = nullptr;
    int m_rerollPoints = 0;
};

}
