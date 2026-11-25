#pragma once

/// Chat and the NPC dialog (host API v9). Replaces the tip's TChatChannelStorage /
/// TChatController / TNpcDialogController / TSpeechBubbleOverlayQuickItem for packs.
/// Host: client/tibia/mods/chat_abi.cpp.

#include "qtclient/abi/session_api.h"
#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An NPC keyword button (CipSoft TNpcKeywordButtonModel button). */
typedef struct QtClientNpcKeyword {
    /** CipSoft's button id (icon / order; 0 trade, 9 bye, ...). */
    int id;
    const char* text_utf8;
} QtClientNpcKeyword;

/** A conversation the pack drives in CipSoft's NPC dialog (the host's
 *  TExternalNpcConversation): the dialog shows `name` / `outfit` and the keyword buttons;
 *  typed lines and button clicks go to `say` instead of the server. */
typedef struct QtClientExternalNpcConversationV1 {
    size_t struct_size;
    const char* name_utf8;
    QtClientCreatureOutfitInfo outfit;
    /** Copied. */
    const QtClientNpcKeyword* keywords;
    size_t keyword_count;
    void* user;
    /** The player said `text` (typed or a keyword button). */
    void (*say)(void* user, const char* text_utf8);
    /** The player closed the dialog. */
    void (*close)(void* user);
    /** The conversation ended (closed, replaced, or the dialog went away). The last call with
     *  `user`. May be null. */
    void (*ended)(void* user);
} QtClientExternalNpcConversationV1;

enum {
    /** CipSoft TChatModeManager modes (QtClientChatApiV1.chat_mode). */
    QTCLIENT_CHAT_MODE_OFF = 0,
    QTCLIENT_CHAT_MODE_ON = 1,
    QTCLIENT_CHAT_MODE_TEMPORARY_ON = 2,
};

typedef struct QtClientChatApiV1 {
    size_t struct_size;

    /** Adds a line to the current session's NPCs channel (CipSoft addNpcEntry), coloured as
     *  `message_mode` (the protocol's talk mode, e.g. 11 NPC from). No-op between sessions. */
    void (*add_npc_entry)(int message_mode, const char* text_utf8, const char* speaker_utf8);
    /** CipSoft chat::consoleColor of a talk mode as 0xAARRGGBB. */
    uint32_t (*console_color_rgba)(int message_mode);

    /** CipSoft TChatController chat mode (QTCLIENT_CHAT_MODE_*); OFF without game window. */
    int (*chat_mode)(void);
    int (*subscribe_chat_mode_changed)(void (*fn)(void* user, int mode), void* user);
    void (*unsubscribe_chat_mode_changed)(int id);

    /** Opens (or replaces) an external conversation in the NPC dialog. Returns nonzero when
     *  the dialog took it. */
    int (*open_external_npc_conversation)(const QtClientExternalNpcConversationV1* conversation);
    /** Ends the external conversation (closes the dialog when it shows one). */
    void (*close_external_npc_conversation)(void);
    /** The NPC dialog holds an external conversation. */
    int (*npc_dialog_has_external_conversation)(void);
    /** The NPC dialog's talk partner names ("" none; sized out). */
    int (*npc_dialog_talk_partner_names)(char* buf, size_t buf_len);
    /** The NPC dialog's close button (CipSoft closeButtonPressed). */
    void (*npc_dialog_close)(void);

    /** Extra bottom inset (pixels) of CipSoft's status message overlay, so status toasts
     *  clear pack chrome (TSpeechBubbleOverlayQuickItem::setStatusBottomMargin). */
    void (*set_status_bottom_margin)(int px);
} QtClientChatApiV1;

#ifdef __cplusplus
}
#endif
