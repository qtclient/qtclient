#pragma once

// Unpacker for the "bundle" container CipSoft links statically (r-lyeh/bundle):
// optional zero padding, 'p', a codec id, the unpacked and packed sizes as
// 7-bit varints, then the packed bytes. Not CipSoft logic; the replacement for
// the library calls of shared::TLZMACompressedReadOnlyMemoryBlob and
// decompressBundle(). Of the codecs CipSoft's build includes, RAW, MINIZ
// (raw deflate) and LZMA20/LZMA25 are decoded here; LZIP, ZPAQ, CSC20 and
// CRUSH are reported as failures.

#include <string>

namespace tibia::shared::bundle {

/// True when `data` starts with a bundle header.
bool isPacked(const std::string& data);

/// Unpacks `packed` into `unpacked`. On failure `unpacked` becomes a copy of
/// `packed` and the result is false (the library's contract).
bool unpack(std::string& unpacked, const std::string& packed);

}
