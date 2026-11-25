#pragma once

/// Cooldowns and static game data (host API v9). Replaces the tip's TCooldownStorage /
/// TClockService / TSpellInfoStorage / cyclopedia charm table / outfitpalette /
/// client::colorFromEightBit for packs. Host: client/tibia/mods/gamedata_abi.cpp.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------------------------
 * Cooldowns: the current game session's CipSoft TCooldownStorage. Times are milliseconds of
 * the process clock (now_ms); every value is 0 between sessions.
 * ---------------------------------------------------------------------------------------- */

/** CipSoft TCooldown at a time. */
typedef struct QtClientCooldown {
    int64_t remaining_ms; /* 0 when over */
    int64_t duration_ms;
    int64_t end_ms;       /* process clock; 0 when none ran */
} QtClientCooldown;

typedef struct QtClientSpellCooldown {
    uint16_t spell_id;
    QtClientCooldown cooldown;
} QtClientSpellCooldown;

enum {
    /** QtClientCooldownApiV1.subscribe_changed `what`. */
    QTCLIENT_COOLDOWN_SPELL_OR_GROUP = 1, /* spellOrSpellGroupCooldownChanged */
    QTCLIENT_COOLDOWN_MULTI_USE = 2,      /* multiUseCooldownChanged */
    QTCLIENT_COOLDOWN_SESSION = 3,        /* a new session (or none): everything changed */
};

typedef struct QtClientCooldownApiV1 {
    size_t struct_size;
    /** CipSoft's process clock (TClockService::process()->nowMillis()). Any thread. */
    int64_t (*now_ms)(void);
    /** The spell's own cooldown at `now_ms`. Returns nonzero when one is known. */
    int (*spell_cooldown)(uint16_t spell_id, int64_t now_ms, QtClientCooldown* out);
    /** A spell group's cooldown (the protocol's group id: 1 attack, 2 healing, ...). */
    int (*group_cooldown)(uint8_t group_id, int64_t now_ms, QtClientCooldown* out);
    /** The multi-use (object use) cooldown. */
    int (*multi_use_cooldown)(int64_t now_ms, QtClientCooldown* out);
    /** Every spell with a cooldown entry at `now_ms` (running or over), ascending ids (list). */
    size_t (*spell_cooldowns)(int64_t now_ms, QtClientSpellCooldown* out, size_t cap);
    /** `fn` runs on each change (QTCLIENT_COOLDOWN_*), across sessions. */
    int (*subscribe_changed)(void (*fn)(void* user, int what), void* user);
    void (*unsubscribe_changed)(int id);
} QtClientCooldownApiV1;

/* ------------------------------------------------------------------------------------------
 * Static game data (client-wide, not per session).
 * ---------------------------------------------------------------------------------------- */

enum {
    /** QtClientSpellInfo.type (CipSoft TSpellInfo::EType). */
    QTCLIENT_SPELL_INSTANT = 1,
    QTCLIENT_SPELL_CREATABLE_RUNE = 2,
    QTCLIENT_SPELL_RUNE = 3,

    /** QtClientSpellInfo.flags. */
    QTCLIENT_SPELL_FLAG_PREMIUM = 1 << 0,
    QTCLIENT_SPELL_FLAG_AGGRESSIVE = 1 << 1,
    QTCLIENT_SPELL_FLAG_IS_RUNE = 1 << 2,
    QTCLIENT_SPELL_FLAG_RUNE_CREATABLE = 1 << 3,
    QTCLIENT_SPELL_FLAG_AUTO_UNLOCK = 1 << 4,
    QTCLIENT_SPELL_FLAG_CROSSHAIR_TARGET = 1 << 5,
    QTCLIENT_SPELL_FLAG_AIM_AT_TARGET = 1 << 6,
};

/** One spell of CipSoft's spell table (TSpellInfo, with the server's modifications). */
typedef struct QtClientSpellInfo {
    uint16_t id;
    /** The spell icon index ("image://spell-icons-20x20/<index>"); -1 none. */
    int16_t icon_index;
    /** QTCLIENT_SPELL_INSTANT / _CREATABLE_RUNE / _RUNE. */
    uint8_t type;
    /** The protocol's spell group ids (1 attack, 2 healing, 3 support, ...; 0 none). */
    uint8_t group_primary;
    uint8_t group_secondary;
    uint8_t reserved0;
    /** QTCLIENT_SPELL_FLAG_*. */
    uint32_t flags;
    uint16_t minimum_level;
    uint16_t minimum_magic_level;
    uint16_t mana;
    uint16_t soul;
    uint32_t cooldown_self_ms;
    uint32_t cooldown_primary_group_ms;
    uint32_t cooldown_secondary_group_ms;
    /** Runes: the rune object (0 for instant spells) and the amount made. */
    uint16_t rune_object_id;
    uint16_t rune_amount;
    char name[64];
    /** The words without parameters ("exura sio"). */
    char words[64];
    /** CipSoft's parameter hints ("" when the spell takes none). */
    char parameter_hints[64];
    /** The casting vocations' names, comma-separated as spells.json spells them. */
    char vocations[192];
} QtClientSpellInfo;

/** A charm of CipSoft's charm table (cyclopedia TCharmInfo). */
typedef struct QtClientCharmInfo {
    int id;
    int kind;
    int cost[3];
    /** Effect percentage per grade, x100. */
    int percent[3];
    char name[64];
    /** The description with the asked grade's percentage (CipSoft charmDescription). */
    char description[512];
    /** The charm image's QML source (CipSoft charmImageSource). */
    char image_source[256];
} QtClientCharmInfo;

typedef struct QtClientGameDataApiV1 {
    size_t struct_size;

    /** Every spell, ascending ids (list). */
    size_t (*spells)(QtClientSpellInfo* out, size_t cap);
    /** Returns nonzero when known. */
    int (*spell)(uint16_t spell_id, QtClientSpellInfo* out);
    /** The spell a typed text casts (CipSoft spellOfText: the words, parameters allowed). */
    int (*spell_of_text)(const char* text_utf8, QtClientSpellInfo* out);
    /** The spell a rune object casts (CipSoft runeSpellOfObject). */
    int (*rune_spell_of_object)(uint16_t object_type_id, QtClientSpellInfo* out);
    /** The spell's icon image URL (CipSoft TSpellInfoStorage::iconUrl; sized out). */
    int (*spell_icon_url)(uint16_t spell_id, char* buf, size_t buf_len);

    /** Charm `charm_id` with its description for `grade` (1..3). Nonzero when known. */
    int (*charm_info)(int charm_id, int grade, QtClientCharmInfo* out);

    /** CipSoft's outfit colour palette entry (0..132) as 0xAARRGGBB; 0 out of range. Any
     *  thread. */
    uint32_t (*outfit_color_rgba)(int palette_index);
    /** CipSoft's 8-bit colour (light colours, text colours) as 0xAARRGGBB. Any thread. */
    uint32_t (*eight_bit_color_rgba)(uint8_t color);
} QtClientGameDataApiV1;

#ifdef __cplusplus
}
#endif
