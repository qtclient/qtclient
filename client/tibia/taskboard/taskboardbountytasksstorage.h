#pragma once

#include <cip_gen.h>

#include <QObject>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia {
namespace taskboard {

/// The claim state of a bounty task (the server's 0..2).
enum class EBountyTaskClaimState : int
{
    Available = 0,
    Active = 1,
    Claimable = 2,
};

/// One bounty task as the board message carries it.
struct TBountyTask
{
    uint8_t index = 0;
    uint16_t raceId = 0;
    uint16_t requiredKills = 0;
    uint32_t rewardExperience = 0;
    uint16_t rewardBountyPoints = 0;
    uint16_t currentKills = 0;
    EBountyTaskClaimState claimState = EBountyTaskClaimState::Available;
    uint8_t grade = 0;
    bool elite = false;
    uint16_t rewardDust = 0;
};

/// One of the four bounty talismans (damage, life leech, loot, bestiary).
struct TBountyTalisman
{
    uint16_t level = 0;
    bool upgradable = false;
    uint16_t upgradeCost = 0;
};

/// One preferred/avoided monster slot. The next locked slot after the unlocked
/// ones is the one that can be bought; slot n costs n * 300 bounty points.
struct TBountyPreferredMonsterSlot
{
    int index = 0;
    bool unlocked = false;
    bool nextToUnlock = false;
    int unlockCost = 0;
    uint32_t preferredRaceId = 0;
    uint32_t avoidedRaceId = 0;
};

struct TBountyTaskData
{
    std::vector<TBountyTask> tasks;
    uint8_t difficulty = 0;
    bool elite = false;
    std::array<TBountyTalisman, 4> talismans{};
    std::vector<TBountyPreferredMonsterSlot> preferredMonsterSlots;
};

/// The bounty tab's data, replaced as a whole by every bounty board message.
class TTaskboardBountyTasksStorage final : public cip::tibia::taskboard::TTaskboardBountyTasksStorage
{
    Q_OBJECT
public:
    explicit TTaskboardBountyTasksStorage(QObject* parent = nullptr);

    static TTaskboardBountyTasksStorage* instance();

    void setBountyTaskData(TBountyTaskData data);
    const TBountyTaskData& bountyTaskData() const { return m_data; }

Q_SIGNALS:
    void bountyTaskDataChanged(const tibia::taskboard::TBountyTaskData& data);

private:
    TBountyTaskData m_data;
};

}
}
