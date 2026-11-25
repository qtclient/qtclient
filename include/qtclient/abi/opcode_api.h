#pragma once

/// Extended opcodes (host API v9): the server's extended opcode messages and sending them.
/// Replaces the tip's protocol::ExtendedOpcodeReader / sendExtendedJsonOpcode.
/// Host: client/tibia/mods/opcode_abi.cpp.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One extended opcode message as the server sent it (`data` borrowed for the call; with the
 *  JSON chunk framing 'S' / 'P' / 'E' still in front when the server chunked it: the receiver
 *  reassembles). */
typedef void (*QtClientOpcodeFn)(void* user, uint8_t opcode, const char* data, size_t len);

typedef struct QtClientOpcodeApiV1 {
    size_t struct_size;

    /** Nonzero when this host's game connection routes extended opcodes (the server
     *  announced them); 0 on a host or server without them. */
    int (*live)(void);

    /** `fn` runs for each received message of `opcode` (0..255), or of every opcode when
     *  `opcode` is -1. */
    int (*subscribe)(int opcode, QtClientOpcodeFn fn, void* user);
    void (*unsubscribe)(int id);

    /** Sends `data` as one extended opcode message (no framing added). Returns nonzero when it
     *  went to the connection (0 offline or not live). */
    int (*send)(uint8_t opcode, const char* data, size_t len);
    /** Sends a JSON value (UTF-8 text), chunked with the 'S' / 'P' / 'E' framing when it does
     *  not fit one message. Returns nonzero when it went out; 0 also for invalid JSON. */
    int (*send_json)(uint8_t opcode, const char* json_utf8);
} QtClientOpcodeApiV1;

#ifdef __cplusplus
}
#endif
