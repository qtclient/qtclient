#pragma once

/// CipSoft objects (host API v9): instances of CipSoft's client classes that the host makes
/// for a pack, so a pack can serve CipSoft's own QML (social dialog pages, team finder pages,
/// the social button, ...) without CipSoft's generated C++ bases. Host:
/// client/tibia/mods/cip_object_abi.cpp.
///
/// Two sorts of class, both named by CipSoft's class name (no namespace):
/// - Model classes (the generated `cip::tibia::...` bases: TSocialDialogController,
///   TFriendsFriendListPageController, TTeamJoinPageController, TAccountCharacterListData,
///   TSocialButtonController, ...): the host makes a delegate subclass. Each of the class's
///   overridable methods (Q_INVOKABLEs, slots, virtual property setters, called by QML or the
///   host) first asks the pack's QtClientCipObjectDelegateV1.invoke; when that returns 0 the
///   CipSoft base implementation runs. The pack fills the properties with set_value /
///   set_object (read-only CipSoft properties included; the property's NOTIFY signal is
///   emitted) and emits the class's signals with emit_signal.
/// - Host classes (CipSoft controllers the host implements, e.g. TCharacterInfoDialogController
///   for a page that embeds it): the host's implementation; the delegate may be null.
/// class_supported says which names this host has.
///
/// Values are JSON (UTF-8): bool / numbers / strings as such, CipSoft enums as their int,
/// string lists as arrays, variant maps as objects; Qt object arguments come as null (use
/// get_object / set_object for Qt object properties). Method and signal names are the moc
/// names without signature ("requestTabSwitch"); an overloaded name takes the overload with
/// as many arguments as the JSON array holds.
///
/// Lifetime: an object is a Qt object; it is deleted with its Qt parent or by destroy, and the
/// delegate's `destroyed` runs then (the last call with its `user`).

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The pack side of a model class instance. */
typedef struct QtClientCipObjectDelegateV1 {
    size_t struct_size;
    void* user;
    /** `method` of `object` was called with `args_json` (a JSON array). Return nonzero when the
     *  pack handled it (write the return value as JSON into `result`, at most result_len - 1
     *  bytes plus a NUL; the host passes at least 1024; leave it "" for void), 0 to run
     *  CipSoft's base implementation. The pack may call call_base inside. May be null (every
     *  method runs the base). */
    int (*invoke)(void* user, QtClientQObjectPtr object, const char* method, const char* args_json, char* result,
                  size_t result_len);
    /** The object is being deleted. May be null. */
    void (*destroyed)(void* user);
} QtClientCipObjectDelegateV1;

/** A signal of a CipSoft object (`args_json` a JSON array, borrowed for the call). */
typedef void (*QtClientCipSignalFn)(void* user, QtClientQObjectPtr object, const char* signal, const char* args_json);

enum {
    /** set_host_controller slots: host surfaces whose CipSoft controller a pack replaces. */
    /** The control button bar's social button (CipSoft TSocialButtonController). */
    QTCLIENT_HOST_CONTROLLER_SOCIAL_BUTTON = 1,
};

typedef struct QtClientCipObjectApiV1 {
    size_t struct_size;

    /** Nonzero when this host can make `class_name`: 1 model class, 2 host class. */
    int (*class_supported)(const char* class_name);
    /** Makes an instance with Qt parent `parent` (may be null: then destroy it). `delegate` is
     *  copied (null: none). Null for an unknown class. */
    QtClientQObjectPtr (*create)(const char* class_name, QtClientQObjectPtr parent,
                                 const QtClientCipObjectDelegateV1* delegate);
    void (*destroy)(QtClientQObjectPtr object);

    /** Calls `method` as QML would (the delegate first for model classes). Returns 0 when it
     *  ran, -1 unknown method / bad arguments; the return value as JSON in `result`
     *  (sized-out rule; may be null with result_len 0). */
    int (*invoke)(QtClientQObjectPtr object, const char* method, const char* args_json, char* result,
                  size_t result_len);
    /** Runs CipSoft's base implementation of `method` (model classes; the host's for host
     *  classes), skipping the delegate. Same returns as invoke. */
    int (*call_base)(QtClientQObjectPtr object, const char* method, const char* args_json, char* result,
                     size_t result_len);

    /** Writes property `property` (its storage, also for a read-only CipSoft property) and
     *  emits its NOTIFY signal when it changed. Returns 0, -1 unknown property / bad value. */
    int (*set_value)(QtClientQObjectPtr object, const char* property, const char* value_json);
    /** The property's value as JSON (sized out); -1 unknown property. */
    int (*get_value)(QtClientQObjectPtr object, const char* property, char* buf, size_t buf_len);
    /** A Qt object property (a page controller, a list model the pack made, ...). The object
     *  must outlive its use there. */
    int (*set_object)(QtClientQObjectPtr object, const char* property, QtClientQObjectPtr value);
    QtClientQObjectPtr (*get_object)(QtClientQObjectPtr object, const char* property);

    /** Emits `signal` with `args_json` (a JSON array). Returns 0, -1 unknown / bad args. */
    int (*emit_signal)(QtClientQObjectPtr object, const char* signal, const char* args_json);
    /** `fn` runs each time `object` emits `signal` (until unsubscribed or the object goes). */
    int (*subscribe_signal)(QtClientQObjectPtr object, const char* signal, QtClientCipSignalFn fn, void* user);
    void (*unsubscribe_signal)(int id);

    /** A CipSoft enum value by key, e.g. ("TSocialDialogController", "EDialogTab",
     *  "AccountSearch"); -1 when unknown. */
    int (*enum_value)(const char* class_name, const char* enum_name, const char* key);

    /** Makes `controller` (a CipSoft object of the slot's class) the one the host surface's
     *  QML binds (QTCLIENT_HOST_CONTROLLER_*); null puts the host's back. Nonzero when the
     *  slot exists and the class fits. */
    int (*set_host_controller)(int slot, QtClientQObjectPtr controller);
} QtClientCipObjectApiV1;

#ifdef __cplusplus
}
#endif
