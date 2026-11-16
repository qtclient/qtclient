#include "taskboard/taskboardciptypes.h"

#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

#include <algorithm>
#include <ctime>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace tibia::taskboard {

namespace {
    constexpr uint8_t TASK_BOARD_TALISMAN_PATHS = 4;
    constexpr uint8_t TASK_BOARD_WEEKLY_BASE_SLOTS = 6;
    constexpr uint8_t TASK_BOARD_WEEKLY_EXPANDED_SLOTS = 9;
    constexpr uint32_t TASK_BOARD_SECONDS_PER_DAY = 24 * 60 * 60;

    using TaskBoardRaceIdSet = std::unordered_set<uint16_t>;

    uint8_t getTaskBoardTalismanMaxLevel(const uint8_t pathIndex)
    {
        switch (pathIndex) {
            case 0:
            case 1:
            case 2:
                return 166;
            case 3:
                return 180;
            default:
                return 166;
        }
    }

    uint16_t getTaskBoardTalismanBonusHundredths(const uint8_t level, const uint8_t pathIndex)
    {
        if (level == 0) {
            return 0;
        }

        switch (pathIndex) {
            case 0:
            case 1:
            case 2: {
                if (level <= 26) {
                    return static_cast<uint16_t>(250 + (static_cast<uint32_t>(level) - 1U) * 50U);
                }
                return static_cast<uint16_t>(std::min<uint32_t>(1500U + (static_cast<uint32_t>(level) - 26U) * 25U, 5000U));
            }
            case 3: {
                if (level <= 20) {
                    return static_cast<uint16_t>(static_cast<uint32_t>(level) * 100U);
                }
                return static_cast<uint16_t>(std::min<uint32_t>(2000U + (static_cast<uint32_t>(level) - 20U) * 50U, 10000U));
            }
            default:
                return 0;
        }
    }

    uint8_t getRemainingDaysUntil(const uint32_t unixTimestamp)
    {
        if (unixTimestamp == 0) {
            return 0;
        }

        const auto now = static_cast<uint32_t>(std::time(nullptr));
        if (unixTimestamp <= now) {
            return 0;
        }

        const auto remainingSeconds = unixTimestamp - now;
        const auto rawDays = (remainingSeconds + TASK_BOARD_SECONDS_PER_DAY - 1) / TASK_BOARD_SECONDS_PER_DAY;
        return static_cast<uint8_t>(std::min<uint32_t>(rawDays, 255));
    }

    std::vector<uint16_t> getAllMonsterRaceIds()
    {
        const auto& races = tibia::game::getAllRaces();
        std::vector<uint16_t> raceIds;
        raceIds.reserve(races.size());

        for (const auto& race : races) {
            // race.raceId is uint32_t; bound to uint16_t for the preferred-slot packet.
            if (race.boss || race.raceId == 0 || race.raceId > std::numeric_limits<uint16_t>::max()) {
                continue;
            }
            raceIds.emplace_back(static_cast<uint16_t>(race.raceId));
        }

        std::sort(raceIds.begin(), raceIds.end());
        raceIds.erase(std::unique(raceIds.begin(), raceIds.end()), raceIds.end());
        return raceIds;
    }

    constexpr uint16_t getSoulsealPointsForCategory(const uint32_t category) noexcept
    {
        // (category + 1) * 10 — max category that fits uint16_t is 6552; beyond that, clamp.
        const uint32_t points = (category + 1U) * 10U;
        return static_cast<uint16_t>(std::min<uint32_t>(points, std::numeric_limits<uint16_t>::max()));
    }

    constexpr uint8_t toSoulsealCategoryIndex(const RaceType& raceData) noexcept
    {
        return raceData.hasCategory
            ? static_cast<uint8_t>(std::min<uint32_t>(raceData.category, std::numeric_limits<uint8_t>::max()))
            : 0;
    }

    constexpr bool isTaskBoardSoulsealRace(const RaceType& raceData) noexcept
    {
        return raceData.hasCategory && !raceData.boss
            && raceData.raceId != 0
            && raceData.raceId <= std::numeric_limits<uint16_t>::max();
    }

    TaskBoardSoulsealEntryData makeTaskBoardSoulsealEntry(const uint16_t raceId, const RaceType& raceData, const bool done)
    {
        TaskBoardSoulsealEntryData entry;
        entry.raceId = raceId;
        entry.category = toSoulsealCategoryIndex(raceData);
        entry.soulsealPoints = raceData.hasCategory ? getSoulsealPointsForCategory(raceData.category) : 0;
        entry.done = done ? 1 : 0;
        entry.outfit = raceData.outfit;
        if (raceData.name.empty()) {
            entry.name = std::to_string(raceId);
        } else {
            entry.name = raceData.name;
        }
        return entry;
    }

    TaskBoardRaceIdSet masteredRaceIdSet(const google::protobuf::RepeatedField<uint32_t>& list)
    {
        TaskBoardRaceIdSet masteredRaceIds;
        masteredRaceIds.reserve(static_cast<size_t>(list.size()));
        for (const uint32_t raceId : list) {
            if (raceId != 0)
                masteredRaceIds.insert(static_cast<uint16_t>(raceId));
        }
        return masteredRaceIds;
    }

    std::vector<TaskBoardSoulsealEntryData> buildTaskBoardSoulsealEntries(TaskBoardRaceIdSet&& masteredRaceIds)
    {
        const auto& races = tibia::game::getAllRaces();
        std::vector<TaskBoardSoulsealEntryData> entries;
        entries.reserve(races.size());

        for (const auto& raceData : races) {
            if (!isTaskBoardSoulsealRace(raceData)) {
                continue;
            }

            const auto raceId = static_cast<uint16_t>(raceData.raceId);
            const bool done = masteredRaceIds.erase(raceId) > 0;
            entries.emplace_back(makeTaskBoardSoulsealEntry(raceId, raceData, done));
        }

        for (const auto raceId : masteredRaceIds) {
            entries.emplace_back(makeTaskBoardSoulsealEntry(raceId, tibia::game::raceData(raceId), true));
        }

        return entries;
    }

    std::map<std::string, uint32_t> toBountyHeaderMap(const TaskBoardBountyHeaderData& header)
    {
        return {
            { "rerollPoints", header.rerollPoints },
            { "claimDaily", header.claimDaily },
            { "difficulty", header.difficulty }
        };
    }

    std::map<std::string, uint32_t> toBountyMonsterMap(const TaskBoardBountyMonsterData& monster)
    {
        return {
            { "taskIndex", monster.taskIndex },
            { "raceId", monster.raceId },
            { "currentKills", monster.currentKills },
            { "totalKills", monster.totalKills },
            { "rewardXp", monster.rewardXp },
            { "rewardPoints", monster.rewardPoints },
            { "rewardReroll", monster.rewardReroll },
            { "rewardDust", monster.rewardDust },
            { "rarity", monster.rarity },
            { "isActive", monster.isActive },
            { "isCompleted", monster.isCompleted }
        };
    }

    std::map<std::string, uint32_t> toTalismanMap(const TaskBoardTalismanData& talisman)
    {
        return {
            { "currentValue", talisman.currentValue },
            { "nextValue", talisman.nextValue },
            { "upgradeCost", talisman.upgradeCost },
            { "isActiveUpgrade", talisman.isActiveUpgrade }
        };
    }

    std::map<std::string, uint32_t> toPreferredSlotMap(const TaskBoardPreferredSlotData& slot)
    {
        return {
            { "slot", slot.slot },
            { "locked", slot.locked },
            { "preferred", slot.preferred },
            { "unwanted", slot.unwanted },
            { "price", slot.price }
        };
    }

    std::map<std::string, uint32_t> toWeeklyHeaderMap(const TaskBoardWeeklyHeaderData& header)
    {
        return {
            { "difficulty", header.difficulty },
            { "currentPlayerLevel", header.currentPlayerLevel },
            { "remainingDays", header.remainingDays },
            { "totalTaskSlots", header.totalTaskSlots },
            { "maxExperience", header.maxExperience },
            { "maxDeliveryExperience", header.maxDeliveryExperience },
            { "completedKillTasks", header.completedKillTasks },
            { "completedDeliveryTasks", header.completedDeliveryTasks },
            { "pointsEarned", header.pointsEarned },
            { "soulsealsEarned", header.soulsealsEarned },
            { "extraSlot", header.extraSlot }
        };
    }

    std::map<std::string, uint32_t> toWeeklyMonsterMap(const TaskBoardWeeklyMonsterData& monster)
    {
        return {
            { "raceId", monster.raceId },
            { "current", monster.current },
            { "total", monster.total },
            { "state", monster.state }
        };
    }

    std::map<std::string, uint32_t> toWeeklyItemMap(const TaskBoardWeeklyItemData& item)
    {
        return {
            { "slotIndex", item.slotIndex },
            { "itemId", item.itemId },
            { "current", item.current },
            { "total", item.total },
            { "claimed", item.claimed },
            { "state", item.state }
        };
    }

    std::map<std::string, std::string> toShopItemMap(const TaskBoardShopItemData& item)
    {
        return {
            { "id", std::to_string(item.id) },
            { "offerType", std::to_string(item.offerType) },
            { "title", item.title },
            { "description", item.description },
            { "price", std::to_string(item.price) },
            { "bought", std::to_string(item.bought) },
            { "lookType", std::to_string(item.lookType) },
            { "lookAddons", std::to_string(item.lookAddons) },
            { "itemId", std::to_string(item.itemId) },
            { "optionalItemId", std::to_string(item.optionalItemId) },
            { "maxPurchases", std::to_string(item.maxPurchases) },
            { "currentPurchases", std::to_string(item.currentPurchases) },
            { "nextCost", std::to_string(item.nextCost) }
        };
    }

using namespace ::tibia::protobuf::protocol;

void applyBounty(const ProtocolTaskboardBounty& b)
{
    TaskBoardBountyHeaderData headerData;
    std::vector<std::map<std::string, uint32_t>> monsterData;
    for (const auto& t : b.tasks()) {
        TaskBoardBountyMonsterData monster;
        monster.taskIndex = t.task_index();
        monster.raceId = t.race_id();
        monster.totalKills = t.required_kills();
        monster.rewardXp = t.reward_xp();
        monster.rewardPoints = t.reward_bounty_points();
        monster.currentKills = t.current_kills();
        // 0 available, 1 active, 2 claimable, 3 claimed
        monster.isActive = (t.claim_state() == 1 || t.claim_state() == 2) ? 1 : 0;
        monster.isCompleted = t.claim_state() == 3 ? 1 : 0;
        monster.rarity = std::min<uint32_t>(t.grade(), 3);
        monster.rewardDust = t.reward_dust();
        monster.rewardReroll = 1;
        auto map = toBountyMonsterMap(monster);
        map["isElite"] = t.elite() ? 1 : 0;
        monsterData.emplace_back(std::move(map));
    }
    // CIP: the reroll tokens are a resource balance; no daily claim on the board.
    headerData.rerollPoints = static_cast<uint32_t>(tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS));
    headerData.claimDaily = 0;
    headerData.difficulty = std::clamp<uint32_t>(b.difficulty() + 1, 1, 4);
    auto header = toBountyHeaderMap(headerData);
    header["eliteEnabled"] = b.elite_enabled() ? 1 : 0;

    std::vector<std::map<std::string, uint32_t>> talismanData;
    for (int i = 0; i < b.talismans_size(); ++i) {
        const auto& t = b.talismans(i);
        const auto path = static_cast<uint8_t>(i);
        const auto level = static_cast<uint8_t>(std::min<uint32_t>(t.level(), 255));
        TaskBoardTalismanData talisman;
        talisman.isActiveUpgrade = t.upgradable() ? 1 : 0;
        talisman.upgradeCost = t.upgrade_cost();
        talisman.currentValue = getTaskBoardTalismanBonusHundredths(level, path);
        talisman.nextValue = level >= getTaskBoardTalismanMaxLevel(path) || talisman.upgradeCost == 0
            ? 0
            : getTaskBoardTalismanBonusHundredths(static_cast<uint8_t>(level + 1), path);
        talismanData.emplace_back(toTalismanMap(talisman));
    }
    tibia::game::notify("onBountyTaskData", header, monsterData, talismanData);

    std::vector<std::map<std::string, uint32_t>> preferredSlotData;
    for (int i = 0; i < b.preferred_slots_size(); ++i) {
        const auto& p = b.preferred_slots(i);
        TaskBoardPreferredSlotData slot;
        slot.slot = static_cast<uint32_t>(i + 1);
        slot.locked = p.unlocked() ? 0 : 1;
        slot.preferred = p.preferred_race_id();
        slot.unwanted = p.avoided_race_id();
        slot.price = 0;
        preferredSlotData.emplace_back(toPreferredSlotMap(slot));
    }
    tibia::game::notify("onBountyPreferredData", preferredSlotData, 0, getAllMonsterRaceIds());
}

void applyWeekly(const ProtocolTaskboardWeekly& w)
{
    TaskBoardWeeklyHeaderData headerData;
    std::vector<std::map<std::string, uint32_t>> monsterData;
    const uint32_t anyTotal = w.any_creature_total();
    const uint32_t anyCurrent = w.any_creature_current();
    if (anyTotal > 0 || anyCurrent > 0) {
        TaskBoardWeeklyMonsterData anyCreatureTask;
        anyCreatureTask.raceId = 0;
        anyCreatureTask.total = anyTotal;
        anyCreatureTask.current = anyCurrent;
        anyCreatureTask.state = (anyTotal > 0 && anyCurrent >= anyTotal) ? 1 : 0;
        monsterData.emplace_back(toWeeklyMonsterMap(anyCreatureTask));
    }
    for (const auto& k : w.kill_tasks()) {
        TaskBoardWeeklyMonsterData task;
        task.raceId = k.race_id();
        task.total = k.total();
        task.current = k.current();
        task.state = (task.total > 0 && task.current >= task.total) ? 1 : 0;
        monsterData.emplace_back(toWeeklyMonsterMap(task));
    }
    std::vector<std::map<std::string, uint32_t>> itemData;
    for (const auto& d : w.delivery_tasks()) {
        TaskBoardWeeklyItemData task;
        task.slotIndex = d.slot_index();
        task.itemId = d.object_id() & 0xffffu;
        task.total = d.required();
        task.current = d.available();
        task.claimed = d.delivered();
        task.state = (task.claimed != 0 || (task.total > 0 && task.current >= task.total)) ? 1 : 0;
        itemData.emplace_back(toWeeklyItemMap(task));
    }
    headerData.maxExperience = w.kill_reward_xp();
    headerData.maxDeliveryExperience = w.delivery_reward_xp();
    headerData.completedKillTasks = w.completed_kill_tasks();
    headerData.completedDeliveryTasks = w.completed_delivery_tasks();
    headerData.pointsEarned = w.reward_hunting_task_points();
    headerData.soulsealsEarned = w.reward_soul_seals();
    const bool hasGeneratedTasks = anyTotal > 0 || anyCurrent > 0 || w.kill_tasks_size() > 0 || w.delivery_tasks_size() > 0
        || w.progress_finished() != 0;
    headerData.difficulty = hasGeneratedTasks ? std::clamp<uint32_t>(w.difficulty() + 1, 1, 4) : 0;
    headerData.currentPlayerLevel = tibia::game::localPlayerLevel();
    headerData.remainingDays = getRemainingDaysUntil(w.reset_time());
    headerData.totalTaskSlots = w.expansion() ? TASK_BOARD_WEEKLY_EXPANDED_SLOTS : TASK_BOARD_WEEKLY_BASE_SLOTS;
    headerData.extraSlot = w.expansion() ? 1 : 0;
    auto headerMap = toWeeklyHeaderMap(headerData);
    headerMap["unlockedDifficulty"] = std::clamp<uint32_t>(w.unlocked_difficulty() + 1, 1, 4);
    headerMap["weeklyProgressFinished"] = w.progress_finished();
    tibia::game::notify("onWeeklyTaskData", headerMap, monsterData, itemData);
}

void applyShop(const google::protobuf::RepeatedPtrField<ProtocolTaskboardOffer>& offers)
{
    std::vector<std::map<std::string, std::string>> shopData;
    uint32_t index = 0;
    for (const auto& o : offers) {
        TaskBoardShopItemData item;
        item.id = index++;
        item.offerType = static_cast<uint8_t>(o.offer_type());
        if (o.offer_type() == tibia::client::TASK_BOARD_SHOP_OFFER_BONUS_PROMOTION) {
            item.nextCost = o.price();
            item.price = item.nextCost;
            item.currentPurchases = o.purchased() > 0 ? o.purchased() - 1 : 0;
            item.bought = (o.status() == tibia::client::TASK_BOARD_SHOP_STATUS_BOUGHT || item.nextCost == 0) ? 1 : 0;
        } else {
            item.title = o.name();
            item.description = o.description();
            item.price = o.price();
            if (o.offer_type() == tibia::client::TASK_BOARD_SHOP_OFFER_ITEM_DOUBLE)
                item.optionalItemId = o.second_object_id();
            if (o.offer_type() == tibia::client::TASK_BOARD_SHOP_OFFER_OUTFIT) {
                item.lookType = o.appearance_id();
                item.lookAddons = static_cast<uint8_t>(o.addons());
            } else if (o.offer_type() == tibia::client::TASK_BOARD_SHOP_OFFER_MOUNT) {
                item.lookType = o.appearance_id();
            } else {
                item.itemId = o.appearance_id();
            }
            item.bought = o.status() == tibia::client::TASK_BOARD_SHOP_STATUS_BOUGHT ? 1 : 0;
        }
        shopData.emplace_back(toShopItemMap(item));
    }
    tibia::game::notify("onTaskHuntingShopData", shopData);
}

} // namespace

void TTaskboardProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    if (opcode == 91 && shadow->HasExtension(GameserverMessageTaskboard::gameserver_message_taskboard)) {
        const auto& m = shadow->GetExtension(GameserverMessageTaskboard::gameserver_message_taskboard);
        if (m.board_type() == tibia::client::TASK_BOARD_HUNT_SHOP)
            applyShop(m.offers());
        else if (m.board_type() == tibia::client::TASK_BOARD_WEEKLY)
            applyWeekly(m.weekly());
        else
            applyBounty(m.bounty());
    } else if (opcode == 186 && shadow->HasExtension(GameserverMessageSoulSealsDialog::gameserver_message_soul_seals_dialog)) {
        const auto& m = shadow->GetExtension(GameserverMessageSoulSealsDialog::gameserver_message_soul_seals_dialog);
        tibia::game::notify("onSoulsealsData", buildTaskBoardSoulsealEntries(masteredRaceIdSet(m.race_ids())));
    }
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
