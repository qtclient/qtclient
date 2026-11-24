#pragma once

/// Host-pushed sound table (v8): an extension holds CipSoft's ambience channel and plays its own
/// bed and one-shots at the ambience volume. Without a hold the server's ambience streams play
/// as in CipSoft.

#include "qtclient/abi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QtClientSoundApiV1 {
    /** The place's ambience stream (the server's), 0 for none. */
    uint16_t (*current_ambience_id)(void);
    /** Holds the channel: the place stream fades out; later stream triggers wait for the end. */
    void (*begin_ambience_hold)(void);
    /** Ends the hold: the bed stops, the deferred stream (else `fallback_restore_id`) plays. */
    void (*end_ambience_hold)(uint16_t fallback_restore_id);
    /** Plays a CipSoft ambience stream as the held bed. Nonzero when it plays. */
    int (*play_held_ambience_stream)(uint16_t ambience_stream_id);
    /** Plays a sound file (virtual path) as the held bed. Nonzero when it plays. */
    int (*play_held_ambience_file)(const char* virtual_path_utf8, int loop);
    /** Multiplier 0..1 of the bed's volume (1 again when the hold ends). */
    void (*set_ambience_hold_gain)(float gain);
    /** A numeric sound effect once at `gain` x the ambience volume. Nonzero when it plays. */
    int (*play_ambience_effect)(uint16_t sound_effect_id, float gain);
    /** A sound file once at `gain` x the ambience volume. Nonzero when it plays. */
    int (*play_ambience_oneshot)(const char* virtual_path_utf8, float gain);
} QtClientSoundApiV1;

#ifdef __cplusplus
}
#endif
