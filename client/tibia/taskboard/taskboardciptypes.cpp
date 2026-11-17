#include "taskboard/taskboardciptypes.h"

#include "protocol/protocolciptypes.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardcurrenciesstorage.h"
#include "taskboard/taskboardshopstorage.h"
#include "taskboard/taskboardweeklytasksstorage.h"

#include <client/const.h>

#include <protocol.pb.h>

#include <QCoreApplication>

#include <stdexcept>

namespace tibia::taskboard {

namespace {

using namespace ::tibia::protobuf::protocol;

/// The protocol enums of the board are plain ranges starting at 0; anything
/// above `maxValue` is rejected.
int checkedEnumValue(const uint32_t value, const uint32_t maxValue)
{
    if (value > maxValue)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(value);
}

void sendCommand(const GameclientMessageTaskboard& command)
{
    auto message = tibia::protocol::newGameclientMessage(95);
    *message->MutableExtension(GameclientMessageTaskboard::gameclient_message_taskboard) = command;
    tibia::protocol::sendGameclientMessage(std::move(message));
}

void sendOption(const int option)
{
    GameclientMessageTaskboard command;
    command.set_option(static_cast<uint32_t>(option));
    sendCommand(command);
}

TShopRewardData shopReward(const ProtocolTaskboardOffer& offer)
{
    TShopRewardData reward;
    reward.price = offer.price();
    reward.type = static_cast<EShopRewardType>(checkedEnumValue(offer.offer_type(), 4));
    if (reward.type != EShopRewardType::PromotionPoint) {
        reward.name = QString::fromStdString(offer.name());
        reward.description = QString::fromStdString(offer.description());
        reward.appearanceId = offer.appearance_id();
        if (reward.type == EShopRewardType::Outfit) {
            reward.firstAddon = offer.addons() == 1;
            reward.secondAddon = offer.addons() == 2;
        } else if (reward.type == EShopRewardType::TwoObjects) {
            reward.secondObjectTypeId = offer.second_object_id();
        }
    }
    reward.status = static_cast<EShopRewardStatus>(checkedEnumValue(offer.status(), 4));
    if (reward.type == EShopRewardType::PromotionPoint) {
        reward.purchaseCount = static_cast<uint16_t>(offer.purchased());
        reward.name = qtTrId("taskboard_shop_offer_promotionpoint");
        // The count of the next purchase; a bought offer shows the count it reached.
        const qulonglong alreadyPurchased = static_cast<qulonglong>(reward.purchaseCount)
            - (reward.status != EShopRewardStatus::Bought ? 1u : 0u);
        reward.description = qtTrId("taskboard_shop_offer_promotionpoint_description").arg(alreadyPurchased);
    }
    return reward;
}

TWeeklyTaskData weeklyTaskData(const ProtocolTaskboardWeekly& weekly)
{
    TWeeklyTaskData data;
    data.anyCreatureRequiredKills = static_cast<uint16_t>(weekly.any_creature_total());
    data.anyCreatureCurrentKills = static_cast<uint16_t>(weekly.any_creature_current());
    for (const auto& kill : weekly.kill_tasks())
        data.killTasks.push_back({ static_cast<uint16_t>(kill.race_id()), static_cast<uint16_t>(kill.total()),
                                   static_cast<uint16_t>(kill.current()) });
    for (const auto& delivery : weekly.delivery_tasks()) {
        TWeeklyDeliveryTask task;
        task.slotIndex = static_cast<uint8_t>(delivery.slot_index());
        task.objectTypeId = static_cast<uint16_t>(delivery.object_id());
        task.requiredAmount = delivery.required();
        task.availableAmount = delivery.available();
        task.state = checkedEnumValue(delivery.delivered(), 1);
        data.deliveryTasks.push_back(task);
    }
    data.difficulty = static_cast<uint8_t>(checkedEnumValue(weekly.difficulty(), 3));
    data.rewardExperiencePerKillTask = weekly.kill_reward_xp();
    data.rewardExperiencePerDeliveryTask = weekly.delivery_reward_xp();
    data.completedKillTasks = weekly.completed_kill_tasks();
    data.completedDeliveryTasks = weekly.completed_delivery_tasks();
    data.progressFinished = checkedEnumValue(weekly.progress_finished(), 1);
    data.maxUnlockedDifficulty = static_cast<uint8_t>(checkedEnumValue(weekly.unlocked_difficulty(), 3));
    data.resetTime = weekly.reset_time();
    data.permanentExpansion = weekly.expansion();
    data.rewardHuntingTaskPoints = weekly.reward_hunting_task_points();
    data.rewardSoulSeals = weekly.reward_soul_seals();
    return data;
}

TBountyTaskData bountyTaskData(const ProtocolTaskboardBounty& bounty)
{
    TBountyTaskData data;
    for (const auto& t : bounty.tasks()) {
        TBountyTask task;
        task.index = static_cast<uint8_t>(t.task_index());
        task.raceId = static_cast<uint16_t>(t.race_id());
        task.requiredKills = static_cast<uint16_t>(t.required_kills());
        task.rewardExperience = t.reward_xp();
        task.rewardBountyPoints = static_cast<uint16_t>(t.reward_bounty_points());
        task.currentKills = static_cast<uint16_t>(t.current_kills());
        task.claimState = static_cast<EBountyTaskClaimState>(checkedEnumValue(t.claim_state(), 2));
        task.grade = static_cast<uint8_t>(checkedEnumValue(t.grade(), 3));
        task.elite = t.elite();
        task.rewardDust = static_cast<uint16_t>(t.reward_dust());
        data.tasks.push_back(task);
    }
    data.difficulty = static_cast<uint8_t>(checkedEnumValue(bounty.difficulty(), 3));
    data.elite = bounty.elite_enabled();
    for (int i = 0; i < 4; ++i) {
        const ProtocolBountyTalisman talisman = i < bounty.talismans_size() ? bounty.talismans(i) : ProtocolBountyTalisman{};
        data.talismans[static_cast<size_t>(i)] = { static_cast<uint16_t>(talisman.level()), talisman.upgradable(),
                                                   static_cast<uint16_t>(talisman.upgrade_cost()) };
    }
    int unlockedSlots = 0;
    for (int i = 0; i < bounty.preferred_slots_size(); ++i) {
        const auto& slot = bounty.preferred_slots(i);
        TBountyPreferredMonsterSlot row;
        row.index = i;
        row.unlocked = slot.unlocked();
        unlockedSlots += row.unlocked ? 1 : 0;
        row.nextToUnlock = i == unlockedSlots;
        row.unlockCost = static_cast<unsigned>(i - 1) < 4u ? (i - 1) * 300 + 300 : 0;
        row.preferredRaceId = static_cast<uint16_t>(slot.preferred_race_id());
        row.avoidedRaceId = static_cast<uint16_t>(slot.avoided_race_id());
        data.preferredMonsterSlots.push_back(row);
    }
    return data;
}

} // namespace

TTaskboardProtocolMessageHandler::TTaskboardProtocolMessageHandler(std::shared_ptr<TTaskboardCurrenciesStorage> currencies,
                                                                   std::shared_ptr<TTaskboardWeeklyTasksStorage> weeklyTasks,
                                                                   std::shared_ptr<TTaskboardBountyTasksStorage> bountyTasks,
                                                                   std::shared_ptr<TTaskboardShopStorage> shop)
    : cip::tibia::taskboard::TTaskboardProtocolMessageHandler(nullptr)
    , m_currencies(std::move(currencies))
    , m_weeklyTasks(std::move(weeklyTasks))
    , m_bountyTasks(std::move(bountyTasks))
    , m_shop(std::move(shop))
{}

TTaskboardProtocolMessageHandler* TTaskboardProtocolMessageHandler::instance()
{
    static TTaskboardProtocolMessageHandler* s_instance = [] {
        auto* handler = new TTaskboardProtocolMessageHandler(TTaskboardCurrenciesStorage::shared(), TTaskboardWeeklyTasksStorage::shared(),
                                                             TTaskboardBountyTasksStorage::shared(), TTaskboardShopStorage::shared());
        handler->setParent(QCoreApplication::instance());
        return handler;
    }();
    return s_instance;
}

void TTaskboardProtocolMessageHandler::handleTaskboardMessage(const ::tibia::protobuf::protocol::GameserverMessageTaskboard& TaskboardMessage)
{
    const int boardType = checkedEnumValue(TaskboardMessage.board_type(), 2);
    if (boardType == 2) {
        std::vector<TShopRewardData> rewards;
        for (const auto& offer : TaskboardMessage.offers())
            rewards.push_back(shopReward(offer));
        m_shop->setShopRewardData(rewards);
    } else if (boardType == 1) {
        m_weeklyTasks->setWeeklyTaskData(weeklyTaskData(TaskboardMessage.weekly()));
    } else {
        m_bountyTasks->setBountyTaskData(bountyTaskData(TaskboardMessage.bounty()));
    }
}

void TTaskboardProtocolMessageHandler::handleResourceBalanceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& ResourceBalanceMessage)
{
    TTaskboardCurrenciesStorage* const currencies = m_currencies.get();
    switch (ResourceBalanceMessage.resource_type()) {
    case tibia::client::RESOURCE_TASK_HUNTING:
        currencies->setPreyHuntingTaskTokensAmount(static_cast<int>(ResourceBalanceMessage.value()));
        break;
    case tibia::client::RESOURCE_BOUNTY_POINTS:
        currencies->setBountyPoints(ResourceBalanceMessage.small_value());
        break;
    case tibia::client::RESOURCE_SOULSEALS:
        currencies->setSoulSealsAmount(ResourceBalanceMessage.small_value());
        break;
    case tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS:
        currencies->setBountyTaskRerollTokens(ResourceBalanceMessage.small_value());
        break;
    default:
        break;
    }
}

void TTaskboardProtocolMessageHandler::handleSoulSealsDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSoulSealsDialog& SoulSealsMessage)
{
    std::vector<uint16_t> raceIds;
    raceIds.reserve(static_cast<size_t>(SoulSealsMessage.race_ids_size()));
    for (const uint32_t raceId : SoulSealsMessage.race_ids())
        raceIds.push_back(static_cast<uint16_t>(raceId));
    Q_EMIT openSoulSealsDialog(raceIds);
}

void TTaskboardProtocolMessageHandler::requestBountyTasks() { sendOption(0); }

void TTaskboardProtocolMessageHandler::requestWeeklyTasks() { sendOption(1); }

void TTaskboardProtocolMessageHandler::requestShopRewards() { sendOption(10); }

void TTaskboardProtocolMessageHandler::setBountyTaskDifficulty(const int difficulty, const bool elite)
{
    GameclientMessageTaskboard command;
    command.set_option(2);
    command.set_bounty_difficulty(static_cast<uint32_t>(checkedEnumValue(static_cast<uint8_t>(difficulty), 3)));
    command.set_elite(elite);
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::rerollBountyTasks() { sendOption(3); }

void TTaskboardProtocolMessageHandler::acceptBountyTask(const int taskIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(5);
    command.set_task_index(static_cast<uint32_t>(taskIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::claimBountyReward() { sendOption(6); }

void TTaskboardProtocolMessageHandler::upgradeBountyTalisman(const int talismanIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(7);
    command.set_talisman_index(static_cast<uint32_t>(talismanIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::deliverWeeklyTask(const int taskIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(8);
    command.set_delivery_task_index(static_cast<uint32_t>(taskIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::selectWeeklyTaskDifficulty(const int difficulty)
{
    GameclientMessageTaskboard command;
    command.set_option(9);
    command.set_weekly_difficulty(static_cast<uint32_t>(checkedEnumValue(static_cast<uint8_t>(difficulty), 3)));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::buyShopReward(const int offerIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(11);
    command.set_offer_index(static_cast<uint32_t>(offerIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::unlockPreferredMonsterSlot() { sendOption(12); }

void TTaskboardProtocolMessageHandler::clearPreferredMonster(const int slotIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(13);
    command.set_slot_index(static_cast<uint32_t>(slotIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::clearAvoidedMonster(const int slotIndex)
{
    GameclientMessageTaskboard command;
    command.set_option(14);
    command.set_slot_index(static_cast<uint32_t>(slotIndex));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::assignPreferredMonster(const int slotIndex, const int raceId)
{
    GameclientMessageTaskboard command;
    command.set_option(15);
    command.set_slot_index(static_cast<uint32_t>(slotIndex));
    command.set_race_id(static_cast<uint32_t>(raceId));
    sendCommand(command);
}

void TTaskboardProtocolMessageHandler::assignAvoidedMonster(const int slotIndex, const int raceId)
{
    GameclientMessageTaskboard command;
    command.set_option(16);
    command.set_slot_index(static_cast<uint32_t>(slotIndex));
    command.set_race_id(static_cast<uint32_t>(raceId));
    sendCommand(command);
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::taskboard
