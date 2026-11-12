#pragma once

/// Session peels on the host-pushed pack ABI (nested under QtClientHostApiV1).

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QtClientMapPos {
    int x;
    int y;
    int z;
} QtClientMapPos;

typedef struct QtClientMapAwareRange {
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
} QtClientMapAwareRange;

typedef struct QtClientMapUseHit {
    int ok;
    uint16_t item_id;
    int stack_pos;
} QtClientMapUseHit;

typedef struct QtClientMapItemHit {
    QtClientMapPos pos;
    uint16_t type_id;
    int stack_pos;
} QtClientMapItemHit;

typedef struct QtClientInventorySlotInfo {
    int empty;
    uint16_t type_id;
    int stackable;
    uint16_t count;
    uint32_t charges;
} QtClientInventorySlotInfo;

typedef struct QtClientCreatureOutfitInfo {
    int exists;
    int is_object;
    uint16_t looktype;
    uint8_t head;
    uint8_t body;
    uint8_t legs;
    uint8_t feet;
    uint8_t addons;
} QtClientCreatureOutfitInfo;

typedef struct QtClientCreatureSnapshot {
    uint32_t id;
    char name[64];
    QtClientMapPos pos;
    uint8_t health_percent;
    uint8_t shield;
    uint8_t vocation;
    uint8_t creature_type;
    int is_player;
    int is_npc;
    int can_be_seen;
    int is_dead;
    int is_local_player;
    QtClientCreatureOutfitInfo outfit;
} QtClientCreatureSnapshot;

typedef struct QtClientRaceOutfitInfo {
    int exists;
    int is_object;
    uint16_t looktype;
    uint8_t head;
    uint8_t body;
    uint8_t legs;
    uint8_t feet;
    uint8_t addons;
    char name[64];
} QtClientRaceOutfitInfo;

typedef struct QtClientQuestInfo {
    int exists;
    uint16_t id;
    char name[128];
} QtClientQuestInfo;

typedef struct QtClientTeamFinderAssemble {
    uint16_t min_level;
    uint16_t max_level;
    uint8_t vocation_ids;
    uint16_t team_slots;
    uint16_t free_slots;
    int party_bool;
    uint32_t timestamp;
    uint8_t team_type;
    uint16_t boss_id;
    uint16_t hunt_type;
    uint16_t hunt_area;
    uint16_t quest_id;
} QtClientTeamFinderAssemble;

typedef void (*QtClientTravelPathAsyncCallback)(void* user, int ok);

typedef struct QtClientSessionApiV1 {
    int (*is_online)(void);
    int (*is_dead)(void);
    int (*is_scene_frozen)(void);
    void (*set_scene_frozen)(int frozen);

    int (*local_player_present)(void);
    QtClientMapPos (*local_player_map_pos)(void);
    /** Write UTF-8 name; returns bytes written (excl NUL), or -1. */
    int (*local_player_name)(char* buf, size_t buf_len);
    uint32_t (*local_player_id)(void);
    int (*local_player_level)(void);

    void (*local_player_stop_auto_walk)(void);
    int (*local_player_is_auto_walking)(void);
    /** path_mode: tip AutoWalkPathMode as int. */
    int (*local_player_auto_walk)(QtClientMapPos goal, int retry, int path_mode);

    void (*talk)(const char* text);
    void (*turn)(int direction);

    uint64_t (*millis)(void);
    uint8_t (*map_sea_floor)(void);
    uint8_t (*world_light_intensity)(void);
    uint32_t (*map_item_revision)(void);

    int (*cip_feature)(int feature);
    void (*enable_feature)(int feature);
    void (*set_custom_os)(int operating_system);

    /** --- Phase 2 session peel (additive) --- */

    int (*local_player_has_reached_main)(void);
    /** inventory_slot: tip `tibia::client::InventorySlot` as uint8_t. */
    uint16_t (*local_player_inventory_type_id)(uint8_t inventory_slot);
    void (*local_player_inventory_slot)(uint8_t inventory_slot, QtClientInventorySlotInfo* out);
    void (*open_local_player_inventory)(uint8_t inventory_slot);

    uint8_t (*local_player_bless_visual_state)(void);
    void (*local_player_set_information_channel)(uint32_t duration_ms);
    void (*local_player_clear_information_channel)(void);
    void (*local_player_set_soft_dead)(int soft_dead);

    uint32_t (*local_player_health)(void);
    uint32_t (*local_player_max_health)(void);
    uint32_t (*local_player_mana)(void);
    uint32_t (*local_player_max_mana)(void);
    uint16_t (*local_player_level_percent)(void);
    uint16_t (*local_player_stamina)(void);
    uint8_t (*local_player_vocation)(void);
    uint8_t (*local_player_shield)(void);
    uint64_t (*local_player_total_money)(void);
    uint32_t (*local_player_free_capacity)(void);
    uint32_t (*local_player_inventory_count)(uint16_t item_type_id);
    void (*local_player_outfit)(QtClientCreatureOutfitInfo* out);

    int (*item_name)(uint16_t item_id, char* buf, size_t buf_len);
    int (*item_is_container)(uint16_t item_id);
    int (*item_is_pickupable)(uint16_t item_id);
    int64_t (*item_mean_price)(uint16_t item_id);
    int64_t (*item_max_npc_sale_gold_price)(uint16_t item_id);

    int (*race_name)(uint32_t race_id, char* buf, size_t buf_len);
    int (*race_exists)(uint32_t race_id);
    int (*race_is_boss)(uint32_t race_id);
    void (*race_outfit)(uint32_t race_id, QtClientRaceOutfitInfo* out);
    int (*race_ids_by_name)(const char* name, uint32_t* out_ids, int max_ids);
    int (*all_race_ids)(uint32_t* out_ids, int max_ids);

    void (*quest_info)(uint16_t quest_id, QtClientQuestInfo* out);
    int (*all_quest_infos)(QtClientQuestInfo* out, int max_out);

    /** Top creature stack row at `pos` (tile missing → empty snapshot id 0). */
    void (*creature_at)(QtClientMapPos pos, QtClientCreatureSnapshot* out);
    /** Storage row by creature id (`id == 0` → empty snapshot). */
    void (*creature_at_id)(uint32_t creature_id, QtClientCreatureSnapshot* out);

    int (*spectators_near)(QtClientMapPos center, int multi_floor, int left, int right, int top, int bottom,
                           QtClientCreatureSnapshot* out, int max_out);
    void (*aware_range)(QtClientMapAwareRange* out);

    int (*item_hits)(QtClientMapPos pos, QtClientMapUseHit* out, int max_out);
    void (*top_use_hit)(QtClientMapPos pos, QtClientMapUseHit* out);
    /** Pass type_id_or_0 alone: single-type scan; type_id_or_0==0 scans all appearances (empty filter). */
    int (*find_items_in_range)(QtClientMapPos center, int range_x, int range_y, uint16_t type_id_or_0,
                               QtClientMapItemHit* out, int max_out);
    /** Multi-type filter (`type_count==0` → same as type_id_or_0==0: all item appearances). */
    int (*find_items_in_range_types)(QtClientMapPos center, int range_x, int range_y, const uint16_t* type_ids,
                                     int type_count, QtClientMapItemHit* out, int max_out);

    int (*is_sight_clear)(QtClientMapPos from, QtClientMapPos to);
    int (*is_walkable)(QtClientMapPos pos, int ignore_creatures);
    int (*can_travel_path)(QtClientMapPos from, QtClientMapPos to, int max_complexity);

    void (*can_travel_path_async)(QtClientMapPos from, QtClientMapPos to, QtClientTravelPathAsyncCallback cb,
                                  void* user);

    void (*safe_logout)(void);
    void (*request_bless)(void);
    void (*party_join)(uint32_t creature_id);
    void (*set_attacking_creature_id)(uint32_t creature_id);
    uint32_t (*attacking_creature_id)(void);

    int (*character_name)(char* buf, size_t buf_len);
    int (*is_switching_character)(void);

    void (*request_quick_loot_black_white_list)(uint8_t filter, uint16_t size, const uint16_t* listed_items,
                                               int listed_count);
    void (*open_container_quick_loot)(uint8_t action, uint8_t category, QtClientMapPos pos, uint16_t item_id,
                                      uint8_t stackpos, int use_main_as_fallback);

    void (*send_status_tracker_bestiary)(uint16_t race_id, int status);
    void (*request_bestiary_search)(uint16_t race_id);

    void (*send_friend_system_payload)(const uint8_t* bytes, size_t len);
    void (*open_private_channel)(const char* receiver_utf8);

    void (*request_team_finder_list)(void);
    void (*request_team_finder_leader)(void);
    void (*join_team_finder)(uint32_t leader_id);
    void (*leave_team_finder)(uint32_t leader_id);
    void (*create_team_finder)(const QtClientTeamFinderAssemble* assemble);
    void (*reset_team_finder_leader)(void);
    void (*update_team_finder_member)(uint32_t member_id, uint8_t status);

    void (*use_inventory_item_on_creature)(uint16_t item_id, uint32_t creature_id);
    void (*use_map_object)(QtClientMapPos pos, uint16_t item_id, int stack_pos);
    void (*use_inventory_item_with_map_object)(uint16_t item_id, QtClientMapPos to_pos, uint16_t to_item_id,
                                                 int to_stack_pos);

    void (*clear_effect_variants)(void);
    void (*register_effect_variant)(uint16_t virtual_id, uint16_t base_id, const char* shader_utf8,
                                   int under_creatures);
    void (*register_attached_effect_by_thing)(uint16_t id, const char* note_utf8, uint16_t base_id, uint8_t category);
    void (*remove_attached_effect)(uint16_t id);

    int (*creature_type_is_summon)(uint8_t creature_type);

    /** Returns stable Item* while the session runs; superseded by the next call on this thread. */
    QtClientHandle (*find_player_item)(uint32_t item_id, int sub_type);

    int (*set_item_thing_shape)(uint16_t item_id, const char* shape_name_utf8);

    /** CIP pathfinder avoid set (SnapBack / levitate mountain face). */
    void (*set_path_avoid)(QtClientMapPos pos, int avoid);
    void (*clear_path_avoids)(void);
} QtClientSessionApiV1;

#ifdef __cplusplus
}
#endif
