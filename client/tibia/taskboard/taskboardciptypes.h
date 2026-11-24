#pragma once

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QObject>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageResourceBalance;
class GameserverMessageSoulSealsDialog;
class GameserverMessageTaskboard;
}

namespace tibia {
namespace taskboard {

/// Fills the task board storages from the board, resource balance and soul
/// seal messages, and builds the task board commands.
class TTaskboardBountyTasksStorage;
class TTaskboardCurrenciesStorage;
class TTaskboardShopStorage;
class TTaskboardWeeklyTasksStorage;

class TTaskboardProtocolMessageHandler final : public cip::tibia::taskboard::TTaskboardProtocolMessageHandler
{
    Q_OBJECT
public:
    TTaskboardProtocolMessageHandler(std::shared_ptr<TTaskboardCurrenciesStorage> currencies,
                                     std::shared_ptr<TTaskboardWeeklyTasksStorage> weeklyTasks,
                                     std::shared_ptr<TTaskboardBountyTasksStorage> bountyTasks,
                                     std::shared_ptr<TTaskboardShopStorage> shop);

    /// Board type 2 replaces the shop offers, 1 the weekly tasks, 0 the bounty tasks.
    void handleTaskboardMessage(const ::tibia::protobuf::protocol::GameserverMessageTaskboard& TaskboardMessage);
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& ResourceBalanceMessage);
    void handleSoulSealsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageSoulSealsDialog& SoulSealsMessage);

    void requestBountyTasks();
    void requestWeeklyTasks();
    void requestShopRewards();
    /// The difficulty 0..3 and the elite flag travel together.
    void setBountyTaskDifficulty(int difficulty, bool elite);
    void rerollBountyTasks();
    void acceptBountyTask(int taskIndex);
    void claimBountyReward();
    void upgradeBountyTalisman(int talismanIndex);
    void deliverWeeklyTask(int taskIndex);
    void selectWeeklyTaskDifficulty(int difficulty);
    void buyShopReward(int offerIndex);
    void unlockPreferredMonsterSlot();
    void clearPreferredMonster(int slotIndex);
    void clearAvoidedMonster(int slotIndex);
    void assignPreferredMonster(int slotIndex, int raceId);
    void assignAvoidedMonster(int slotIndex, int raceId);

Q_SIGNALS:
    void sendTaskboardCommand(const ::tibia::protobuf::protocol::GameclientMessageTaskboard& TaskboardMessage);
    void sendFightMonsterInSoulPitCommand(const ::tibia::protobuf::protocol::GameclientMessageSoulSealsFightMonster& SoulSealsFightMonsterMessage);
    void openSoulSealsDialog(const std::vector<uint16_t>& AnimusMasteryRaceIds);

private:
    /// Emits the command (sendTaskboardCommand); the session links it to the message queue.
    void sendCommand(const ::tibia::protobuf::protocol::GameclientMessageTaskboard& command);
    void sendOption(int option);

    std::shared_ptr<TTaskboardCurrenciesStorage> m_currencies;
    std::shared_ptr<TTaskboardWeeklyTasksStorage> m_weeklyTasks;
    std::shared_ptr<TTaskboardBountyTasksStorage> m_bountyTasks;
    std::shared_ptr<TTaskboardShopStorage> m_shop;
};

}
}
