#include "protocol/featurewire.h"

#include "protocol/inventoryitemwire.h"
#include "protocol/mapwire.h"

#include <client/const.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

namespace tibia::protocol {

namespace {

using namespace ::tibia::protobuf::protocol;

bool readBool(ClassicReader& r, bool& out)
{
    uint8_t v = 0;
    if (!r.u8(v))
        return false;
    out = v != 0;
    return true;
}

/// Objects with an upgrade tier carry one more byte after their id.
bool tieredObject(const uint16_t id)
{
    const ThingType* type = tibia::game::rawThingType(id, ThingCategoryItem);
    return type && type->getClassification() > 0;
}

/// Object id, then its tier when the object is tiered.
template<typename Entry>
bool readTieredObject(ClassicReader& r, Entry* entry)
{
    uint16_t id = 0;
    if (!r.u16(id))
        return false;
    entry->set_object_id(id);
    if (tieredObject(id)) {
        uint8_t tier = 0;
        if (!r.u8(tier))
            return false;
        entry->set_tier(tier);
    }
    return true;
}

/// One to four bytes: the top two bits of the first byte select the length
/// (1, 2 or 4), the remaining bits are the most significant ones.
bool readVarInt(ClassicReader& r, uint32_t& out)
{
    uint8_t first = 0;
    if (!r.u8(first) || first >= 0xc0)
        return false;
    const unsigned length = 1u << (first >> 6);
    uint32_t value = first & 0x3f;
    for (unsigned k = 1; k < length; ++k) {
        uint8_t next = 0;
        if (!r.u8(next))
            return false;
        value = (value << 8) | next;
    }
    out = value;
    return true;
}

bool readProgress(ClassicReader& r, ProtocolUnjustifiedProgress* out)
{
    uint8_t progress = 0, remaining = 0;
    if (!r.u8(progress) || !r.u8(remaining))
        return false;
    out->set_progress(progress);
    out->set_remaining(remaining);
    return true;
}

template<typename Repeated>
bool readU16List(ClassicReader& r, Repeated* out)
{
    uint16_t count = 0;
    if (!r.u16(count))
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t v = 0;
        if (!r.u16(v))
            return false;
        out->Add(v);
    }
    return true;
}

/// CIP v190: the team's settings.
bool readTeamSettings(ClassicReader& r, ProtocolTeamSettings* out)
{
    uint16_t minLevel = 0, maxLevel = 0, slots = 0, freeSlots = 0;
    uint8_t vocations = 0, type = 0;
    uint32_t timestamp = 0;
    if (!r.u16(minLevel) || !r.u16(maxLevel) || !r.u8(vocations) || !r.u16(slots) || !r.u16(freeSlots) || !r.u32(timestamp)
        || !r.u8(type))
        return false;
    out->set_min_level(minLevel);
    out->set_max_level(maxLevel);
    out->set_vocations(vocations);
    out->set_team_slots(slots);
    out->set_free_slots(freeSlots);
    out->set_timestamp(timestamp);
    ProtocolTeamType* teamType = out->mutable_type();
    teamType->set_team_type(type);
    uint16_t a = 0, b = 0;
    if (type == 3) { // quest
        if (!r.u16(a))
            return false;
        teamType->set_quest_id(a);
    } else if (type == 2) { // hunt
        if (!r.u16(a) || !r.u16(b))
            return false;
        teamType->set_hunt_type(a);
        teamType->set_hunt_area(b);
    } else if (type == 1) { // boss
        if (!r.u16(a))
            return false;
        teamType->set_boss_id(a);
    }
    return true;
}

/// CIP v27: one bosstiary slot.
bool readBossSlot(ClassicReader& r, ProtocolBossSlot* out)
{
    bool unlocked = false;
    if (!readBool(r, unlocked))
        return false;
    out->set_unlocked(unlocked);
    uint32_t value = 0;
    if (!r.u32(value))
        return false;
    if (!unlocked) {
        out->set_locked_value(value);
        return true;
    }
    out->mutable_boss()->set_boss_id(value);
    if (value == 0)
        return true;
    uint8_t race = 0, killBonus = 0, raceRepeat = 0;
    uint32_t kills = 0, removePrice = 0;
    uint16_t lootBonus = 0;
    bool inactive = false;
    if (!r.u8(race) || !r.u32(kills) || !r.u16(lootBonus) || !r.u8(killBonus) || !r.u8(raceRepeat) || !r.u32(removePrice)
        || !readBool(r, inactive))
        return false;
    out->mutable_boss()->set_race(race);
    out->set_kill_count(kills);
    out->set_loot_bonus(lootBonus);
    out->set_kill_bonus(killBonus);
    out->set_race_repeat(raceRepeat);
    out->set_remove_price(removePrice);
    out->set_inactive(inactive);
    return true;
}

bool readTrackedMission(ClassicReader& r, ProtocolTrackedMission* out)
{
    uint16_t mission = 0, quest = 0;
    if (!r.u16(mission) || !r.u16(quest) || !r.string(*out->mutable_quest_name()) || !r.string(*out->mutable_mission_name())
        || !r.string(*out->mutable_description()))
        return false;
    out->set_mission_id(mission);
    out->set_quest_id(quest);
    return true;
}

template<typename List>
bool readTierPrices(ClassicReader& r, List* out)
{
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        ProtocolTierPrice* price = out->Add();
        uint8_t tier = 0;
        uint64_t value = 0;
        if (!r.u8(tier) || !r.u64(value))
            return false;
        price->set_tier(tier);
        price->set_price(value);
    }
    return true;
}

template<typename List>
bool readStrings16(ClassicReader& r, List* out)
{
    uint16_t count = 0;
    if (!r.u16(count))
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        if (!r.string(*out->Add()))
            return false;
    }
    return true;
}

/// CIP v149: a creature by name and outfit.
bool readPreyMonster(ClassicReader& r, ProtocolPreyMonster* out)
{
    return r.string(*out->mutable_name()) && readOutfit(r, out->mutable_outfit(), true);
}

/// CIP v153: one imbuement.
bool readImbuement(ClassicReader& r, ProtocolImbuement* out)
{
    uint32_t id = 0, duration = 0, cost = 0;
    uint8_t tier = 0, sources = 0;
    uint16_t icon = 0;
    if (!r.u32(id) || !r.string(*out->mutable_name()) || !r.string(*out->mutable_description()) || !r.u8(tier) || !r.u16(icon)
        || !r.u32(duration) || !r.u8(sources))
        return false;
    out->set_imbuement_id(id);
    out->set_tier(tier);
    out->set_icon_id(icon);
    out->set_duration(duration);
    for (uint8_t i = 0; i < sources; ++i) {
        ProtocolImbuementSource* source = out->add_sources();
        uint16_t object = 0, count = 0;
        if (!r.u16(object) || !r.string(*source->mutable_name()) || !r.u16(count))
            return false;
        source->set_object_id(object);
        source->set_count(count);
    }
    if (!r.u32(cost))
        return false;
    out->set_cost(cost);
    return true;
}

/// CIP v20: one outfit, mount or familiar the window offers.
template<typename List>
bool readOutfitOptions(ClassicReader& r, List* out, const bool addons)
{
    uint16_t count = 0;
    if (!r.u16(count))
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        ProtocolOutfitOption* option = out->Add();
        uint16_t look = 0;
        uint8_t mode = 0;
        if (!r.u16(look) || !r.string(*option->mutable_name()))
            return false;
        option->set_look_type(look);
        if (addons) {
            uint8_t value = 0;
            if (!r.u8(value))
                return false;
            option->set_addons(value);
        }
        if (!r.u8(mode))
            return false;
        option->set_mode(mode);
        if (mode == 1) { // store offer
            uint32_t offer = 0;
            if (!r.u32(offer))
                return false;
            option->set_offer_id(offer);
        }
    }
    return true;
}

template<typename List>
bool readInspectionDetails(ClassicReader& r, List* out)
{
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        ProtocolInspectionDetail* detail = out->Add();
        if (!r.string(*detail->mutable_label()) || !r.string(*detail->mutable_value()))
            return false;
    }
    return true;
}

template<typename List>
bool readForgeObjects(ClassicReader& r, List* out, const uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        ProtocolForgeObject* object = out->Add();
        uint16_t amount = 0;
        if (!readTieredObject(r, object->mutable_object()) || !r.u16(amount))
            return false;
        object->set_count(amount);
    }
    return true;
}

template<typename List>
bool readForgeTransfers(ClassicReader& r, List* out)
{
    uint8_t count = 0;
    if (!r.u8(count))
        return false;
    for (uint8_t i = 0; i < count; ++i) {
        ProtocolForgeTransfer* transfer = out->Add();
        uint16_t donors = 0, receivers = 0;
        if (!r.u16(donors) || !readForgeObjects(r, transfer->mutable_donors(), donors) || !r.u16(receivers))
            return false;
        for (uint16_t k = 0; k < receivers; ++k) {
            ProtocolForgeReceiver* receiver = transfer->add_receivers();
            uint16_t id = 0, amount = 0;
            if (!r.u16(id) || !r.u16(amount))
                return false;
            receiver->set_object_id(id);
            receiver->set_count(amount);
        }
    }
    return true;
}

bool readPosition(ClassicReader& r, ProtocolCoordinate* out)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (!r.u16(x) || !r.u16(y) || !r.u8(z))
        return false;
    out->set_x(x);
    out->set_y(y);
    out->set_z(z);
    return true;
}

template<typename List>
bool readAreaIds(ClassicReader& r, List* out)
{
    uint16_t count = 0;
    if (!r.u16(count))
        return false;
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t id = 0;
        if (!r.u16(id))
            return false;
        out->Add(id);
    }
    return true;
}

/// CIP v25: where the store opens (action, its detail, sort order, service type).
bool readStoreOpenParameters(ClassicReader& r, ProtocolStoreOpenParameters* p)
{
    uint8_t action = 0;
    if (!r.u8(action))
        return false;
    p->set_open_action(action);
    if (action == 1 || action == 3) { // premium page / useful things
        uint8_t detail = 0;
        if (!r.u8(detail))
            return false;
        p->set_detail(detail);
    } else if (action == 2) { // category
        if (!r.string(*p->mutable_category()) || !r.string(*p->mutable_sub_category()))
            return false;
    } else if (action == 4) { // offer
        uint32_t offer = 0;
        if (!r.u32(offer))
            return false;
        p->set_offer_id(offer);
    } else if (action == 5) { // search
        if (!r.string(*p->mutable_search_text()))
            return false;
    }
    uint8_t sort = 0, service = 0;
    if (!r.u8(sort) || !r.u8(service))
        return false;
    p->set_sort_order(sort);
    p->set_service_type(service);
    return true;
}

/// CIP v26: how a store offer or product is shown.
bool readStoreVisual(ClassicReader& r, ProtocolStoreVisual* v)
{
    uint8_t type = 0;
    if (!r.u8(type))
        return false;
    v->set_type(type);
    uint16_t id = 0, female = 0;
    uint8_t sex = 0, head = 0, body = 0, legs = 0, feet = 0;
    switch (type) {
    case 0: // icon
        return r.string(*v->mutable_icon());
    case 1: // mount
    case 3: // object
        if (!r.u16(id))
            return false;
        v->set_type_id(id);
        return true;
    case 2: // outfit
        if (!r.u16(id) || !r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet))
            return false;
        v->set_type_id(id);
        break;
    case 4: // hireling
        if (!r.u8(sex) || !r.u16(id) || !r.u16(female) || !r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet))
            return false;
        v->set_sex(sex);
        v->set_male_outfit(id);
        v->set_female_outfit(female);
        break;
    default:
        return true;
    }
    v->set_head(head);
    v->set_body(body);
    v->set_legs(legs);
    v->set_feet(feet);
    return true;
}

}

bool isFeatureMessage(const int opcode)
{
    switch (opcode) {
    case 46: // UnlockedSpells
    case 94: // PassiveAbilityData
    case 129: // TextForReport
    case 133: // SoundTrigger
    case 148: // DepotSearchResult
    case 151: // EditList
    case 158: // PremiumTrigger
    case 183: // UnjustifiedPoints
    case 185: // BestiaryTracker
    case 192: // UpdateManagedContainers
    case 204: // ImpactTracking
    case 205: // MarketStatistics
    case 207: // ItemLooted
    case 209: // KillTracking
    case 223: // CreditBalance
    case 237: // ShowMessageDialog
    case 245: // PlayerInventory
    case 137: // CloseExaltationDialog
    case 154: // CloseDepotSearch
    case 201: // ExivaSuppressed
    case 212: // BuddyGroupData
    case 213: // MonsterCyclopedia
    case 236: // CloseImbuingDialog
    case 240: // QuestLog
    case 244: // ObjectInfo
    case 246: // MarketEnter
    case 115: // Bosstiary
    case 138: // ExaltationResult
    case 153: // DepotSearchDetailList
    case 193: // VocationSpecificPlayerData
    case 214: // MonsterCyclopediaMonsters
    case 226: // OpenRewardWall
    case 241: // QuestLine
    case 44: // TeamFinderTeamLeader
    case 45: // TeamFinderTeamMember
    case 98: // BossSlots
    case 150: // EditText
    case 196: // WeaponProficiency
    case 208: // TrackQuestFlags
    case 253: // TransactionHistory
    case 28: // NpcTalkParters
    case 43: // PartyHuntAnalyser
    case 187: // ShapedPerkReshapeOffers
    case 198: // CyclopediaStaticHouseData
    case 155: // BlessingsDialog
    case 210: // BuddyData
    case 250: // ShowModalDialog
    case 251: // StoreCategories
    case 93: // ImbuementDurations
    case 134: // ExaltationBaseData
    case 202: // UpdateExivaOptions
    case 225: // RequestPurchaseData
    case 156: // Blessings
    case 194: // ConfigureCreaturePodium
    case 203: // TransactionDetails
    case 232: // PreyData
    case 235: // ImbuingDialogRefresh
    case 47: // BossDifficultySelection
    case 97: // BosstiaryStaticData
    case 176: // OutfitMemorial
    case 200: // Outfit
    case 215: // MonsterCyclopediaRace
    case 249: // MarketBrowse
    case 177: // Highscores
    case 168: // SetStoreButtonDeeplink
    case 3: // CreatureData
    case 118: // InspectionList
    case 128: // CharacterTradeConfiguration
    case 117: // GameEvent
    case 95: // SkillWheel
    case 135: // ExaltationDialogRefresh
    case 221: // CyclopediaMapData
    case 248: // MarketDetail
    case 252: // StoreOffers
        return true;
    default:
        return false;
    }
}

size_t fillFeatureMessage(GameserverMessage& msg, const int opcode, ClassicReader& r)
{
    switch (opcode) {
    case 46: {
        auto* m = msg.MutableExtension(GameserverMessageUnlockedSpells::gameserver_message_unlocked_spells);
        return readU16List(r, m->mutable_spell_ids()) ? r.i : 0;
    }
    case 94: {
        auto* m = msg.MutableExtension(GameserverMessagePassiveAbilityData::gameserver_message_passive_ability_data);
        uint8_t ability = 0, kind = 0;
        if (!r.u8(ability) || !r.u8(kind))
            return 0;
        m->set_ability(ability);
        m->set_kind(kind);
        if (kind == 1) {
            uint8_t first = 0, second = 0;
            if (!r.u8(first) || !r.u8(second))
                return 0;
            m->set_first_value(first);
            m->set_second_value(second);
        } else if (kind == 0) {
            uint32_t remaining = 0, total = 0;
            bool decaying = false;
            if (!r.u32(remaining) || !r.u32(total) || !readBool(r, decaying))
                return 0;
            m->set_remaining_ms(remaining);
            m->set_total_ms(total);
            m->set_decaying(decaying);
        }
        return r.i;
    }
    case 129: {
        auto* m = msg.MutableExtension(GameserverMessageTextForReport::gameserver_message_text_for_report);
        uint32_t statement = 0;
        if (!r.u32(statement) || !r.string(*m->mutable_text()) || !r.string(*m->mutable_name()))
            return 0;
        m->set_statement_id(statement);
        return r.i;
    }
    case 133: {
        auto* m = msg.MutableExtension(GameserverMessageSoundTrigger::gameserver_message_sound_trigger);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_type(type);
        if (type <= 2) { // ambience, music, anthem
            uint16_t id = 0;
            if (!r.u16(id))
                return 0;
            m->set_sound_id(id);
        }
        return r.i;
    }
    case 148: {
        auto* m = msg.MutableExtension(GameserverMessageDepotSearchResult::gameserver_message_depot_search_result);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolDepotSearchEntry* entry = m->add_entries();
            uint16_t amount = 0;
            if (!readTieredObject(r, entry) || !r.u16(amount))
                return 0;
            entry->set_count(amount);
        }
        return r.i;
    }
    case 151: {
        auto* m = msg.MutableExtension(GameserverMessageEditList::gameserver_message_edit_list);
        uint8_t type = 0;
        uint32_t id = 0;
        if (!r.u8(type) || !r.u32(id) || !r.string(*m->mutable_text()))
            return 0;
        m->set_list_type(type);
        m->set_list_id(id);
        return r.i;
    }
    case 158: {
        auto* m = msg.MutableExtension(GameserverMessagePremiumTrigger::gameserver_message_premium_trigger);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            uint8_t trigger = 0;
            if (!r.u8(trigger))
                return 0;
            m->add_triggers(trigger);
        }
        return r.i;
    }
    case 183: {
        auto* m = msg.MutableExtension(GameserverMessageUnjustifiedPoints::gameserver_message_unjustified_points);
        uint8_t skull = 0;
        if (!readProgress(r, m->mutable_day()) || !readProgress(r, m->mutable_week()) || !readProgress(r, m->mutable_month())
            || !r.u8(skull))
            return 0;
        m->set_skull_duration(skull);
        return r.i;
    }
    case 185: {
        auto* m = msg.MutableExtension(GameserverMessageBestiaryTracker::gameserver_message_bestiary_tracker);
        uint8_t type = 0, count = 0;
        if (!r.u8(type) || !r.u8(count))
            return 0;
        m->set_tracker_type(type);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolBestiaryTrackerEntry* entry = m->add_entries();
            uint16_t race = 0, first = 0, second = 0, last = 0;
            uint32_t kills = 0;
            bool completed = false;
            if (!r.u16(race) || !r.u32(kills) || !r.u16(first) || !r.u16(second) || !r.u16(last) || !readBool(r, completed))
                return 0;
            entry->set_race_id(race);
            entry->set_kills(kills);
            entry->set_first_unlock(first);
            entry->set_second_unlock(second);
            entry->set_last_unlock(last);
            entry->set_completed(completed);
        }
        return r.i;
    }
    case 192: {
        auto* m = msg.MutableExtension(GameserverMessageUpdateManagedContainers::gameserver_message_update_managed_containers);
        bool fallback = false;
        uint8_t count = 0;
        if (!readBool(r, fallback) || !r.u8(count))
            return 0;
        m->set_fallback_to_main_container(fallback);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolManagedContainer* container = m->add_containers();
            uint8_t category = 0;
            uint16_t loot = 0, obtainer = 0;
            if (!r.u8(category) || !r.u16(loot) || !r.u16(obtainer))
                return 0;
            container->set_category(category);
            container->set_loot_container_id(loot);
            container->set_obtainer_container_id(obtainer);
        }
        return r.i;
    }
    case 204: {
        auto* m = msg.MutableExtension(GameserverMessageImpactTracking::gameserver_message_impact_tracking);
        uint8_t type = 0;
        uint32_t amount = 0;
        if (!r.u8(type) || !r.u32(amount))
            return 0;
        m->set_type(type);
        m->set_amount(amount);
        if (type == 2) { // damage received
            uint8_t element = 0;
            if (!r.u8(element) || !r.string(*m->mutable_source()))
                return 0;
            m->set_element(element);
        } else if (type == 1) { // damage dealt
            uint32_t elemental = 0;
            uint8_t element = 0;
            if (!r.u32(elemental) || !r.u8(element))
                return 0;
            m->set_element(0);
            m->set_elemental_amount(elemental);
            m->set_elemental_type(element);
        }
        return r.i;
    }
    case 205: {
        auto* m = msg.MutableExtension(GameserverMessageMarketStatistics::gameserver_message_market_statistics);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolMarketStatistic* entry = m->add_entries();
            uint64_t price = 0;
            if (!readTieredObject(r, entry) || !r.u64(price))
                return 0;
            entry->set_price(price);
        }
        return r.i;
    }
    case 207: {
        auto* m = msg.MutableExtension(GameserverMessageItemLooted::gameserver_message_item_looted);
        if (!readInventoryItem(r, m->mutable_item()) || !r.string(*m->mutable_name()))
            return 0;
        return r.i;
    }
    case 209: {
        auto* m = msg.MutableExtension(GameserverMessageKillTracking::gameserver_message_kill_tracking);
        uint8_t count = 0;
        if (!r.string(*m->mutable_name()) || !readOutfit(r, m->mutable_outfit(), true) || !r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            if (!readInventoryItem(r, m->add_items()))
                return 0;
        }
        return r.i;
    }
    case 223: {
        auto* m = msg.MutableExtension(GameserverMessageCreditBalance::gameserver_message_credit_balance);
        bool update = false;
        if (!readBool(r, update))
            return 0;
        m->set_update(update);
        if (update) {
            uint32_t total = 0, transferable = 0, auction = 0;
            if (!r.u32(total) || !r.u32(transferable) || !r.u32(auction))
                return 0;
            m->set_total(total);
            m->set_transferable(transferable);
            m->set_reserved_auction(auction);
        }
        return r.i;
    }
    case 237: {
        auto* m = msg.MutableExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog);
        uint8_t type = 0;
        if (!r.u8(type) || !r.string(*m->mutable_message()))
            return 0;
        m->set_dialog_type(type);
        if (type == 0x50) {
            uint32_t value = 0;
            if (!r.u32(value))
                return 0;
            m->set_extra_value(value);
        } else if (type == 0x5a) {
            uint8_t value = 0;
            if (!r.u8(value))
                return 0;
            m->set_extra_byte(value);
        }
        return r.i;
    }
    case 245: {
        auto* m = msg.MutableExtension(GameserverMessagePlayerInventory::gameserver_message_player_inventory);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolInventoryCount* entry = m->add_entries();
            uint16_t id = 0;
            uint8_t tier = 0;
            uint32_t amount = 0;
            if (!r.u16(id) || !r.u8(tier) || !readVarInt(r, amount))
                return 0;
            entry->set_object_id(id);
            entry->set_tier(tier);
            entry->set_count(amount);
        }
        return r.i;
    }
    case 137:
        msg.MutableExtension(GameserverMessageCloseExaltationDialog::gameserver_message_close_exaltation_dialog);
        return r.i;
    case 154:
        msg.MutableExtension(GameserverMessageCloseDepotSearch::gameserver_message_close_depot_search);
        return r.i;
    case 201:
        msg.MutableExtension(GameserverMessageExivaSuppressed::gameserver_message_exiva_suppressed);
        return r.i;
    case 236:
        msg.MutableExtension(GameserverMessageCloseImbuingDialog::gameserver_message_close_imbuing_dialog);
        return r.i;
    case 212: {
        auto* m = msg.MutableExtension(GameserverMessageBuddyGroupData::gameserver_message_buddy_group_data);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolBuddyGroup* group = m->add_groups();
            uint8_t id = 0;
            bool editable = false;
            if (!r.u8(id) || !r.string(*group->mutable_name()) || !readBool(r, editable))
                return 0;
            group->set_group_id(id);
            group->set_editable(editable);
        }
        uint8_t left = 0;
        if (!r.u8(left))
            return 0;
        m->set_groups_left(left);
        return r.i;
    }
    case 213: {
        auto* m = msg.MutableExtension(GameserverMessageMonsterCyclopedia::gameserver_message_monster_cyclopedia);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolMonsterClass* entry = m->add_classes();
            uint16_t total = 0, known = 0;
            if (!r.string(*entry->mutable_name()) || !r.u16(total) || !r.u16(known))
                return 0;
            entry->set_total(total);
            entry->set_known(known);
        }
        return r.i;
    }
    case 240: {
        auto* m = msg.MutableExtension(GameserverMessageQuestLog::gameserver_message_quest_log);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolQuest* quest = m->add_quests();
            uint16_t id = 0;
            bool completed = false;
            if (!r.u16(id) || !r.string(*quest->mutable_name()) || !readBool(r, completed))
                return 0;
            quest->set_quest_id(id);
            quest->set_completed(completed);
        }
        return r.i;
    }
    case 244: {
        auto* m = msg.MutableExtension(GameserverMessageObjectInfo::gameserver_message_object_info);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolObjectInfo* info = m->add_objects();
            uint16_t id = 0;
            uint8_t data = 0;
            if (!r.u16(id) || !r.u8(data) || !r.string(*info->mutable_description()))
                return 0;
            info->set_object_id(id);
            info->set_data(data);
        }
        return r.i;
    }
    case 246: {
        auto* m = msg.MutableExtension(GameserverMessageMarketEnter::gameserver_message_market_enter);
        uint8_t offers = 0;
        uint16_t count = 0;
        if (!r.u8(offers) || !r.u16(count))
            return 0;
        m->set_active_offers(offers);
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolDepotSearchEntry* entry = m->add_depot_objects();
            uint16_t amount = 0;
            if (!readTieredObject(r, entry) || !r.u16(amount))
                return 0;
            entry->set_count(amount);
        }
        return r.i;
    }
    case 115: {
        auto* m = msg.MutableExtension(GameserverMessageBosstiary::gameserver_message_bosstiary);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolBosstiaryEntry* boss = m->add_bosses();
            uint32_t race = 0, kills = 0;
            uint8_t category = 0, reserved = 0;
            bool tracked = false;
            if (!r.u32(race) || !r.u8(category) || !r.u32(kills) || !r.u8(reserved) || !readBool(r, tracked))
                return 0;
            boss->set_race_id(race);
            boss->set_category(category);
            boss->set_kills(kills);
            boss->set_reserved(reserved);
            boss->set_tracked(tracked);
        }
        return r.i;
    }
    case 138: {
        auto* m = msg.MutableExtension(GameserverMessageExaltationResult::gameserver_message_exaltation_result);
        uint8_t action = 0, bonus = 0;
        bool convergence = false, success = false;
        if (!r.u8(action) || !readBool(r, convergence) || !readBool(r, success) || !readTieredObject(r, m->mutable_first_object())
            || !readTieredObject(r, m->mutable_second_object()) || !r.u8(bonus))
            return 0;
        m->set_action_type(action);
        m->set_convergence(convergence);
        m->set_success(success);
        m->set_bonus(bonus);
        if (bonus >= 4 && bonus <= 8) { // the bonus names an object
            if (!readTieredObject(r, m->mutable_bonus_object()))
                return 0;
        } else if (bonus == 2) { // cores kept
            uint8_t cores = 0;
            if (!r.u8(cores))
                return 0;
            m->set_core_count(cores);
        }
        return r.i;
    }
    case 153: {
        auto* m = msg.MutableExtension(GameserverMessageDepotSearchDetailList::gameserver_message_depot_search_detail_list);
        uint32_t depot = 0, inbox = 0;
        uint8_t count = 0;
        if (!readTieredObject(r, m->mutable_object()) || !r.u32(depot) || !r.u8(count))
            return 0;
        m->set_depot_count(depot);
        for (uint8_t i = 0; i < count; ++i) {
            if (!readInventoryItem(r, m->add_depot_objects()))
                return 0;
        }
        if (!r.u32(inbox) || !r.u8(count))
            return 0;
        m->set_inbox_count(inbox);
        for (uint8_t i = 0; i < count; ++i) {
            if (!readInventoryItem(r, m->add_inbox_objects()))
                return 0;
        }
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolStashEntry* entry = m->add_stash();
            uint16_t id = 0;
            uint32_t amount = 0;
            if (!r.u16(id) || !r.u32(amount))
                return 0;
            entry->set_object_id(id);
            entry->set_amount(amount);
        }
        return r.i;
    }
    case 193: {
        auto* m = msg.MutableExtension(GameserverMessageVocationSpecificPlayerData::gameserver_message_vocation_specific_player_data);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_data_type(type);
        if (type == 0) { // harmony
            uint8_t harmony = 0;
            if (!r.u8(harmony))
                return 0;
            m->set_harmony(harmony);
        } else if (type == 1) { // serene
            bool serene = false;
            if (!readBool(r, serene))
                return 0;
            m->set_serene(serene);
        } else if (type == 2) { // virtue spells
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                uint16_t spell = 0;
                if (!r.u16(spell))
                    return 0;
                m->add_virtue_spell_ids(spell);
            }
        }
        return r.i;
    }
    case 214: {
        auto* m = msg.MutableExtension(GameserverMessageMonsterCyclopediaMonsters::gameserver_message_monster_cyclopedia_monsters);
        uint16_t count = 0;
        if (!r.string(*m->mutable_class_name()) || !r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolMonsterCyclopediaEntry* entry = m->add_monsters();
            uint16_t race = 0, bonus = 0;
            bool known = false;
            uint8_t progress = 0;
            if (!r.u16(race) || !readBool(r, known) || !r.u8(progress))
                return 0;
            entry->set_race_id(race);
            entry->set_known(known);
            entry->set_progress(progress);
            if (progress != 0) {
                uint8_t occurrence = 0, extra = 0;
                if (!r.u8(occurrence) || !r.u8(extra))
                    return 0;
                entry->set_occurrence(occurrence);
                entry->set_extra(extra);
            }
            if (!r.u16(bonus))
                return 0;
            entry->set_animus_mastery_bonus(bonus * 0.001);
        }
        uint16_t points = 0;
        if (!r.u16(points))
            return 0;
        m->set_animus_mastery_points(points);
        return r.i;
    }
    case 226: {
        auto* m = msg.MutableExtension(GameserverMessageOpenRewardWall::gameserver_message_open_reward_wall);
        bool shrine = false;
        uint32_t next = 0;
        uint8_t day = 0, collected = 0, state = 0;
        if (!readBool(r, shrine) || !r.u32(next) || !r.u8(day) || !r.u8(collected))
            return 0;
        m->set_from_shrine(shrine);
        m->set_next_reward_time(next);
        m->set_day_streak_day(day);
        m->set_collected(collected);
        if (collected != 0 && !r.string(*m->mutable_message()))
            return 0;
        if (!r.u8(state))
            return 0;
        m->set_pickup_state(state);
        if (state == 2) {
            uint32_t timeLeft = 0;
            if (!r.u32(timeLeft))
                return 0;
            m->set_time_left(timeLeft);
        }
        if (state == 1 || state == 2) {
            uint16_t jokers = 0;
            if (!r.u16(jokers))
                return 0;
            m->set_jokers(jokers);
        }
        uint16_t streak = 0;
        if (!r.u16(streak))
            return 0;
        m->set_streak_level(streak);
        return r.i;
    }
    case 241: {
        auto* m = msg.MutableExtension(GameserverMessageQuestLine::gameserver_message_quest_line);
        uint16_t quest = 0;
        uint8_t count = 0;
        if (!r.u16(quest) || !r.u8(count))
            return 0;
        m->set_quest_id(quest);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolQuestMission* mission = m->add_missions();
            uint16_t id = 0;
            if (!r.u16(id) || !r.string(*mission->mutable_name()) || !r.string(*mission->mutable_description()))
                return 0;
            mission->set_mission_id(id);
        }
        return r.i;
    }
    case 44: {
        auto* m = msg.MutableExtension(GameserverMessageTeamFinderTeamLeader::gameserver_message_team_finder_team_leader);
        uint8_t reset = 0;
        if (!r.u8(reset))
            return 0;
        m->set_reset(reset);
        if (reset != 0)
            return r.i;
        uint16_t count = 0;
        if (!readTeamSettings(r, m->mutable_settings()) || !r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolTeamMember* member = m->add_members();
            uint32_t guid = 0;
            uint16_t level = 0;
            uint8_t vocation = 0, status = 0;
            if (!r.u32(guid) || !r.string(*member->mutable_name()) || !r.u16(level) || !r.u8(vocation) || !r.u8(status))
                return 0;
            member->set_guid(guid);
            member->set_level(level);
            member->set_vocation(vocation);
            member->set_status(status);
        }
        return r.i;
    }
    case 45: {
        auto* m = msg.MutableExtension(GameserverMessageTeamFinderTeamMember::gameserver_message_team_finder_team_member);
        uint8_t limited = 0;
        if (!r.u8(limited))
            return 0;
        m->set_rate_limited(limited);
        if (limited != 0)
            return r.i;
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolTeamListing* team = m->add_teams();
            uint32_t leader = 0;
            uint8_t status = 0;
            if (!r.u32(leader) || !r.string(*team->mutable_leader_name()) || !readTeamSettings(r, team->mutable_settings())
                || !r.u8(status))
                return 0;
            team->set_leader_guid(leader);
            team->set_status(status);
        }
        return r.i;
    }
    case 98: {
        auto* m = msg.MutableExtension(GameserverMessageBossSlots::gameserver_message_boss_slots);
        uint32_t points = 0, nextPoints = 0;
        uint16_t current = 0, next = 0;
        bool hasUnlocked = false;
        if (!r.u32(points) || !r.u32(nextPoints) || !r.u16(current) || !r.u16(next) || !readBossSlot(r, m->mutable_first_slot())
            || !readBossSlot(r, m->mutable_second_slot()) || !readBossSlot(r, m->mutable_boosted_slot())
            || !readBool(r, hasUnlocked))
            return 0;
        m->set_points(points);
        m->set_points_next_bonus(nextPoints);
        m->set_current_bonus(current);
        m->set_next_bonus(next);
        m->set_has_unlocked_bosses(hasUnlocked);
        if (hasUnlocked) {
            uint16_t count = 0;
            if (!r.u16(count))
                return 0;
            for (uint16_t i = 0; i < count; ++i) {
                ProtocolBossSlotBoss* boss = m->add_unlocked_bosses();
                uint32_t id = 0;
                uint8_t race = 0;
                if (!r.u32(id) || !r.u8(race))
                    return 0;
                boss->set_boss_id(id);
                boss->set_race(race);
            }
        }
        return r.i;
    }
    case 150: {
        auto* m = msg.MutableExtension(GameserverMessageEditText::gameserver_message_edit_text);
        uint32_t id = 0;
        uint16_t maxLength = 0;
        bool traded = false;
        if (!r.u32(id) || !readInventoryItem(r, m->mutable_object()) || !r.u16(maxLength) || !r.string(*m->mutable_text())
            || !r.string(*m->mutable_author()) || !readBool(r, traded) || !r.string(*m->mutable_date()))
            return 0;
        m->set_edit_id(id);
        m->set_max_length(maxLength);
        m->set_recently_traded(traded);
        return r.i;
    }
    case 196: {
        auto* m = msg.MutableExtension(GameserverMessageWeaponProficiency::gameserver_message_weapon_proficiency);
        uint16_t id = 0;
        uint32_t experience = 0;
        uint8_t count = 0;
        if (!r.u16(id) || !r.u32(experience) || !r.u8(count))
            return 0;
        m->set_object_id(id);
        m->set_experience(experience);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolProficiencyPerk* perk = m->add_perks();
            uint8_t level = 0, position = 0;
            if (!r.u8(level) || !r.u8(position))
                return 0;
            perk->set_level(level);
            perk->set_position(position);
        }
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolProficiencyModifiedPerk* perk = m->add_modified_perks();
            uint8_t level = 0, position = 0, refine = 0;
            uint16_t modifier = 0;
            if (!r.u8(level) || !r.u8(position) || !r.u16(modifier) || !r.u8(refine))
                return 0;
            perk->set_level(level);
            perk->set_position(position);
            perk->set_modifier(modifier);
            perk->set_refine_level(refine);
        }
        return r.i;
    }
    case 208: {
        auto* m = msg.MutableExtension(GameserverMessageTrackQuestFlags::gameserver_message_track_quest_flags);
        bool full = false;
        if (!readBool(r, full))
            return 0;
        m->set_full_list(full);
        uint8_t count = 1;
        if (full) {
            uint8_t remaining = 0;
            if (!r.u8(remaining) || !r.u8(count))
                return 0;
            m->set_remaining(remaining);
        }
        for (uint8_t i = 0; i < count; ++i) {
            if (!readTrackedMission(r, m->add_missions()))
                return 0;
        }
        return r.i;
    }
    case 253: {
        auto* m = msg.MutableExtension(GameserverMessageTransactionHistory::gameserver_message_transaction_history);
        uint32_t page = 0, pages = 0;
        uint8_t count = 0;
        if (!r.u32(page) || !r.u32(pages) || !r.u8(count))
            return 0;
        m->set_current_page(page);
        m->set_page_count(pages);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolStoreTransaction* entry = m->add_transactions();
            uint32_t id = 0, time = 0, amount = 0;
            uint8_t mode = 0, coinType = 0;
            bool details = false;
            if (!r.u32(id) || !r.u32(time) || !r.u8(mode) || !r.u32(amount) || !r.u8(coinType)
                || !r.string(*entry->mutable_description()) || !readBool(r, details))
                return 0;
            entry->set_transaction_id(id);
            entry->set_timestamp(time);
            entry->set_mode(mode);
            entry->set_amount(static_cast<int32_t>(amount));
            entry->set_coin_type(coinType);
            entry->set_has_details(details);
        }
        return r.i;
    }
    case 28: {
        auto* m = msg.MutableExtension(GameserverMessageNpcTalkParters::gameserver_message_npc_talk_parters);
        bool closed = false;
        if (!readBool(r, closed))
            return 0;
        m->set_closed(closed);
        if (closed)
            return r.i;
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            uint32_t npc = 0;
            if (!r.u32(npc))
                return 0;
            m->add_npc_ids(npc);
        }
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolNpcTalkButton* button = m->add_buttons();
            uint8_t id = 0;
            if (!r.u8(id) || !r.string(*button->mutable_text()))
                return 0;
            button->set_button_id(id);
        }
        return r.i;
    }
    case 43: {
        auto* m = msg.MutableExtension(GameserverMessagePartyHuntAnalyser::gameserver_message_party_hunt_analyser);
        uint32_t start = 0, leader = 0;
        uint8_t price = 0, count = 0;
        if (!r.u32(start) || !r.u32(leader) || !r.u8(price) || !r.u8(count))
            return 0;
        m->set_session_start(start);
        m->set_leader_id(leader);
        m->set_price_type(price);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolPartyHuntMember* member = m->add_members();
            uint32_t id = 0;
            bool highlighted = false;
            uint64_t loot = 0, supplies = 0, damage = 0, healing = 0;
            if (!r.u32(id) || !readBool(r, highlighted) || !r.u64(loot) || !r.u64(supplies) || !r.u64(damage) || !r.u64(healing))
                return 0;
            member->set_member_id(id);
            member->set_highlighted(highlighted);
            member->set_loot(loot);
            member->set_supplies(supplies);
            member->set_damage(damage);
            member->set_healing(healing);
        }
        bool hasNames = false;
        if (!readBool(r, hasNames))
            return 0;
        if (hasNames) {
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolPartyHuntName* name = m->add_names();
                uint32_t id = 0;
                if (!r.u32(id) || !r.string(*name->mutable_name()))
                    return 0;
                name->set_member_id(id);
            }
        }
        return r.i;
    }
    case 187: {
        auto* m = msg.MutableExtension(GameserverMessageShapedPerkReshapeOffers::gameserver_message_shaped_perk_reshape_offers);
        uint16_t id = 0;
        uint8_t level = 0, position = 0, count = 0;
        if (!r.u16(id) || !r.u8(level) || !r.u8(position) || !r.u8(count))
            return 0;
        m->set_object_id(id);
        ProtocolReshapeSlot* slot = m->mutable_slot();
        slot->set_level(level);
        slot->set_position(position);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolReshapeOffer* offer = slot->add_offers();
            uint16_t modifier = 0;
            uint8_t refine = 0;
            if (!r.u16(modifier) || !r.u8(refine))
                return 0;
            offer->set_modifier(modifier);
            offer->set_refine_level(refine);
        }
        return r.i;
    }
    case 198: {
        auto* m = msg.MutableExtension(GameserverMessageCyclopediaStaticHouseData::gameserver_message_cyclopedia_static_house_data);
        uint32_t excluded = 0;
        uint8_t counts[6] = {};
        if (!r.u32(excluded))
            return 0;
        for (uint8_t& c : counts) {
            if (!r.u8(c))
                return 0;
        }
        m->set_excluded_until(excluded);
        m->set_first_count(counts[0]);
        m->set_second_count(counts[1]);
        m->set_third_count(counts[2]);
        m->set_fourth_count(counts[3]);
        m->set_fifth_count(counts[4]);
        m->set_sixth_count(counts[5]);
        uint8_t shown = 0;
        if (!r.u8(shown))
            return 0;
        for (uint8_t i = 0; i < shown; ++i) {
            uint32_t house = 0;
            if (!r.u32(house))
                return 0;
            m->add_shown_houses(house);
        }
        uint16_t others = 0;
        if (!r.u16(others))
            return 0;
        for (uint16_t i = 0; i < others; ++i) {
            uint32_t house = 0;
            if (!r.u32(house))
                return 0;
            m->add_other_houses(house);
        }
        return r.i;
    }
    case 155: {
        auto* m = msg.MutableExtension(GameserverMessageBlessingsDialog::gameserver_message_blessings_dialog);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolBlessing* blessing = m->add_blessings();
            uint16_t bit = 0;
            uint8_t have = 0, store = 0;
            if (!r.u16(bit) || !r.u8(have) || !r.u8(store))
                return 0;
            blessing->set_blessing(bit);
            blessing->set_count(have);
            blessing->set_store_count(store);
        }
        bool premium = false, skull = false, amulet = false;
        uint8_t promotion = 0, pvpMin = 0, pvpMax = 0, pve = 0, equipPvp = 0, equipPve = 0;
        if (!readBool(r, premium) || !r.u8(promotion) || !r.u8(pvpMin) || !r.u8(pvpMax) || !r.u8(pve) || !r.u8(equipPvp)
            || !r.u8(equipPve) || !readBool(r, skull) || !readBool(r, amulet))
            return 0;
        m->set_premium(premium);
        m->set_promotion(promotion);
        m->set_pvp_min_xp_loss(pvpMin);
        m->set_pvp_max_xp_loss(pvpMax);
        m->set_pve_xp_loss(pve);
        // 0xff: the whole equipment is lost.
        m->set_equip_pvp_loss(equipPvp == 0xff ? 100 : equipPvp);
        m->set_equip_pvp_loss_tenth(equipPvp == 0xff ? 100 : equipPvp / 10);
        m->set_equip_pve_loss(equipPve == 0xff ? 100 : equipPve);
        m->set_equip_pve_loss_tenth(equipPve == 0xff ? 100 : equipPve / 10);
        m->set_skull(skull);
        m->set_amulet_of_loss(amulet);
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolBlessingHistoryEntry* entry = m->add_history();
            uint32_t time = 0;
            uint8_t color = 0;
            if (!r.u32(time) || !r.u8(color) || !r.string(*entry->mutable_text()))
                return 0;
            entry->set_timestamp(time);
            entry->set_color(color);
        }
        return r.i;
    }
    case 210: {
        auto* m = msg.MutableExtension(GameserverMessageBuddyData::gameserver_message_buddy_data);
        uint32_t id = 0, icon = 0;
        bool notify = false;
        uint8_t status = 0, count = 0;
        if (!r.u32(id) || !r.string(*m->mutable_name()) || !r.string(*m->mutable_description()) || !r.u32(icon)
            || !readBool(r, notify) || !r.u8(status) || !r.u8(count))
            return 0;
        m->set_buddy_id(id);
        m->set_icon(icon);
        m->set_notify_login(notify);
        m->set_status(status);
        for (uint8_t i = 0; i < count; ++i) {
            uint8_t group = 0;
            if (!r.u8(group))
                return 0;
            m->add_group_ids(group);
        }
        return r.i;
    }
    case 250: {
        auto* m = msg.MutableExtension(GameserverMessageShowModalDialog::gameserver_message_show_modal_dialog);
        uint32_t id = 0;
        if (!r.u32(id) || !r.string(*m->mutable_title()) || !r.string(*m->mutable_message()))
            return 0;
        m->set_dialog_id(id);
        for (auto* list : { m->mutable_buttons(), m->mutable_choices() }) {
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolModalChoice* choice = list->Add();
                uint8_t choiceId = 0;
                if (!r.string(*choice->mutable_text()) || !r.u8(choiceId))
                    return 0;
                choice->set_choice_id(choiceId);
            }
        }
        uint8_t escape = 0, enter = 0;
        bool priority = false;
        if (!r.u8(escape) || !r.u8(enter) || !readBool(r, priority))
            return 0;
        m->set_escape_button(escape);
        m->set_enter_button(enter);
        m->set_priority(priority);
        return r.i;
    }
    case 251: {
        auto* m = msg.MutableExtension(GameserverMessageStoreCategories::gameserver_message_store_categories);
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolStoreCategory* category = m->add_categories();
            uint8_t state = 0, icons = 0;
            if (!r.string(*category->mutable_name()) || !r.u8(state) || !r.u8(icons))
                return 0;
            category->set_state(state);
            for (uint8_t k = 0; k < icons; ++k) {
                if (!r.string(*category->add_icons()))
                    return 0;
            }
            if (!r.string(*category->mutable_parent()))
                return 0;
        }
        return r.i;
    }
    case 93: {
        auto* m = msg.MutableExtension(GameserverMessageImbuementDurations::gameserver_message_imbuement_durations);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolImbuementTrackerItem* item = m->add_items();
            uint8_t slot = 0, slots = 0;
            if (!r.u8(slot) || !readInventoryItem(r, item->mutable_item()) || !r.u8(slots))
                return 0;
            item->set_inventory_slot(slot);
            for (uint8_t k = 0; k < slots; ++k) {
                ProtocolImbuementTrackerSlot* entry = item->add_slots();
                bool imbued = false;
                if (!readBool(r, imbued))
                    return 0;
                entry->set_imbued(imbued);
                if (!imbued)
                    continue;
                uint16_t icon = 0;
                uint32_t duration = 0;
                bool decaying = false;
                if (!r.string(*entry->mutable_name()) || !r.u16(icon) || !r.u32(duration) || !readBool(r, decaying))
                    return 0;
                entry->set_icon_id(icon);
                entry->set_duration(duration);
                entry->set_decaying(decaying);
            }
        }
        return r.i;
    }
    case 134: {
        auto* m = msg.MutableExtension(GameserverMessageExaltationBaseData::gameserver_message_exaltation_base_data);
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolClassTierPrices* prices = m->add_class_prices();
            uint8_t classId = 0;
            if (!r.u8(classId) || !readTierPrices(r, prices->mutable_tiers()))
                return 0;
            prices->set_class_id(classId);
        }
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolFusionGrade* grade = m->add_fusion_grades();
            uint8_t tier = 0, cores = 0;
            if (!r.u8(tier) || !r.u8(cores))
                return 0;
            grade->set_tier(tier);
            grade->set_cores(cores);
        }
        uint8_t dust = 0;
        if (!readTierPrices(r, m->mutable_convergence_fusion_prices()) || !readTierPrices(r, m->mutable_convergence_transfer_prices())
            || !r.u8(dust))
            return 0;
        m->set_dust_level(dust);
        return r.i;
    }
    case 202: {
        auto* m = msg.MutableExtension(GameserverMessageUpdateExivaOptions::gameserver_message_update_exiva_options);
        bool flags[6] = {};
        for (bool& flag : flags) {
            if (!readBool(r, flag))
                return 0;
        }
        m->set_allow_all(flags[0]);
        m->set_allow_own_guild(flags[1]);
        m->set_allow_own_party(flags[2]);
        m->set_allow_vip_list(flags[3]);
        m->set_allow_character_whitelist(flags[4]);
        m->set_allow_guild_whitelist(flags[5]);
        if (!readStrings16(r, m->mutable_character_whitelist()) || !readStrings16(r, m->mutable_removed_characters())
            || !readStrings16(r, m->mutable_guild_whitelist()) || !readStrings16(r, m->mutable_removed_guilds()))
            return 0;
        return r.i;
    }
    case 225: {
        auto* m = msg.MutableExtension(GameserverMessageRequestPurchaseData::gameserver_message_request_purchase_data);
        uint32_t offer = 0;
        bool tokens = false;
        uint8_t type = 0;
        if (!r.u32(offer) || !readBool(r, tokens) || !r.u8(type))
            return 0;
        m->set_offer_id(offer);
        m->set_cosmetic_tokens(tokens);
        m->set_purchase_type(type);
        if (type == 6) { // confirmation
            if (!r.string(*m->mutable_message()))
                return 0;
        } else if (type == 4) { // main character
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolMainCharacterOption* option = m->add_main_characters();
                uint16_t level = 0;
                if (!r.string(*option->mutable_name()) || !r.string(*option->mutable_world()) || !r.string(*option->mutable_vocation())
                    || !r.u16(level))
                    return 0;
                option->set_level(level);
            }
        } else if (type == 2) { // world transfer
            ProtocolWorldTransferInformation* info = m->mutable_world_transfer();
            bool flag = false;
            uint8_t count = 0;
            if (!readBool(r, flag) || !r.u8(count))
                return 0;
            info->set_flag(flag);
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolTransferWorld* world = info->add_worlds();
                bool first = false, second = false;
                uint8_t pvp = 0;
                if (!r.string(*world->mutable_name()) || !readBool(r, first) || !readBool(r, second) || !r.u8(pvp))
                    return 0;
                world->set_first_flag(first);
                world->set_second_flag(second);
                world->set_pvp_type(pvp);
            }
            for (int i = 0; i < 5; ++i) {
                bool value = false;
                if (!readBool(r, value))
                    return 0;
                info->add_flags(value);
            }
        }
        return r.i;
    }
    case 156: {
        auto* m = msg.MutableExtension(GameserverMessageBlessings::gameserver_message_blessings);
        uint16_t mask = 0;
        uint8_t state = 0;
        if (!r.u16(mask) || !r.u8(state))
            return 0;
        m->set_blessings(mask);
        m->set_visual_state(state);
        return r.i;
    }
    case 194: {
        auto* m = msg.MutableExtension(GameserverMessageConfigureCreaturePodium::gameserver_message_configure_creature_podium);
        uint16_t reserved = 0, count = 0;
        bool boss = false;
        if (!readOutfit(r, m->mutable_outfit(), true) || !r.u16(reserved) || !readBool(r, boss) || !r.u16(count))
            return 0;
        m->set_reserved(reserved);
        m->set_boss_podium(boss);
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolPodiumRace* race = m->add_races();
            uint16_t id = 0;
            if (!r.u16(id))
                return 0;
            race->set_race_id(id);
            if (boss && (!r.string(*race->mutable_name()) || !readOutfit(r, race->mutable_outfit(), true)))
                return 0;
        }
        uint16_t x = 0, y = 0, object = 0;
        uint8_t z = 0, stack = 0, direction = 0;
        bool podiumVisible = false, monsterVisible = false;
        if (!r.u16(x) || !r.u16(y) || !r.u8(z) || !r.u16(object) || !r.u8(stack) || !readBool(r, podiumVisible)
            || !readBool(r, monsterVisible) || !r.u8(direction))
            return 0;
        m->mutable_position()->set_x(x);
        m->mutable_position()->set_y(y);
        m->mutable_position()->set_z(z);
        m->set_object_id(object);
        m->set_stack_position(stack);
        m->set_podium_visible(podiumVisible);
        m->set_monster_visible(monsterVisible);
        m->set_direction(direction);
        return r.i;
    }
    case 203: {
        auto* m = msg.MutableExtension(GameserverMessageTransactionDetails::gameserver_message_transaction_details);
        uint32_t id = 0;
        uint8_t type = 0;
        if (!r.u32(id) || !r.u8(type))
            return 0;
        m->set_transaction_id(id);
        m->set_detail_type(type);
        if (type == 1) {
            ProtocolTransactionPurchase* p = m->mutable_purchase();
            uint32_t a = 0, b = 0, c = 0;
            uint64_t d = 0, e = 0;
            uint16_t count = 0;
            if (!r.string(*p->mutable_text()) || !r.u32(a) || !r.u32(b) || !r.u32(c) || !r.u64(d) || !r.u64(e) || !r.u16(count))
                return 0;
            p->set_first_value(a);
            p->set_second_value(b);
            p->set_third_value(c);
            p->set_fourth_value(d);
            p->set_fifth_value(e);
            for (uint16_t i = 0; i < count; ++i) {
                ProtocolTransactionItem* item = p->add_items();
                uint32_t first = 0;
                uint8_t second = 0;
                uint64_t amount = 0;
                if (!r.u32(first) || !r.string(*item->mutable_text()) || !r.u8(second) || !r.u64(amount))
                    return 0;
                item->set_first_value(first);
                item->set_second_value(second);
                item->set_amount(static_cast<int64_t>(amount));
            }
        } else if (type == 0) {
            ProtocolTransactionTransfer* t = m->mutable_transfer();
            uint32_t a = 0, b = 0;
            uint64_t c = 0, d = 0;
            if (!r.u32(a) || !r.string(*t->mutable_first_text()) || !r.string(*t->mutable_second_text()) || !r.u32(b) || !r.u64(c)
                || !r.u64(d))
                return 0;
            t->set_first_value(a);
            t->set_second_value(static_cast<int32_t>(b));
            t->set_third_value(c);
            t->set_fourth_value(static_cast<int64_t>(d));
        }
        return r.i;
    }
    case 232: {
        auto* m = msg.MutableExtension(GameserverMessagePreyData::gameserver_message_prey_data);
        uint8_t slot = 0, state = 0;
        if (!r.u8(slot) || !r.u8(state))
            return 0;
        m->set_slot(slot);
        m->set_state(state);
        const auto readBonus = [&]() {
            uint8_t type = 0, grade = 0;
            uint16_t value = 0;
            if (!r.u8(type) || !r.u16(value) || !r.u8(grade))
                return false;
            m->set_bonus_type(type);
            m->set_bonus_value(value);
            m->set_bonus_grade(grade);
            return true;
        };
        switch (state) {
        case 0: { // locked
            uint8_t unlock = 0;
            if (!r.u8(unlock))
                return 0;
            m->set_unlock_state(unlock);
            break;
        }
        case 2: { // active
            uint16_t timeLeft = 0;
            if (!readPreyMonster(r, m->add_monsters()) || !readBonus() || !r.u16(timeLeft))
                return 0;
            m->set_time_left(timeLeft);
            break;
        }
        case 4: // selection with a bonus
        case 6: // list selection with a bonus
            if (!readBonus())
                return 0;
            break;
        default:
            break;
        }
        if (state == 5 || state == 6) {
            uint16_t count = 0;
            if (!r.u16(count))
                return 0;
            for (uint16_t i = 0; i < count; ++i) {
                uint16_t race = 0;
                if (!r.u16(race))
                    return 0;
                m->add_race_ids(race);
            }
        } else if (state == 3 || state == 4) {
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (!readPreyMonster(r, m->add_monsters()))
                    return 0;
            }
        }
        uint32_t reroll = 0;
        uint8_t option = 0;
        if (!r.u32(reroll) || !r.u8(option))
            return 0;
        m->set_next_free_reroll(reroll);
        m->set_option(option);
        return r.i;
    }
    case 235: {
        auto* m = msg.MutableExtension(GameserverMessageImbuingDialogRefresh::gameserver_message_imbuing_dialog_refresh);
        uint8_t type = 0;
        bool flag = false;
        if (!r.u8(type) || !readBool(r, flag))
            return 0;
        m->set_window_type(type);
        m->set_flag(flag);
        if (type == 2) { // scroll
            bool first = false, second = false;
            if (!readBool(r, first) || !readBool(r, second))
                return 0;
            m->set_first_scroll_flag(first);
            m->set_second_scroll_flag(second);
        } else if (type == 1) { // item
            uint16_t object = 0;
            uint8_t slots = 0;
            if (!r.u16(object))
                return 0;
            m->set_object_id(object);
            if (tieredObject(object)) {
                uint8_t tier = 0;
                if (!r.u8(tier))
                    return 0;
                m->set_tier(tier);
            }
            if (!r.u8(slots))
                return 0;
            for (uint8_t i = 0; i < slots; ++i) {
                ProtocolImbuingSlot* slot = m->add_slots();
                bool occupied = false;
                if (!readBool(r, occupied))
                    return 0;
                slot->set_occupied(occupied);
                if (!occupied)
                    continue;
                uint32_t duration = 0, removal = 0;
                if (!readImbuement(r, slot->mutable_imbuement()) || !r.u32(duration) || !r.u32(removal))
                    return 0;
                slot->set_duration(duration);
                slot->set_removal_cost(removal);
            }
        }
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            if (!readImbuement(r, m->add_imbuements()))
                return 0;
        }
        uint32_t needed = 0;
        if (!r.u32(needed))
            return 0;
        for (uint32_t i = 0; i < needed; ++i) {
            ProtocolNeededObject* object = m->add_needed_objects();
            uint16_t id = 0, amount = 0;
            if (!r.u16(id) || !r.u16(amount))
                return 0;
            object->set_object_id(id);
            object->set_count(amount);
        }
        return r.i;
    }
    case 47: {
        auto* m = msg.MutableExtension(GameserverMessageBossDifficultySelection::gameserver_message_boss_difficulty_selection);
        uint8_t state = 0;
        if (!r.u8(state))
            return 0;
        m->set_state(state);
        if (state != 0 && state != 2) // closed
            return r.i;
        if (state == 0) {
            uint32_t lowest = 0, badLuck = 0;
            bool locked = false;
            uint16_t race = 0, selected = 0, group = 0, personal = 0;
            if (!r.u32(lowest) || !readBool(r, locked) || !r.u16(race) || !r.u16(selected) || !r.u16(group) || !r.u16(personal)
                || !r.u32(badLuck))
                return 0;
            m->set_lowest_difficulty(lowest);
            m->set_spinner_locked(locked);
            m->set_race_id(race);
            m->set_selected_difficulty(selected);
            m->set_group_highest(group);
            m->set_personal_highest(personal);
            m->set_bad_luck(badLuck);
            for (int i = 0; i < 5; ++i) {
                if (!r.string(*m->add_banners()))
                    return 0;
            }
        }
        uint16_t difficulty = 0;
        if (!r.u16(difficulty))
            return 0;
        m->set_difficulty(difficulty);
        for (auto* list : { m->mutable_red_entries(), m->mutable_green_entries() }) {
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (!r.string(*list->Add()))
                    return 0;
            }
        }
        return r.i;
    }
    case 97: {
        auto* m = msg.MutableExtension(GameserverMessageBosstiaryStaticData::gameserver_message_bosstiary_static_data);
        for (auto* list : { m->mutable_kills_for_stage(), m->mutable_points_for_stage() }) {
            for (int i = 0; i < 9; ++i) {
                uint16_t value = 0;
                if (!r.u16(value))
                    return 0;
                list->Add(value);
            }
        }
        return r.i;
    }
    case 176: {
        auto* m = msg.MutableExtension(GameserverMessageOutfitMemorial::gameserver_message_outfit_memorial);
        uint32_t base = 0, one = 0, full = 0;
        if (!r.u32(base) || !r.u32(one) || !r.u32(full) || !readStrings16(r, m->mutable_base_golden_owners())
            || !readStrings16(r, m->mutable_one_addon_golden_owners()) || !readStrings16(r, m->mutable_full_golden_owners()))
            return 0;
        m->set_base_golden_cost(base);
        m->set_one_addon_golden_cost(one);
        m->set_full_golden_cost(full);
        uint16_t tokens[6] = {};
        for (uint16_t& t : tokens) {
            if (!r.u16(t))
                return 0;
        }
        m->set_base_silver_tokens(tokens[0]);
        m->set_base_gold_tokens(tokens[1]);
        m->set_one_addon_silver_tokens(tokens[2]);
        m->set_one_addon_gold_tokens(tokens[3]);
        m->set_full_silver_tokens(tokens[4]);
        m->set_full_gold_tokens(tokens[5]);
        if (!readStrings16(r, m->mutable_base_token_owners()) || !readStrings16(r, m->mutable_one_addon_token_owners())
            || !readStrings16(r, m->mutable_full_token_owners()))
            return 0;
        return r.i;
    }
    case 200: {
        auto* m = msg.MutableExtension(GameserverMessageOutfit::gameserver_message_outfit);
        // The player's own outfit: a creature look (no item look).
        ProtocolOutfit* outfit = m->mutable_outfit();
        uint16_t look = 0;
        if (!r.u16(look))
            return 0;
        outfit->set_look_type(look);
        if (look != 0) {
            uint8_t head = 0, body = 0, legs = 0, feet = 0, addons = 0;
            if (!r.u8(head) || !r.u8(body) || !r.u8(legs) || !r.u8(feet) || !r.u8(addons))
                return 0;
            outfit->set_head(head);
            outfit->set_body(body);
            outfit->set_legs(legs);
            outfit->set_feet(feet);
            outfit->set_addons(addons);
        }
        uint16_t mount = 0, familiar = 0;
        uint8_t mh = 0, mb = 0, ml = 0, mf = 0;
        if (!r.u16(mount) || !r.u8(mh) || !r.u8(mb) || !r.u8(ml) || !r.u8(mf) || !r.u16(familiar))
            return 0;
        m->set_mount(mount);
        ProtocolOutfit* mountColors = m->mutable_mount_colors();
        mountColors->set_look_type(mount);
        mountColors->set_head(mh);
        mountColors->set_body(mb);
        mountColors->set_legs(ml);
        mountColors->set_feet(mf);
        m->set_familiar(familiar);
        if (!readOutfitOptions(r, m->mutable_outfits(), true) || !readOutfitOptions(r, m->mutable_mounts(), false)
            || !readOutfitOptions(r, m->mutable_familiars(), false))
            return 0;
        uint8_t mode = 0;
        bool mounted = false;
        if (!r.u8(mode) || !readBool(r, mounted))
            return 0;
        m->set_window_mode(mode);
        m->set_mounted(mounted);
        if (mode == 0) { // own outfit
            bool random = false;
            if (!readBool(r, random))
                return 0;
            m->set_random_mount(random);
        } else if (mode == 3) { // hireling
            uint32_t hireling = 0;
            if (!r.u32(hireling))
                return 0;
            m->set_hireling_id(hireling);
        } else if (mode == 4 || mode == 5) { // outfit pairs (5: a podium)
            uint16_t count = 0;
            if (!r.u16(count))
                return 0;
            for (uint16_t i = 0; i < count; ++i) {
                ProtocolGenderPair* pair = m->add_gender_pairs();
                uint16_t male = 0, female = 0;
                if (!r.u16(male) || !r.u16(female))
                    return 0;
                pair->set_male(male);
                pair->set_female(female);
            }
            if (mode == 5) {
                uint16_t x = 0, y = 0, object = 0;
                uint8_t z = 0, stack = 0, direction = 0;
                bool visible = false, showOutfit = false;
                if (!r.u16(x) || !r.u16(y) || !r.u8(z) || !r.u16(object) || !r.u8(stack) || !readBool(r, visible)
                    || !readBool(r, showOutfit) || !r.u8(direction))
                    return 0;
                m->mutable_podium_position()->set_x(x);
                m->mutable_podium_position()->set_y(y);
                m->mutable_podium_position()->set_z(z);
                m->set_podium_object(object);
                m->set_podium_stack_position(stack);
                m->set_podium_visible(visible);
                m->set_podium_show_outfit(showOutfit);
                m->set_podium_direction(direction);
            }
        }
        return r.i;
    }
    case 215: {
        auto* m = msg.MutableExtension(GameserverMessageMonsterCyclopediaRace::gameserver_message_monster_cyclopedia_race);
        uint16_t race = 0, bonus = 0, points = 0, first = 0, second = 0, last = 0;
        uint8_t progress = 0;
        uint32_t kills = 0;
        bool difficulty = false;
        if (!r.u16(race) || !r.string(*m->mutable_class_name()) || !r.u8(progress) || !r.u16(bonus) || !r.u16(points) || !r.u32(kills)
            || !r.u16(first) || !r.u16(second) || !r.u16(last) || !readBool(r, difficulty))
            return 0;
        m->set_race_id(race);
        m->set_progress(progress);
        m->set_animus_mastery_bonus(bonus * 0.001);
        m->set_animus_mastery_points(points);
        m->set_kills(kills);
        m->set_first_unlock(first);
        m->set_second_unlock(second);
        m->set_last_unlock(last);
        m->set_difficulty(difficulty);
        if (progress == 0)
            return r.i;
        uint8_t occurrence = 0, leader = 0, count = 0;
        if (!r.u8(occurrence) || !r.u8(leader) || !r.u8(count))
            return 0;
        m->set_occurrence(occurrence);
        m->set_leader_killed(leader);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolMonsterLoot* loot = m->add_loot();
            uint16_t object = 0;
            uint8_t rarity = 0;
            bool event = false;
            if (!r.u16(object) || !r.u8(rarity) || !readBool(r, event))
                return 0;
            loot->set_object_id(object);
            loot->set_rarity(rarity);
            loot->set_special_event(event);
            if (object != 0) {
                uint8_t amount = 0;
                if (!r.string(*loot->mutable_name()) || !r.u8(amount))
                    return 0;
                loot->set_amount(amount);
            }
        }
        if (progress == 1)
            return r.i;
        uint16_t charm = 0, speed = 0, armor = 0;
        uint8_t attack = 0;
        bool spells = false;
        uint32_t health = 0, experience = 0;
        double mitigation = 0;
        if (!r.u16(charm) || !r.u8(attack) || !readBool(r, spells) || !r.u32(health) || !r.u32(experience) || !r.u16(speed)
            || !r.u16(armor) || !r.real(mitigation))
            return 0;
        m->set_charm_value(charm);
        m->set_attack_mode(attack);
        m->set_casts_spells(spells);
        m->set_max_health(health);
        m->set_experience(experience);
        m->set_speed(speed);
        m->set_armor(armor);
        m->set_mitigation(mitigation);
        if (progress == 2)
            return r.i;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolElementModifier* element = m->add_elements();
            uint8_t id = 0;
            int16_t value = 0;
            if (!r.u8(id) || !r.s16(value))
                return 0;
            element->set_element(id);
            element->set_value(value);
        }
        if (!readStrings16(r, m->mutable_locations()))
            return 0;
        return r.i;
    }
    case 249: {
        auto* m = msg.MutableExtension(GameserverMessageMarketBrowse::gameserver_message_market_browse);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_request_type(type);
        if (type == 3 && !readTieredObject(r, m->mutable_object())) // item browse
            return 0;
        for (auto* list : { m->mutable_buy_offers(), m->mutable_sell_offers() }) {
            uint32_t count = 0;
            if (!r.u32(count))
                return 0;
            for (uint32_t i = 0; i < count; ++i) {
                ProtocolMarketOffer* offer = list->Add();
                uint32_t time = 0;
                uint16_t counter = 0, amount = 0;
                uint64_t price = 0;
                if (!r.u32(time) || !r.u16(counter))
                    return 0;
                offer->set_timestamp(time);
                offer->set_counter(counter);
                if (type == 1 || type == 2) { // own history / own offers
                    if (!readTieredObject(r, offer->mutable_object()))
                        return 0;
                } else {
                    *offer->mutable_object() = m->object();
                }
                if (!r.u16(amount) || !r.u64(price))
                    return 0;
                offer->set_amount(amount);
                offer->set_price(price);
                if (type == 1) {
                    uint8_t state = 0;
                    if (!r.u8(state))
                        return 0;
                    offer->set_state(state);
                } else if (type != 2) {
                    if (!r.string(*offer->mutable_character_name()))
                        return 0;
                }
            }
        }
        return r.i;
    }
    case 177: {
        auto* m = msg.MutableExtension(GameserverMessageHighscores::gameserver_message_highscores);
        uint8_t noData = 0;
        if (!r.u8(noData))
            return 0;
        m->set_no_data(noData);
        if (noData != 0)
            return r.i;
        uint8_t count = 0;
        if (!r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            if (!r.string(*m->add_worlds()))
                return 0;
        }
        if (!r.string(*m->mutable_selected_world()) || !r.u8(count))
            return 0;
        for (uint8_t i = 0; i < count; ++i) {
            uint8_t category = 0;
            if (!r.u8(category))
                return 0;
            m->add_world_categories(category);
        }
        uint8_t battlEye = 0;
        if (!r.u8(battlEye) || !r.u8(count))
            return 0;
        m->set_battleye_type(static_cast<int8_t>(battlEye));
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolHighscoreVocation* vocation = m->add_vocations();
            uint32_t id = 0;
            if (!r.u32(id) || !r.string(*vocation->mutable_name()))
                return 0;
            vocation->set_vocation_id(static_cast<int32_t>(id));
        }
        uint32_t selectedVocation = 0;
        if (!r.u32(selectedVocation) || !r.u8(count))
            return 0;
        m->set_selected_vocation(static_cast<int32_t>(selectedVocation));
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolHighscoreCategory* category = m->add_categories();
            uint8_t id = 0;
            if (!r.u8(id) || !r.string(*category->mutable_name()))
                return 0;
            category->set_category_id(id);
        }
        uint8_t selectedCategory = 0;
        uint16_t page = 0, pages = 0;
        if (!r.u8(selectedCategory) || !r.u16(page) || !r.u16(pages) || !r.u8(count))
            return 0;
        m->set_selected_category(selectedCategory);
        m->set_page(page);
        m->set_page_count(pages);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolHighscoreEntry* entry = m->add_entries();
            uint32_t rank = 0;
            uint8_t vocation = 0;
            uint16_t level = 0;
            bool own = false;
            uint64_t points = 0;
            if (!r.u32(rank) || !r.string(*entry->mutable_name()) || !r.string(*entry->mutable_title()) || !r.u8(vocation)
                || !r.string(*entry->mutable_world()) || !r.u16(level) || !readBool(r, own) || !r.u64(points))
                return 0;
            entry->set_rank(rank);
            entry->set_vocation(vocation);
            entry->set_level(level);
            entry->set_is_player(own);
            entry->set_points(points);
        }
        uint8_t selectedIndex = 0, valueType = 0;
        bool loyalty = false;
        uint32_t updated = 0;
        if (!r.u8(selectedIndex) || !readBool(r, loyalty) || !r.u8(valueType) || !r.u32(updated))
            return 0;
        m->set_selected_index(static_cast<int8_t>(selectedIndex));
        m->set_show_loyalty_title(loyalty);
        m->set_value_type(valueType);
        m->set_last_update(updated);
        return r.i;
    }
    case 168:
        if (!readStoreOpenParameters(
                r, msg.MutableExtension(GameserverMessageSetStoreButtonDeeplink::gameserver_message_set_store_button_deeplink)->mutable_parameters()))
            return 0;
        return r.i;
    case 3: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureData::gameserver_message_creature_data);
        constexpr uint16_t NewCreature = 0x61;
        if (!readCreature(r, m->mutable_creature(), NewCreature))
            return 0;
        return r.i;
    }
    case 118: {
        auto* m = msg.MutableExtension(GameserverMessageInspectionList::gameserver_message_inspection_list);
        uint8_t window = 0, type = 0, count = 0;
        uint32_t creature = 0;
        if (!r.u8(window) || !r.u8(type) || !r.u32(creature) || !r.u8(count))
            return 0;
        m->set_window_type(window);
        m->set_inspection_type(type);
        m->set_creature_id(creature);
        for (uint8_t i = 0; i < count; ++i) {
            ProtocolInspectedObject* object = m->add_objects();
            if (window == 1) { // character: the inventory slot
                uint8_t slot = 0;
                if (!r.u8(slot))
                    return 0;
                object->set_slot(slot);
            }
            uint8_t icons = 0;
            if (!r.string(*object->mutable_name()) || !readInventoryItem(r, object->mutable_item()) || !r.u8(icons))
                return 0;
            for (uint8_t k = 0; k < icons; ++k) {
                uint16_t icon = 0;
                if (!r.u16(icon))
                    return 0;
                object->add_imbuement_icons(icon);
            }
            if (!readInspectionDetails(r, object->mutable_details()))
                return 0;
        }
        if (window == 1 && (!r.string(*m->mutable_player_name()) || !readInspectionDetails(r, m->mutable_player_details())))
            return 0;
        return r.i;
    }
    case 128: {
        auto* m = msg.MutableExtension(GameserverMessageCharacterTradeConfiguration::gameserver_message_character_trade_configuration);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_data_type(type);
        if (type == 0) { // base data and conditions
            uint16_t level = 0, minimumBid = 0;
            uint8_t vocation = 0, world = 0, count = 0;
            uint32_t start = 0, end = 0;
            if (!r.string(*m->mutable_name()) || !r.u16(level) || !r.u8(vocation) || !readOutfit(r, m->mutable_outfit(), true)
                || !r.u8(world) || !r.u32(start) || !r.u32(end) || !r.u16(minimumBid) || !r.u8(count))
                return 0;
            m->set_level(level);
            m->set_vocation(vocation);
            m->set_world_type(world);
            m->set_start_time(start);
            m->set_end_time(end);
            m->set_minimum_bid(minimumBid);
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolTradeCondition* condition = m->add_conditions();
                bool fulfilled = false;
                if (!r.string(*condition->mutable_text()) || !readBool(r, fulfilled))
                    return 0;
                condition->set_fulfilled(fulfilled);
            }
        } else if (type == 1 || type == 2) { // highlighted items
            uint16_t count = 0;
            if (!r.u16(count))
                return 0;
            for (uint16_t i = 0; i < count; ++i) {
                ProtocolTradeHighlightItem* item = m->add_items();
                uint32_t amount = 0;
                if (!readTieredObject(r, item->mutable_object()) || !r.u32(amount))
                    return 0;
                item->set_count(amount);
            }
        } else if (type == 3) { // highlighted texts
            uint8_t groups = 0;
            if (!r.u8(groups))
                return 0;
            for (uint8_t g = 0; g < groups; ++g) {
                ProtocolTradeHighlightGroup* group = m->add_groups();
                uint8_t category = 0;
                uint16_t count = 0;
                if (!r.u8(category) || !r.string(*group->mutable_name()) || !r.u16(count))
                    return 0;
                group->set_category(category);
                for (uint16_t i = 0; i < count; ++i) {
                    ProtocolTradeHighlightEntry* entry = group->add_entries();
                    uint16_t id = 0;
                    if (!r.u16(id) || !r.string(*entry->mutable_text()))
                        return 0;
                    entry->set_entry_id(id);
                }
            }
        } else if (type == 4) { // summary
            uint16_t fee = 0;
            uint8_t count = 0;
            if (!r.u16(fee) || !r.u8(count))
                return 0;
            m->set_fee(fee);
            for (uint8_t i = 0; i < count; ++i) {
                if (!r.string(*m->add_problems()))
                    return 0;
            }
        }
        return r.i;
    }
    case 117: {
        auto* m = msg.MutableExtension(GameserverMessageGameEvent::gameserver_message_game_event);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_event_type(type);
        const auto small = [&] {
            uint8_t v = 0;
            if (!r.u8(v))
                return false;
            m->set_small_value(v);
            return true;
        };
        const auto value = [&] {
            uint16_t v = 0;
            if (!r.u16(v))
                return false;
            m->set_value(v);
            return true;
        };
        const auto large = [&] {
            uint32_t v = 0;
            if (!r.u32(v))
                return false;
            m->set_large_value(static_cast<int32_t>(v));
            return true;
        };
        bool ok = true;
        switch (type) {
        case 1: ok = small(); break; // simple event
        case 2: // achievement
        case 3: ok = r.string(*m->mutable_text()); break; // title
        case 4: // level
        case 11: // bounty task
        case 12: // weekly task
        case 15: // subarea
        case 16: ok = value(); break;
        case 5: ok = small() && value(); break; // skill
        case 6: // bestiary
        case 7: ok = value() && small(); break; // bosstiary
        case 8: ok = r.string(*m->mutable_text()) && small(); break; // quest
        case 9: ok = value() && r.string(*m->mutable_text()) && small(); break; // cosmetic
        case 10: ok = value() && r.string(*m->mutable_text()); break; // proficiency
        case 13: ok = large(); break; // spell unlocked
        case 14: ok = value() && large(); break; // leader monster
        default: break;
        }
        return ok ? r.i : 0;
    }
    case 95: {
        auto* m = msg.MutableExtension(GameserverMessageSkillWheel::gameserver_message_skill_wheel);
        uint32_t player = 0;
        bool canView = false;
        if (!r.u32(player) || !readBool(r, canView))
            return 0;
        m->set_player_id(player);
        m->set_can_view(canView);
        if (!canView)
            return r.i;
        uint8_t change = 0, vocation = 0;
        uint16_t points = 0, extra = 0;
        if (!r.u8(change) || !r.u8(vocation) || !r.u16(points) || !r.u16(extra))
            return 0;
        m->set_change_state(change);
        m->set_vocation(vocation);
        m->set_points(points);
        m->set_extra_points(extra);
        for (int i = 0; i < 36; ++i) {
            uint16_t slot = 0;
            if (!r.u16(slot))
                return 0;
            m->add_slot_points(slot);
        }
        uint16_t count = 0;
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolWheelScroll* scroll = m->add_scrolls();
            uint16_t object = 0;
            uint8_t scrollPoints = 0;
            if (!r.u16(object) || !r.u8(scrollPoints))
                return 0;
            scroll->set_object_id(object);
            scroll->set_points(scrollPoints);
        }
        bool monk = false;
        uint16_t hunting = 0;
        uint8_t mask = 0;
        if (!readBool(r, monk) || !r.u16(hunting) || !r.u8(mask))
            return 0;
        m->set_monk_quest(monk);
        m->set_hunting_task_points(hunting);
        // One bit per vessel, most significant first; a set bit carries the gem.
        for (int vessel = 0; vessel < 8; ++vessel) {
            if ((mask >> (7 - vessel)) & 1) {
                uint16_t gem = 0;
                if (!r.u16(gem))
                    return 0;
                m->add_equipped_gems(gem);
            } else {
                m->add_equipped_gems(-1);
            }
        }
        if (!r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolWheelGem* gem = m->add_gems();
            uint16_t id = 0;
            bool locked = false;
            uint8_t domain = 0, quality = 0, first = 0;
            if (!r.u16(id) || !readBool(r, locked) || !r.u8(domain) || !r.u8(quality) || !r.u8(first))
                return 0;
            gem->set_gem_id(id);
            gem->set_locked(locked);
            gem->set_domain(domain);
            gem->set_quality(quality);
            gem->set_first_modifier(first);
            if (quality == 1 || quality == 2) {
                uint8_t second = 0;
                if (!r.u8(second))
                    return 0;
                gem->set_second_modifier(second);
                if (quality == 2) {
                    uint8_t supreme = 0;
                    if (!r.u8(supreme))
                        return 0;
                    gem->set_supreme_modifier(supreme);
                }
            }
        }
        for (auto* list : { m->mutable_basic_grades(), m->mutable_supreme_grades() }) {
            uint8_t grades = 0;
            if (!r.u8(grades))
                return 0;
            for (uint8_t i = 0; i < grades; ++i) {
                ProtocolWheelGrade* grade = list->Add();
                uint8_t modifier = 0, value = 0;
                if (!r.u8(modifier) || !r.u8(value))
                    return 0;
                grade->set_modifier(modifier);
                grade->set_grade(value);
            }
        }
        return r.i;
    }
    case 135: {
        auto* m = msg.MutableExtension(GameserverMessageExaltationDialogRefresh::gameserver_message_exaltation_dialog_refresh);
        for (auto* groups : { m->mutable_fusion_groups(), m->mutable_convergence_fusion_groups() }) {
            uint16_t count = 0;
            if (!r.u16(count))
                return 0;
            for (uint16_t i = 0; i < count; ++i) {
                uint8_t objects = 0;
                if (!r.u8(objects) || !readForgeObjects(r, groups->Add()->mutable_objects(), objects))
                    return 0;
            }
        }
        uint8_t dust = 0;
        if (!readForgeTransfers(r, m->mutable_transfers()) || !readForgeTransfers(r, m->mutable_convergence_transfers()) || !r.u8(dust))
            return 0;
        m->set_dust_level(dust);
        return r.i;
    }
    case 221: {
        auto* m = msg.MutableExtension(GameserverMessageCyclopediaMapData::gameserver_message_cyclopedia_map_data);
        uint8_t type = 0;
        if (!r.u8(type))
            return 0;
        m->set_data_type(type);
        uint8_t u8v = 0, count = 0;
        uint16_t u16v = 0;
        bool flag = false;
        switch (type) {
        case 0: // minimap marker
            if (!readPosition(r, m->mutable_position()) || !r.u8(u8v) || !r.string(*m->mutable_text()))
                return 0;
            m->set_icon(u8v);
            break;
        case 1: { // discovery data
            uint16_t areas = 0;
            if (!r.u16(areas))
                return 0;
            for (uint16_t i = 0; i < areas; ++i) {
                ProtocolMapMainArea* area = m->add_main_areas();
                uint8_t status = 0, progress = 0;
                if (!r.u16(u16v) || !r.u8(status) || !r.u8(progress))
                    return 0;
                area->set_area_id(u16v);
                area->set_status(status);
                area->set_progress(progress);
            }
            if (!readAreaIds(r, m->mutable_discovered_areas()) || !readAreaIds(r, m->mutable_discoverable_areas()))
                return 0;
            break;
        }
        case 2: // active raid
            if (!readPosition(r, m->mutable_position()) || !r.u8(u8v))
                return 0;
            m->set_icon(u8v);
            break;
        case 3: // imminent raid (main area)
        case 10: // current area
        case 11: // exploring sub area
            if (!r.u16(u16v))
                return 0;
            m->set_area_id(u16v);
            break;
        case 4: // imminent raid (sub area)
            if (!r.u16(u16v) || !r.u8(u8v))
                return 0;
            m->set_area_id(u16v);
            m->set_icon(u8v);
            break;
        case 5: // discovery area
            if (!r.u16(u16v) || !readBool(r, flag) || !r.u8(u8v) || !r.u8(count))
                return 0;
            m->set_area_id(u16v);
            m->set_flag(flag);
            m->set_target(u8v);
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolMapPoint* point = m->add_points();
                uint8_t state = 0;
                if (!readPosition(r, point->mutable_position()) || !r.u8(state))
                    return 0;
                point->set_state(state);
            }
            break;
        case 6: // passage
            if (!readPosition(r, m->mutable_position()) || !r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (!r.u16(u16v))
                    return 0;
                m->add_race_ids(u16v);
            }
            break;
        case 7: { // sub area monsters
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolSubAreaMonsters* area = m->add_sub_area_monsters();
                uint8_t monsters = 0;
                if (!r.u16(u16v) || !r.u8(monsters))
                    return 0;
                area->set_sub_area_id(u16v);
                for (uint8_t k = 0; k < monsters; ++k) {
                    ProtocolSubAreaMonster* monster = area->add_monsters();
                    bool known = false;
                    uint8_t stage = 0;
                    if (!r.u16(u16v) || !readBool(r, known) || !r.u8(stage))
                        return 0;
                    monster->set_race_id(u16v);
                    monster->set_known(known);
                    monster->set_stage(stage);
                }
            }
            break;
        }
        case 8: // monster bestiary
            if (!r.u16(u16v) || !readBool(r, flag))
                return 0;
            m->set_area_id(u16v);
            m->set_flag(flag);
            break;
        case 9: { // donations
            uint64_t goal = 0;
            if (!r.u64(goal) || !r.u8(count))
                return 0;
            m->set_donation_goal(goal);
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolMapDonation* donation = m->add_donations();
                uint64_t donated = 0;
                if (!r.u16(u16v) || !readBool(r, flag) || !r.u64(donated))
                    return 0;
                donation->set_area_id(u16v);
                donation->set_improved_respawn(flag);
                donation->set_donated(donated);
            }
            break;
        }
        default:
            break;
        }
        return r.i;
    }
    case 248: {
        auto* m = msg.MutableExtension(GameserverMessageMarketDetail::gameserver_message_market_detail);
        if (!readTieredObject(r, m->mutable_object()))
            return 0;
        for (int i = 0; i < 26; ++i) {
            if (!r.string(*m->add_descriptions()))
                return 0;
        }
        for (auto* list : { m->mutable_buy_statistics(), m->mutable_sell_statistics() }) {
            uint8_t count = 0;
            if (!r.u8(count))
                return 0;
            for (uint8_t i = 0; i < count; ++i) {
                ProtocolMarketTradeStatistics* stats = list->Add();
                uint32_t transactions = 0;
                uint64_t total = 0, highest = 0, lowest = 0;
                if (!r.u32(transactions) || !r.u64(total) || !r.u64(highest) || !r.u64(lowest))
                    return 0;
                stats->set_transactions(transactions);
                stats->set_total_price(total);
                stats->set_highest_price(highest);
                stats->set_lowest_price(lowest);
            }
        }
        return r.i;
    }
    case 252: {
        auto* m = msg.MutableExtension(GameserverMessageStoreOffers::gameserver_message_store_offers);
        uint32_t redirect = 0;
        uint8_t sort = 0, filters = 0;
        if (!r.string(*m->mutable_category_name()) || !r.u32(redirect) || !r.u8(sort) || !r.u8(filters))
            return 0;
        m->set_redirect_id(redirect);
        m->set_sort_order(sort);
        for (uint8_t i = 0; i < filters; ++i) {
            if (!r.string(*m->add_menu_filters()))
                return 0;
        }
        uint16_t count = 0;
        if (!r.string(*m->mutable_collection_name()) || !readStrings16(r, m->mutable_disable_reasons()) || !r.u16(count))
            return 0;
        for (uint16_t i = 0; i < count; ++i) {
            ProtocolStoreOffer* offer = m->add_offers();
            uint8_t subOffers = 0;
            if (!r.string(*offer->mutable_name()) || !r.u8(subOffers))
                return 0;
            for (uint8_t k = 0; k < subOffers; ++k) {
                ProtocolStoreSubOffer* sub = offer->add_sub_offers();
                uint32_t id = 0, price = 0;
                uint16_t amount = 0;
                uint8_t coin = 0, state = 0;
                bool disabled = false, flag = false;
                if (!r.u32(id) || !r.u16(amount) || !r.u32(price) || !r.u8(coin) || !readBool(r, disabled))
                    return 0;
                sub->set_offer_id(id);
                sub->set_count(amount);
                sub->set_price(price);
                sub->set_coin_type(coin);
                sub->set_disabled(disabled);
                if (disabled) {
                    uint8_t reasons = 0;
                    if (!r.u8(reasons))
                        return 0;
                    for (uint8_t n = 0; n < reasons; ++n) {
                        uint16_t reason = 0;
                        if (!r.u16(reason))
                            return 0;
                        sub->add_disable_reasons(reason);
                    }
                }
                if (!r.u8(state))
                    return 0;
                sub->set_state(state);
                if (state == 2) { // sale
                    uint32_t until = 0, base = 0;
                    if (!r.u32(until) || !r.u32(base))
                        return 0;
                    sub->set_valid_until(until);
                    sub->set_base_price(base);
                }
                if (!readBool(r, flag))
                    return 0;
                sub->set_flag(flag);
            }
            uint8_t tryOn = 0;
            uint16_t popularity = 0, products = 0;
            uint32_t newUntil = 0;
            bool configurable = false;
            if (!readStoreVisual(r, offer->mutable_visual()) || !r.u8(tryOn) || !r.string(*offer->mutable_collection())
                || !r.u16(popularity) || !r.u32(newUntil) || !readBool(r, configurable) || !r.u16(products))
                return 0;
            offer->set_try_on_type(tryOn);
            offer->set_popularity(popularity);
            offer->set_new_until(newUntil);
            offer->set_configurable(configurable);
            for (uint16_t k = 0; k < products; ++k) {
                ProtocolStoreProduct* product = offer->add_products();
                if (!r.string(*product->mutable_name()) || !readStoreVisual(r, product->mutable_visual()))
                    return 0;
            }
        }
        if (m->category_name() == "Home") {
            uint8_t banners = 0, delay = 0;
            if (!r.u8(banners))
                return 0;
            for (uint8_t i = 0; i < banners; ++i) {
                ProtocolStoreBanner* banner = m->add_banners();
                if (!r.string(*banner->mutable_image()) || !readStoreOpenParameters(r, banner->mutable_parameters()))
                    return 0;
            }
            if (!r.u8(delay))
                return 0;
            m->set_banner_delay(delay);
        }
        if (m->category_name() == "Search") {
            bool tooMany = false;
            if (!readBool(r, tooMany))
                return 0;
            m->set_too_many_results(tooMany);
        }
        return r.i;
    }
    default:
        return 0;
    }
}

}
