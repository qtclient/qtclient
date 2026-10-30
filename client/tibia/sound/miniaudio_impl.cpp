// miniaudio single-translation-unit. CIP banks are Ogg Vorbis. miniaudio does
// not compile a Vorbis decoder unless stb_vorbis is included around it.
#define MA_NO_FLAC
#define MA_NO_MP3

#define STB_VORBIS_HEADER_ONLY
#include "vendor/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "vendor/stb_vorbis.c"
