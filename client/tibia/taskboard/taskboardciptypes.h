#pragma once

#include <cip_gen.h>

#include <QObject>

#include <cstdint>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageResourceBalance;
class GameserverMessageSoulSealsDialog;
class GameserverMessageTaskboard;
}

namespace tibia {
namespace taskboard {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// Fills the task board storages from the board, resource balance and soul
/// seal messages, and builds the task board commands.
class TTaskboardProtocolMessageHandler final : public cip::tibia::taskboard::TTaskboardProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::taskboard::TTaskboardProtocolMessageHandler::TTaskboardProtocolMessageHandler;

    static TTaskboardProtocolMessageHandler* instance();

    /// Board type 2 replaces the shop offers, 1 the weekly tasks, 0 the bounty tasks.
    static void handleTaskboardMessage(const ::tibia::protobuf::protocol::GameserverMessageTaskboard& TaskboardMessage);
    static void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& ResourceBalanceMessage);
    static void handleSoulSealsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageSoulSealsDialog& SoulSealsMessage);

    static void requestBountyTasks();
    static void requestWeeklyTasks();
    static void requestShopRewards();
    /// The difficulty 0..3 and the elite flag travel together.
    static void setBountyTaskDifficulty(int difficulty, bool elite);
    static void rerollBountyTasks();
    static void acceptBountyTask(int taskIndex);
    static void claimBountyReward();
    static void upgradeBountyTalisman(int talismanIndex);
    static void deliverWeeklyTask(int taskIndex);
    static void selectWeeklyTaskDifficulty(int difficulty);
    static void buyShopReward(int offerIndex);
    static void unlockPreferredMonsterSlot();
    static void clearPreferredMonster(int slotIndex);
    static void clearAvoidedMonster(int slotIndex);
    static void assignPreferredMonster(int slotIndex, int raceId);
    static void assignAvoidedMonster(int slotIndex, int raceId);

Q_SIGNALS:
    void openSoulSealsDialog(const std::vector<uint16_t>& AnimusMasteryRaceIds);
};

}
}
