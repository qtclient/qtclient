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

/** Team Finder listing parameters (host API v7+). `activity_kind`: 0 none, 1 boss, 2 hunt,
 *  3 quest, 4 other. 0xffff team / free slots: not set. `vocation_mask`: bit n = vocation n. */
typedef struct QtClientTeamFinderParams {
    uint16_t min_level;
    uint16_t max_level;
    uint32_t vocation_mask;
    uint16_t team_slots;
    uint16_t free_slots;
    uint32_t start_time;
    uint8_t activity_kind;
    uint16_t boss_id;
    uint16_t hunt_type;
    uint16_t hunt_area;
    uint32_t quest_id;
    int include_party;
} QtClientTeamFinderParams;

/** A roster row of the own team (host API v7+). `status`: 0 not in team, 1 applied,
 *  2 invited, 3 accepted, 4 rejected. */
typedef struct QtClientTeamFinderMember {
    uint32_t character_id;
    char name[64];
    uint16_t level;
    uint8_t vocation;
    uint32_t status;
} QtClientTeamFinderMember;

/** A listed team (host API v7+); `status` is the own application state there. */
typedef struct QtClientTeamFinderTeam {
    uint32_t team_id;
    char leader_name[64];
    QtClientTeamFinderParams params;
    uint32_t status;
} QtClientTeamFinderTeam;

/** A stored field of the session's map (host API v8+). `walkable`: creatures block. */
typedef struct QtClientMapFieldInfo {
    QtClientMapPos pos;
    int ground_speed;
    uint8_t walkable;
    uint8_t pathable;
} QtClientMapFieldInfo;

/** An area or subarea of the Cyclopedia map's static data, map.dat (host API v8+).
 *  Names are UTF-8, for display only; ids are the keys. `label` 0,0,0: none.
 *  `mask_*`: the fields a subarea's mask file covers (has_mask_file 0: none). */
typedef struct QtClientStaticMapArea {
    uint16_t id;
    uint16_t area_id;
    int is_subarea;
    char name[96];
    char alias[96];
    QtClientMapPos label;
    int has_mask_file;
    QtClientMapPos mask_origin;
    int mask_width;
    int mask_height;
} QtClientStaticMapArea;

typedef struct QtClientSessionApiV1 {
    int (*is_online)(void);
    int (*is_dead)(void);

    int (*local_player_present)(void);
    QtClientMapPos (*local_player_map_pos)(void);
    /** Write UTF-8 name; returns bytes written (excl NUL), or -1. */
    int (*local_player_name)(char* buf, size_t buf_len);
    uint32_t (*local_player_id)(void);
    int (*local_player_level)(void);

    void (*local_player_stop_auto_walk)(void);
    int (*local_player_is_auto_walking)(void);
    /** path_mode: QTCLIENT_AUTOWALK_PATH_* (the walk uses CipSoft's path finder whatever the mode). */
    int (*local_player_auto_walk)(QtClientMapPos goal, int retry, int path_mode);

    void (*talk)(const char* text);
    void (*turn)(int direction);

    uint64_t (*millis)(void);
    uint8_t (*map_sea_floor)(void);
    uint8_t (*world_light_intensity)(void);
    uint32_t (*map_item_revision)(void);


    /** --- Phase 2 session peel (additive) --- */

    int (*local_player_has_reached_main)(void);
    /** inventory_slot: tip `tibia::client::InventorySlot` as uint8_t. */
    uint16_t (*local_player_inventory_type_id)(uint8_t inventory_slot);
    void (*local_player_inventory_slot)(uint8_t inventory_slot, QtClientInventorySlotInfo* out);
    void (*open_local_player_inventory)(uint8_t inventory_slot);

    uint8_t (*local_player_bless_visual_state)(void);

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

    int (*creature_type_is_summon)(uint8_t creature_type);

    /** Returns stable Item* while the session runs; superseded by the next call on this thread. */
    QtClientHandle (*find_player_item)(uint32_t item_id, int sub_type);

    /** QTCLIENT_DIRECTION_* the local player faces, or QTCLIENT_DIRECTION_INVALID offline. */
    int (*local_player_direction)(void);

    /** Hold a walk direction (QTCLIENT_DIRECTION_*, diagonals too): the player steps
     *  now and keeps stepping like a held arrow key until released or another
     *  direction is held (touch joysticks). */
    void (*local_player_hold_direction)(int direction);
    void (*local_player_release_direction)(void);

    /** --- Current game session objects (host API v5+; read only when api_version >= 5) ---
     *  Opaque tip pointers into the game client's current session (one per login, replay or
     *  tutorial); null between sessions. Resolve them at call time and never keep them: the
     *  next login builds new ones. Rebind signal connections on notify->subscribe_session_changed. */
    /** container::TContainerStorage* (<qtclient/container/containerciptypes.h>). */
    void* (*container_storage)(void);
    /** container::TInventoryContainer* (<qtclient/container/containerciptypes.h>). */
    void* (*inventory_container)(void);
    /** creatures::TCreatureStorage* (<qtclient/creatures/creaturestorage.h>). */
    void* (*creature_storage)(void);
    /** game::TCooldownStorage* (<qtclient/cooldown.h>). */
    void* (*cooldown_storage)(void);
    /** quickloot::TQuickLootBlackWhitelistStorage* (<qtclient/quickloot/quicklootblackwhiteliststorage.h>). */
    void* (*quick_loot_black_white_list_storage)(void);
    /** Non-zero once the session read the character's configuration (quick-loot and analyser
     *  lists hold the character's), zero before that and after the game ended. */
    int (*character_configuration_loaded)(void);

    /** --- Team Finder (host API v7+; read only when api_version >= 7) ---
     *  The current session's CipSoft Team Finder storage, as the Social window sees it.
     *  team_finder_params: the own team's listing; returns nonzero when it has one.
     *  team_finder_members / team_finder_teams: fill up to `cap` rows, return the total.
     *  notify->subscribe_team_finder_changed runs when any of them changes. */
    int (*team_finder_params)(QtClientTeamFinderParams* out);
    size_t (*team_finder_members)(QtClientTeamFinderMember* out, size_t cap);
    size_t (*team_finder_teams)(QtClientTeamFinderTeam* out, size_t cap);

    /** --- Creature draw filter (host API v8+; read only when api_version >= 8) ---
     *  hidden != 0 keeps creature `creature_id` of the current session off the map
     *  (outfit, decorations, name and bars); 0 shows it again. No effect for an unknown id;
     *  the flag goes with the session's creature. */
    void (*set_creature_hidden)(uint32_t creature_id, int hidden);

    /** --- Map data (host API v8+; read only when api_version >= 8) ---
     *  stored_fields_on_floor: the session map's stored fields on floor `z`; fills up to
     *  `cap`, returns the total (0 between sessions). GUI thread.
     *  is_aware_of_position: the position is in the session's aware range.
     *  waypoint_costs_snapshot: the minimap's waypoint costs (CipSoft TWaypointCostMap:
     *  0..250 walkable, 0xfe unknown, 0xff blocked), taken on the GUI thread; one worker
     *  thread may call waypoint_cost_at on it afterwards; release it once.
     *  static_map_*: map.dat's areas and subareas (client-wide, not per session).
     *  static_map_area: `subarea` nonzero asks for a subarea; returns nonzero when known.
     *  static_map_area_ids / static_map_subareas_of_area: ascending ids, up to `cap`,
     *  return the total. */
    size_t (*stored_fields_on_floor)(int z, QtClientMapFieldInfo* out, size_t cap);
    int (*is_aware_of_position)(QtClientMapPos pos);
    QtClientHandle (*waypoint_costs_snapshot)(void);
    int (*waypoint_cost_at)(QtClientHandle costs, int x, int y, int z);
    void (*waypoint_costs_release)(QtClientHandle costs);
    uint16_t (*static_map_subarea_at)(QtClientMapPos pos);
    int (*static_map_area)(uint16_t id, int subarea, QtClientStaticMapArea* out);
    size_t (*static_map_area_ids)(int subareas, uint16_t* out, size_t cap);
    size_t (*static_map_subareas_of_area)(uint16_t area_id, uint16_t* out, size_t cap);
} QtClientSessionApiV1;

#ifdef __cplusplus
}
#endif
