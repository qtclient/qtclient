// miniaudio single-translation-unit. CIP client banks are Ogg Vorbis only — decode
// via libvorbis + libvorbisfile (same family as CIP TSoundDecompressorThreadpool).
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_VORBIS

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#include "vendor/miniaudio_libvorbis.c"
