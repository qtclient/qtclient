#pragma once

#include "sprites/spritesheetdecompressor.h"

namespace tibia {
namespace sprites {

/// Decodes on the calling thread and reports before returning
/// (CIP sprites::TSpritesheetDecompressorSynchronous).
class TSpritesheetDecompressorSynchronous final : public TSpritesheetDecompressor
{
    Q_OBJECT
public:
    using TSpritesheetDecompressor::TSpritesheetDecompressor;

    void decompress(const TSpritesheetID& spritesheetId,
                    const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data) override;
};

}
}
