#pragma once

/// Extension backends as C vtables (host API v9; replace set_*_backend(void* tip backend)).
/// The host keeps its own adapters (client/tibia/mods/backend_adapters.cpp) over these and
/// binds them itself where CipSoft's UI reads them (quest tracker, quest log, info dialogs,
/// button bar); the pack only sets the vtables and says when its data changed.
///
/// Every set_* copies the vtable; null removes the backend. `user` and the functions must
/// stay valid until the backend is replaced or removed (remove them before the pack's objects
/// go, at the latest on QCoreApplication::aboutToQuit). All calls on the GUI thread.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extra data for CipSoft's quest tracker rows and the quest log's selected mission. The
 * host hands the JSON objects to QML untouched (the tracker's `extension` role, the quest
 * log's `missionExtension` property) and passes QML's activations back; it reads no key.
 * String results use the sized out convention (the host calls again with a larger buffer
 * when the return is >= buf_len); -1 or "{}" mean no data.
 */
typedef struct QtClientMissionBackendV1 {
    size_t struct_size;
    void* user;
    /** `missions_json`: the tracked missions as a JSON array of
     *  {"questFlagId": int, "questLineId": int (0: none sent), "name": string,
     *  "completed": bool}. Writes a JSON object keyed by quest flag id (decimal string) whose
     *  values are each row's extra data object; rows without data are left out. */
    int (*tracker_extensions_json)(void* user, const char* missions_json, char* buf, size_t buf_len);
    /** The quest log's extra data (a JSON object) for its selected quest line, quest flag
     *  (0: none) and mission name. */
    int (*quest_log_extension_json)(void* user, int quest_line_id, int quest_flag_id, const char* mission_utf8,
                                    char* buf, size_t buf_len);
    /** The quest log selected `quest_line_id` (> 0). May be null. */
    void (*quest_line_selected)(void* user, int quest_line_id);
    /** QML activated the extra data of a tracker row or of the quest log's selection;
     *  `choice_utf8` is what the QML passed ("" when nothing). */
    void (*activate)(void* user, int quest_line_id, int quest_flag_id, const char* mission_utf8,
                     const char* choice_utf8);
} QtClientMissionBackendV1;

/** Highlights of CipSoft's control button bar buttons. */
typedef struct QtClientAttentionBackendV1 {
    size_t struct_size;
    void* user;
    /** Nonzero: the reward wall button is highlighted. */
    int (*reward_wall)(void* user);
    /** Nonzero: the weapon proficiency button is highlighted. */
    int (*proficiency)(void* user);
} QtClientAttentionBackendV1;

/** The place the player is in, for CipSoft's info dialogs (the Cyclopedia map's default
 *  selection). */
typedef struct QtClientInstanceBackendV1 {
    size_t struct_size;
    void* user;
    /** The map.dat subarea id of the player's place, 0 when none. */
    int (*subarea_id)(void* user);
} QtClientInstanceBackendV1;

typedef struct QtClientBackendApiV1 {
    size_t struct_size;

    void (*set_mission_backend)(const QtClientMissionBackendV1* backend);
    /** The mission data changed: the tracker and the quest log ask again. */
    void (*mission_backend_changed)(void);

    void (*set_attention_backend)(const QtClientAttentionBackendV1* backend);
    /** A highlight changed: `signal_id_utf8` is "rewardWall" or "proficiency" (the
     *  backend function's name), `active` its new value. */
    void (*attention_changed)(const char* signal_id_utf8, int active);

    void (*set_instance_backend)(const QtClientInstanceBackendV1* backend);
} QtClientBackendApiV1;

#ifdef __cplusplus
}
#endif
