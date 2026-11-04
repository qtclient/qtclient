#pragma once

#include "vendor/miniaudio.h"
#include "vendor/miniaudio_libvorbis.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ma_decoding_backend_vtable* ma_decoding_backend_libvorbis;

#ifdef __cplusplus
}
#endif

inline ma_decoder_config maDecoderConfigForCipOgg()
{
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    static ma_decoding_backend_vtable* backends[] = { ma_decoding_backend_libvorbis };
    config.ppCustomBackendVTables = backends;
    config.customBackendCount = 1;
    config.encodingFormat = ma_encoding_format_vorbis;
    return config;
}
