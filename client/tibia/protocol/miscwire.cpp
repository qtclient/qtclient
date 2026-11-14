#include "protocol/miscwire.h"

#include <protocol.pb.h>

#include <string>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

/// Resource types whose balance is a 4-byte value (mask 0xf0400004000000f from type 30).
bool smallResource(const uint8_t type)
{
    const unsigned offset = static_cast<unsigned>(type) - 0x1e;
    return offset < 0x3c && ((0xf0400004000000fULL >> offset) & 1) != 0;
}

bool readBool(ClassicReader& r, bool& out)
{
    uint8_t v = 0;
    if (!r.u8(v))
        return false;
    out = v != 0;
    return true;
}

/// One house of the current house data: the fields follow the house state.
bool readHouse(ClassicReader& r, ProtocolHouse* h)
{
    uint32_t id = 0;
    bool rentable = false;
    uint8_t state = 0;
    if (!r.u32(id) || !readBool(r, rentable) || !r.u8(state))
        return false;
    h->set_house_id(id);
    h->set_rentable(rentable);
    h->set_state(state);
    if (state >= 2 && state <= 4) { // rented, transfer, move-out
        uint32_t paidUntil = 0;
        bool isOwner = false;
        if (!r.string(*h->mutable_owner_name()) || !r.u32(paidUntil) || !readBool(r, isOwner))
            return false;
        h->set_paid_until(paidUntil);
        h->set_is_owner(isOwner);
        if (isOwner) {
            uint8_t moveOut = 0, transfer = 0;
            if (!r.u8(moveOut) || !r.u8(transfer))
                return false;
            h->set_move_out_disable_reason(moveOut);
            h->set_transfer_disable_reason(transfer);
        }
        if (state == 4) {
            uint32_t date = 0;
            if (!r.u32(date))
                return false;
            h->set_move_out_date(date);
            if (isOwner) {
                uint8_t cancel = 0;
                if (!r.u8(cancel))
                    return false;
                h->set_cancel_move_out_disable_reason(cancel);
            }
        } else if (state == 3) {
            uint32_t date = 0;
            bool accepted = false, isNewOwner = false;
            uint64_t price = 0;
            if (!r.u32(date) || !r.string(*h->mutable_new_owner_name()) || !readBool(r, accepted) || !r.u64(price)
                || !readBool(r, isNewOwner))
                return false;
            h->set_transfer_date(date);
            h->set_new_owner_accepted(accepted);
            h->set_transfer_price(price);
            h->set_is_new_owner(isNewOwner);
            if (isNewOwner) {
                uint8_t accept = 0, reject = 0;
                if (!r.u8(accept) || !r.u8(reject))
                    return false;
                h->set_accept_transfer_disable_reason(accept);
                h->set_reject_transfer_disable_reason(reject);
            }
            if (isOwner) {
                uint8_t cancel = 0;
                if (!r.u8(cancel))
                    return false;
                h->set_cancel_transfer_disable_reason(cancel);
            }
        }
        return true;
    }
    if (state != 0) // only an available house says more
        return true;
    bool isBidder = false;
    uint8_t reason = 0;
    if (!r.string(*h->mutable_bidder_name()) || !readBool(r, isBidder) || !r.u8(reason))
        return false;
    h->set_is_bidder(isBidder);
    h->set_bid_disable_reason(reason);
    if (!h->bidder_name().empty()) {
        uint32_t end = 0;
        uint64_t highest = 0;
        if (!r.u32(end) || !r.u64(highest))
            return false;
        h->set_bid_end(end);
        h->set_highest_bid(highest);
    }
    if (isBidder) {
        uint64_t limit = 0;
        if (!r.u64(limit))
            return false;
        h->set_bid_limit(limit);
    }
    return true;
}

}

bool isMiscMessage(const int opcode)
{
    switch (opcode) {
    case 11: // ReadyForSecondaryConnection
    case 41: // Stash
    case 119: // InspectionState
    case 136: // ExaltationHistory
    case 189: // BossTracking
    case 195: // CyclopediaHouseActionResult
    case 199: // CyclopediaCurrentHouseData
    case 211: // BuddyStatusChange
    case 224: // StoreError
    case 229: // DailyRewardHistory
    case 234: // OfferDescription
    case 254: // StoreSuccess
    case 25: // StoreButtonIndicators
    case 40: // Dead
    case 92: // WeaponProficiencyNotification
    case 152: // ShowGameNews
    case 157: // SwitchPreset
    case 175: // XpChanged
    case 184: // PvpSituations
    case 206: // ItemWasted
    case 217: // MonsterCyclopediaNewDetails
    case 222: // DailyRewardCollectionState
    case 230: // BosstiaryHighlight
    case 231: // PreyTimeLeft
    case 233: // PreyPrices
    case 238: // ResourceBalance
    case 239: // TibiaTime
    case 242: // UpdatingShopBalance
        return true;
    default:
        return false;
    }
}

size_t fillMiscMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 25: {
        auto* m = msg.MutableExtension(GameserverMessageStoreButtonIndicators::gameserver_message_store_button_indicators);
        uint8_t sale = 0, isNew = 0;
        if (!r.u8(sale) || !r.u8(isNew))
            return 0;
        m->set_sale(sale != 0);
        m->set_new_offers(isNew != 0);
        return r.i;
    }
    case 40: {
        auto* m = msg.MutableExtension(GameserverMessageDead::gameserver_message_dead);
        uint8_t type = 0, penalty = 0, redemption = 0;
        if (!r.u8(type))
            return 0;
        m->set_death_type(type);
        if (type == 0) {
            if (!r.u8(penalty))
                return 0;
            m->set_penalty(penalty);
        }
        if (!r.u8(redemption))
            return 0;
        m->set_death_redemption(redemption != 0);
        return r.i;
    }
    case 92: {
        auto* m = msg.MutableExtension(
            GameserverMessageWeaponProficiencyNotification::gameserver_message_weapon_proficiency_notification);
        uint16_t object = 0;
        uint32_t experience = 0;
        uint8_t unusedPerk = 0;
        if (!r.u16(object) || !r.u32(experience) || !r.u8(unusedPerk))
            return 0;
        m->set_object_id(object);
        m->set_experience(experience);
        m->set_unused_perk(unusedPerk != 0);
        return r.i;
    }
    case 152: {
        auto* m = msg.MutableExtension(GameserverMessageShowGameNews::gameserver_message_show_game_news);
        uint32_t type = 0;
        uint8_t notificationOnly = 0;
        if (!r.u32(type) || !r.u8(notificationOnly))
            return 0;
        m->set_news_type(type);
        m->set_notification_only(notificationOnly != 0);
        return r.i;
    }
    case 157: {
        uint32_t preset = 0;
        if (!r.u32(preset))
            return 0;
        msg.MutableExtension(GameserverMessageSwitchPreset::gameserver_message_switch_preset)->set_preset_id(preset);
        return r.i;
    }
    case 175: {
        auto* m = msg.MutableExtension(GameserverMessageXpChanged::gameserver_message_xp_changed);
        uint64_t raw = 0, final = 0;
        if (!r.u64(raw) || !r.u64(final))
            return 0;
        m->set_raw_experience(static_cast<int64_t>(raw));
        m->set_final_experience(static_cast<int64_t>(final));
        return r.i;
    }
    case 184: {
        uint8_t situations = 0;
        if (!r.u8(situations))
            return 0;
        msg.MutableExtension(GameserverMessagePvpSituations::gameserver_message_pvp_situations)->set_open_situations(situations);
        return r.i;
    }
    case 206: {
        uint16_t object = 0;
        if (!r.u16(object))
            return 0;
        msg.MutableExtension(GameserverMessageItemWasted::gameserver_message_item_wasted)->set_object_id(object);
        return r.i;
    }
    case 217: {
        uint16_t race = 0;
        if (!r.u16(race))
            return 0;
        msg.MutableExtension(GameserverMessageMonsterCyclopediaNewDetails::gameserver_message_monster_cyclopedia_new_details)
            ->set_race_id(race);
        return r.i;
    }
    case 222: {
        uint8_t state = 0;
        if (!r.u8(state))
            return 0;
        msg.MutableExtension(GameserverMessageDailyRewardCollectionState::gameserver_message_daily_reward_collection_state)
            ->set_collected(state != 0);
        return r.i;
    }
    case 230: {
        uint32_t boss = 0;
        if (!r.u32(boss))
            return 0;
        msg.MutableExtension(GameserverMessageBosstiaryHighlight::gameserver_message_bosstiary_highlight)->set_boss_id(boss);
        return r.i;
    }
    case 231: {
        auto* m = msg.MutableExtension(GameserverMessagePreyTimeLeft::gameserver_message_prey_time_left);
        uint8_t slot = 0;
        uint16_t timeLeft = 0;
        if (!r.u8(slot) || !r.u16(timeLeft))
            return 0;
        m->set_slot(slot);
        m->set_time_left(timeLeft);
        return r.i;
    }
    case 233: {
        auto* m = msg.MutableExtension(GameserverMessagePreyPrices::gameserver_message_prey_prices);
        uint32_t reroll = 0;
        uint8_t wildcard = 0, selection = 0;
        if (!r.u32(reroll) || !r.u8(wildcard) || !r.u8(selection))
            return 0;
        m->set_reroll_price(reroll);
        m->set_wildcard_price(wildcard);
        m->set_selection_price(selection);
        return r.i;
    }
    case 238: {
        auto* m = msg.MutableExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_resource_type(type);
        if (smallResource(type)) {
            uint32_t value = 0;
            if (!r.u32(value))
                return 0;
            m->set_small_value(static_cast<int32_t>(value));
        } else {
            uint64_t value = 0;
            if (!r.u64(value))
                return 0;
            m->set_value(value);
        }
        return r.i;
    }
    case 239: {
        auto* m = msg.MutableExtension(GameserverMessageTibiaTime::gameserver_message_tibia_time);
        uint8_t hour = 0, minute = 0;
        if (!r.u8(hour) || !r.u8(minute))
            return 0;
        m->set_hour(hour);
        m->set_minute(minute);
        return r.i;
    }
    case 242: {
        uint8_t updating = 0;
        if (!r.u8(updating))
            return 0;
        msg.MutableExtension(GameserverMessageUpdatingShopBalance::gameserver_message_updating_shop_balance)
            ->set_balance_final(updating == 0);
        return r.i;
    }
    case 11:
        if (!r.string(*msg.MutableExtension(GameserverMessageReadyForSecondaryConnection::gameserver_message_ready_for_secondary_connection)
                           ->mutable_token()))
            return 0;
        return r.i;
    case 41: {
        auto* m = msg.MutableExtension(GameserverMessageStash::gameserver_message_stash);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t object = 0;
            uint32_t amount = 0;
            if (!r.u16(object) || !r.u32(amount))
                return 0;
            ProtocolStashEntry* entry = m->add_entries();
            entry->set_object_id(object);
            entry->set_amount(amount);
        }
        return r.i;
    }
    case 119: {
        auto* m = msg.MutableExtension(GameserverMessageInspectionState::gameserver_message_inspection_state);
        uint32_t creature = 0;
        uint8_t state = 0;
        if (!r.u32(creature) || !r.u8(state))
            return 0;
        m->set_creature_id(creature);
        m->set_state(state);
        return r.i;
    }
    case 136: {
        auto* m = msg.MutableExtension(GameserverMessageExaltationHistory::gameserver_message_exaltation_history);
        uint16_t page = 0, lastPage = 0;
        uint8_t count = 0;
        if (!r.u16(page) || !r.u16(lastPage) || !r.u8(count))
            return 0;
        m->set_page(page);
        m->set_last_page(lastPage);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolExaltationHistoryEntry* entry = m->add_entries();
            uint32_t time = 0;
            uint8_t action = 0;
            bool bonus = false;
            if (!r.u32(time) || !r.u8(action) || !r.string(*entry->mutable_description()) || !readBool(r, bonus))
                return 0;
            entry->set_timestamp(time);
            entry->set_action_type(action);
            entry->set_bonus(bonus);
        }
        return r.i;
    }
    case 189: {
        auto* m = msg.MutableExtension(GameserverMessageBossTracking::gameserver_message_boss_tracking);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            uint32_t race = 0;
            uint64_t cooldown = 0;
            if (!r.u32(race) || !r.u64(cooldown))
                return 0;
            ProtocolBossCooldown* entry = m->add_cooldowns();
            entry->set_race_id(race);
            entry->set_cooldown(cooldown);
        }
        return r.i;
    }
    case 195: {
        auto* m = msg.MutableExtension(GameserverMessageCyclopediaHouseActionResult::gameserver_message_cyclopedia_house_action_result);
        uint32_t house = 0;
        uint8_t type = 0, error = 0;
        if (!r.u32(house) || !r.u8(type) || !r.u8(error))
            return 0;
        m->set_house_id(house);
        m->set_action_type(type);
        m->set_error_code(error);
        if (error == 0 && type == 1) {
            bool outbidden = false;
            if (!readBool(r, outbidden))
                return 0;
            m->set_outbidden(outbidden);
        }
        return r.i;
    }
    case 199: {
        auto* m = msg.MutableExtension(GameserverMessageCyclopediaCurrentHouseData::gameserver_message_cyclopedia_current_house_data);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            if (!readHouse(r, m->add_houses()))
                return 0;
        }
        return r.i;
    }
    case 211: {
        auto* m = msg.MutableExtension(GameserverMessageBuddyStatusChange::gameserver_message_buddy_status_change);
        uint32_t buddy = 0;
        uint8_t status = 0;
        if (!r.u32(buddy) || !r.u8(status))
            return 0;
        m->set_buddy_id(buddy);
        m->set_status(status);
        return r.i;
    }
    case 224:
    case 254: {
        uint8_t type = 0;
        std::string text;
        if (!r.u8(type) || !r.string(text))
            return 0;
        if (opcode == 224) {
            auto* m = msg.MutableExtension(GameserverMessageStoreError::gameserver_message_store_error);
            m->set_error_type(type);
            m->set_message(text);
        } else {
            auto* m = msg.MutableExtension(GameserverMessageStoreSuccess::gameserver_message_store_success);
            m->set_success_type(type);
            m->set_message(text);
        }
        return r.i;
    }
    case 229: {
        auto* m = msg.MutableExtension(GameserverMessageDailyRewardHistory::gameserver_message_daily_reward_history);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolDailyRewardHistoryEntry* entry = m->add_entries();
            uint32_t time = 0;
            bool premium = false;
            uint16_t streak = 0;
            if (!r.u32(time) || !readBool(r, premium) || !r.string(*entry->mutable_description()) || !r.u16(streak))
                return 0;
            entry->set_timestamp(time);
            entry->set_premium(premium);
            entry->set_streak(streak);
        }
        return r.i;
    }
    case 234: {
        auto* m = msg.MutableExtension(GameserverMessageOfferDescription::gameserver_message_offer_description);
        uint32_t offer = 0;
        if (!r.u32(offer) || !r.string(*m->mutable_description()))
            return 0;
        m->set_offer_id(offer);
        return r.i;
    }
    default:
        return 0;
    }
}

}
