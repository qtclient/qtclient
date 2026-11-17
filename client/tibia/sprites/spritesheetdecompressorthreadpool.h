#pragma once

#include "sprites/spritesheetdecompressor.h"

namespace tibia {
namespace sprites {

/// Decodes on Qt's global thread pool and reports on this object's thread
/// (CIP sprites::TSpritesheetDecompressorThreadpool).
class TSpritesheetDecompressorThreadpool final : public TSpritesheetDecompressor
{
    Q_OBJECT
public:
    using TSpritesheetDecompressor::TSpritesheetDecompressor;

    void decompress(const TSpritesheetID& spritesheetId,
                    const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data) override;
};

}
}
