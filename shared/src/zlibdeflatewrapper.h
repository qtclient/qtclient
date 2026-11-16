#pragma once

#include "zlibbasewrapper.h"

namespace tibia::shared {

/// CIP `shared::TZlibDeflateWrapper`: raw deflate. A streaming deflater ends
/// every message with SYNC_FLUSH (and drops the 00 00 FF FF trailer); a
/// one-shot deflater finishes every message and resets. One-shot deflaters
/// share one process-wide stream.
class TZlibDeflateWrapper : public TZlibBaseWrapper
{
public:
    TZlibDeflateWrapper() = default;
    ~TZlibDeflateWrapper() override;

    int init(unsigned char windowBits, unsigned char memLevel, unsigned char level, bool oneShot);
    int end();
    bool isInitialized() const;
    unsigned char getWindowBits() const;
    unsigned char getMemLevel() const;
    unsigned char getLevel() const;
    bool isOneShot() const;
    /// Deflates `input` into `output` (resized to the result). Z_OK or a zlib error.
    int deflate(const unsigned char* input, unsigned inputSize, std::vector<unsigned char>& output);

private:
    int deflateInto(const unsigned char* input, unsigned inputSize, unsigned char* output, unsigned& outputSize);

    unsigned char m_windowBits = 15;
    unsigned char m_memLevel = 8;
    unsigned char m_level = 6;
    bool m_oneShot = false;
};

}
