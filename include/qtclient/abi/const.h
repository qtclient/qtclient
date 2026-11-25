#pragma once

/// Stable numeric constants for the host-pushed pack ABI (mirrors tip enums).

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* CIP chat message mode (the protocol's) */
    QTCLIENT_MESSAGE_FAILURE = 21,
    QTCLIENT_MESSAGE_STATUS = 30,
    QTCLIENT_MESSAGE_TRANSACTION = 51,

    QTCLIENT_SHIELD_NONE = 0,
    QTCLIENT_SHIELD_WHITE_YELLOW = 1,

    QTCLIENT_INVENTORY_SLOT_HEAD = 1,
    QTCLIENT_INVENTORY_SLOT_NECKLACE = 2,
    QTCLIENT_INVENTORY_SLOT_BACKPACK = 3,
    QTCLIENT_INVENTORY_SLOT_ARMOR = 4,
    QTCLIENT_INVENTORY_SLOT_RIGHT = 5,
    QTCLIENT_INVENTORY_SLOT_LEFT = 6,
    QTCLIENT_INVENTORY_SLOT_LEGS = 7,
    QTCLIENT_INVENTORY_SLOT_FEET = 8,
    QTCLIENT_INVENTORY_SLOT_RING = 9,
    QTCLIENT_INVENTORY_SLOT_AMMO = 10,
    QTCLIENT_INVENTORY_SLOT_PURSE = 11,

    QTCLIENT_DIRECTION_NORTH = 0,
    QTCLIENT_DIRECTION_EAST = 1,
    QTCLIENT_DIRECTION_SOUTH = 2,
    QTCLIENT_DIRECTION_WEST = 3,
    QTCLIENT_DIRECTION_NORTH_EAST = 4,
    QTCLIENT_DIRECTION_SOUTH_EAST = 5,
    QTCLIENT_DIRECTION_SOUTH_WEST = 6,
    QTCLIENT_DIRECTION_NORTH_WEST = 7,
    QTCLIENT_DIRECTION_INVALID = 8,

    QTCLIENT_AUTOWALK_PATH_MINIMAP = 0,
    QTCLIENT_AUTOWALK_PATH_CYCLOPEDIA = 1,
    QTCLIENT_AUTOWALK_PATH_TRAVEL = 2,

    /** set_walk_unreachable_hook reasons (CipSoft's planner results). */
    QTCLIENT_WALK_UNREACHABLE_NO_WAY = 1,
    QTCLIENT_WALK_UNREACHABLE_OUT_OF_RANGE = 2,
    QTCLIENT_WALK_UNREACHABLE_GO_DOWNSTAIRS = 3,
    QTCLIENT_WALK_UNREACHABLE_GO_UPSTAIRS = 4,

    QTCLIENT_THING_CATEGORY_EFFECT = 2,

    /** set_creature_decoration_hook layers. */
    QTCLIENT_CREATURE_LAYER_UNDER = 0, /* drawn before the creature's outfit */
    QTCLIENT_CREATURE_LAYER_OVER = 1,  /* drawn after it */

    /** QtClientCreatureDecoration.phase: the appearance's own animation on the frame clock. */
    QTCLIENT_DECORATION_PHASE_ANIMATED = -1,

    /* --- Host API v9 --- */

    /** The protocol's NPC talk mode (chat->add_npc_entry, console_color_rgba). */
    QTCLIENT_MESSAGE_NPC_FROM = 11,

    /** Message box buttons (CipSoft TMessageDialogButton::EMessageDialogButtonIdentifier).
     *  OK / YES accept (CipSoft acceptClicked), CANCEL / NO discard (discardClicked). */
    QTCLIENT_MESSAGE_BUTTON_OK = 0,
    QTCLIENT_MESSAGE_BUTTON_CANCEL = 1,
    QTCLIENT_MESSAGE_BUTTON_YES = 2,
    QTCLIENT_MESSAGE_BUTTON_NO = 3,
    QTCLIENT_MESSAGE_BUTTON_USER_DEFINED = 4,

    /** Message box text (CipSoft EMessageDialogTextFormat). */
    QTCLIENT_MESSAGE_TEXT_PLAIN = 0,
    QTCLIENT_MESSAGE_TEXT_STYLED = 1,
    QTCLIENT_MESSAGE_TEXT_RICH = 2,
    QTCLIENT_MESSAGE_TEXT_MARKDOWN = 3,

    /** Quick-loot lists (CipSoft TQuickLootBlackWhitelistStorage::EListType). */
    QTCLIENT_QUICK_LOOT_BLACKLIST = 0,
    QTCLIENT_QUICK_LOOT_WHITELIST = 1,

    /** Team Finder member / application status (QtClientTeamFinderMember.status,
     *  QtClientTeamFinderTeam.status, update_team_finder_member). */
    QTCLIENT_TEAM_FINDER_STATUS_NONE = 0,
    QTCLIENT_TEAM_FINDER_STATUS_APPLIED = 1,
    QTCLIENT_TEAM_FINDER_STATUS_INVITED = 2,
    QTCLIENT_TEAM_FINDER_STATUS_ACCEPTED = 3,
    QTCLIENT_TEAM_FINDER_STATUS_REJECTED = 4,

    /** Team Finder activity (QtClientTeamFinderParams.activity_kind; team_type of
     *  QtClientTeamFinderAssemble stays the protocol's value). */
    QTCLIENT_TEAM_FINDER_ACTIVITY_NONE = 0,
    QTCLIENT_TEAM_FINDER_ACTIVITY_BOSS = 1,
    QTCLIENT_TEAM_FINDER_ACTIVITY_HUNT = 2,
    QTCLIENT_TEAM_FINDER_ACTIVITY_QUEST = 3,
    QTCLIENT_TEAM_FINDER_ACTIVITY_OTHER = 4,
};

#ifdef __cplusplus
}
#endif
