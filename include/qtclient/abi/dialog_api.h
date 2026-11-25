#pragma once

/// Dialogs (host API v9): a pack's dialog on CipSoft's dialog stack, message boxes, the
/// stack's state and the host's own CipSoft dialogs. Replaces the tip's TDialogController /
/// TDialog / TDialogStackController / TMessageDialogController for packs.
/// Host: client/tibia/mods/dialog_abi.cpp.
///
/// A pack dialog's controller is a plain pack Qt object (pack moc). The host makes the
/// dialog's item in the game window's dialog placeholder with `controller` as its
/// `controller` initial property (CipSoft's TDialogControllerBase default) and puts it on the
/// dialog stack like CipSoft's dialogs (one shown at a time, mouse shield, Escape).
/// The controller should declare CipSoft TDialogControllerBase's signals `abort()` and
/// `dialogClosed()` when its QML calls or binds them; the host emits `dialogClosed()` on it
/// (by name, when present) each time the dialog closes, as CipSoft's TDialog does.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A dialog the host made (create / message_box). Host-opaque; valid until destroy (pack
 *  dialogs) or until its `closed` callback returned (message boxes). */
typedef struct QtClientDialog QtClientDialog;

/** CipSoft TDialogHooks (TDialogControllerBase vtable slots 19 to 25) for a pack dialog.
 *  Every function may be null. */
typedef struct QtClientDialogHooksV1 {
    size_t struct_size;
    void* user;
    /** The QML item was made (first show, or create). */
    void (*after_create)(void* user, QtClientQQuickItemPtr item);
    /** The item is about to be destroyed (destroy, or the host's teardown). */
    void (*before_destroy)(void* user);
    void (*before_show)(void* user);
    void (*after_show)(void* user);
    void (*before_hide)(void* user);
    void (*after_hide)(void* user);
    /** The dialog closed (close, Escape on a shielded dialog, leaving the world). */
    void (*closed)(void* user);
} QtClientDialogHooksV1;

enum {
    /** QtClientDialogDescV1.flags: the dialog blocks clicks itself (TibiaDialog's
     *  personalMouseShield) instead of a mouse shield over the game window (CipSoft
     *  hasPersonalMouseShield). */
    QTCLIENT_DIALOG_FLAG_PERSONAL_MOUSE_SHIELD = 1 << 0,
};

typedef struct QtClientDialogDescV1 {
    size_t struct_size;
    /** A file of the host's qmlcomponents module ("BlessingsDialog.qml") or a full URL: a
     *  pack module file ("qrc:/qt/qml/<module>/X.qml", resolved through the pack's FS QML and
     *  overrides) or "file:". Copied. */
    const char* qml_url;
    /** The dialog's controller, set as the item's `controller` initial property. Must outlive
     *  the dialog (destroy it before deleting the controller). */
    QtClientQObjectPtr controller;
    /** Further initial properties as a JSON object (UTF-8; may be null). Copied. */
    const char* initial_properties_json;
    /** QTCLIENT_DIALOG_FLAG_*. */
    int flags;
    /** Copied; `hooks.user` and the functions must stay valid until destroy returns. */
    QtClientDialogHooksV1 hooks;
} QtClientDialogDescV1;

/** A message box button (CipSoft TMessageDialogButton). */
typedef struct QtClientMessageButtonV1 {
    const char* label_utf8;
    /** QTCLIENT_MESSAGE_BUTTON_* (CipSoft EMessageDialogButtonIdentifier). */
    int id;
    /** QKeySequence portable text ("Return", "Esc"); null / "" for none. */
    const char* shortcut_utf8;
} QtClientMessageButtonV1;

/** A CipSoft message dialog (MessageDialog.qml, TMessageDialogController). */
typedef struct QtClientMessageBoxDescV1 {
    size_t struct_size;
    const char* title_utf8;
    const char* text_utf8;
    /** QTCLIENT_MESSAGE_TEXT_* (CipSoft EMessageDialogTextFormat). */
    int text_format;
    /** Copied. button_count 0: CipSoft's default buttons (Ok). */
    const QtClientMessageButtonV1* buttons;
    size_t button_count;
    /** The check box label; null / "" for none. */
    const char* checkbox_utf8;
    void* user;
    /** A button was clicked (`button_id` QTCLIENT_MESSAGE_BUTTON_*, `checked` the check box);
     *  the box then closes. May be null. */
    void (*clicked)(void* user, int button_id, int checked);
    /** The box closed (after `clicked`, through close, or leaving the world). The last call
     *  with `user`. May be null. */
    void (*closed)(void* user);
} QtClientMessageBoxDescV1;

/** Host dialogs (CipSoft dialogs the host owns) the pack may look at. */
enum {
    QTCLIENT_HOST_DIALOG_BLESSINGS = 1,
    QTCLIENT_HOST_DIALOG_NPC = 2,
    QTCLIENT_HOST_DIALOG_IMBUING = 3,
    QTCLIENT_HOST_DIALOG_CREATE_CHARACTER = 4,
    QTCLIENT_HOST_DIALOG_CHARACTER_INFO = 5,
    QTCLIENT_HOST_DIALOG_BOSS_DIFFICULTY_SELECTION = 6,
};

typedef struct QtClientDialogApiV1 {
    size_t struct_size;

    /* --- pack dialogs --- */

    /** Makes a dialog (not shown; the item is made on first show). Null when the game window
     *  is not there yet. */
    QtClientDialog* (*create)(const QtClientDialogDescV1* desc);
    /** Closes and removes the dialog and its item (before_destroy runs). The handle is then
     *  invalid. Also for a message box (no `clicked`; `closed` runs). */
    void (*destroy)(QtClientDialog* dialog);
    /** CipSoft TDialog show / hide (stays stacked) / close (leaves the stack). */
    void (*show)(QtClientDialog* dialog);
    void (*hide)(QtClientDialog* dialog);
    void (*close)(QtClientDialog* dialog);
    int (*is_shown)(QtClientDialog* dialog);
    int (*is_in_stack)(QtClientDialog* dialog);
    /** The dialog's QML item; null before it was made. */
    QtClientQQuickItemPtr (*item)(QtClientDialog* dialog);
    /** CipSoft TDialog::setMouseShieldVisible. */
    void (*set_mouse_shield_visible)(QtClientDialog* dialog, int visible);

    /* --- message boxes --- */

    /** Shows a message box now. Returns its handle (close with destroy), null when the game
     *  window is not there. */
    QtClientDialog* (*message_box)(const QtClientMessageBoxDescV1* desc);

    /* --- the dialog stack --- */

    /** A dialog is stacked (CipSoft isAnyDialogStacked). */
    int (*any_stacked)(void);
    /** A stacked dialog shields the game window (no personal mouse shield). */
    int (*any_shielding_stacked)(void);
    /** Escape: closes the topmost shown dialog with a personal mouse shield; 0 when none. */
    int (*close_top_shielded)(void);
    /** `fn` runs when the stack shows or hides its top dialog (CipSoft
     *  TDialogStackController::dialogVisible). */
    int (*subscribe_stack_visible)(void (*fn)(void* user, int visible), void* user);
    void (*unsubscribe_stack_visible)(int id);

    /* --- host dialogs --- */

    /** The host dialog's item while it exists (QTCLIENT_HOST_DIALOG_*), else null. Re-read it
     *  on each show: CipSoft makes dialog items again. */
    QtClientQQuickItemPtr (*host_dialog_item)(int kind);
    int (*host_dialog_shown)(int kind);
    /** `fn` runs when a host dialog of `kind` (0: any) shows or hides. */
    int (*subscribe_host_dialog_shown)(int kind, void (*fn)(void* user, int kind, int shown), void* user);
    void (*unsubscribe_host_dialog_shown)(int id);
    /** The host dialog of `kind` loads `qml_url` (as QtClientDialogDescV1.qml_url) instead of
     *  CipSoft's file, with the same controller (its QML surface must match). Null / "" goes
     *  back to CipSoft's. Takes effect on the next item made. Returns nonzero when the kind
     *  supports it (CREATE_CHARACTER). */
    int (*set_host_dialog_qml)(int kind, const char* qml_url);
} QtClientDialogApiV1;

#ifdef __cplusplus
}
#endif
