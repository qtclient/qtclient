#pragma once

#include "zlibbasewrapper.h"

namespace tibia::shared {

/// CIP `shared::TZlibInflateWrapper`: raw inflate. A streaming inflater keeps
/// its window across calls (each message ends at a SYNC_FLUSH point); a
/// one-shot inflater expects whole streams and resets after each.
class TZlibInflateWrapper : public TZlibBaseWrapper
{
public:
    TZlibInflateWrapper() = default;
    ~TZlibInflateWrapper() override;

    /// Z_OK, or the zlib error of inflateInit2 (also when already initialised: Z_OK, with an error message).
    int init(unsigned char windowBits, bool oneShot);
    int end();
    bool isInitialized() const;
    bool isOneShot() const;
    /// Inflates `input` into `output` (resized to the result). Z_OK or a zlib error.
    int inflate(const unsigned char* input, unsigned inputSize, std::vector<unsigned char>& output);

private:
    void inflateGrowing(int flush, int& result, std::vector<unsigned char>& output);

    bool m_oneShot = false;
};

}
