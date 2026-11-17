#include "sprites/spritesheetdecompressorsynchronous.h"

namespace tibia::sprites {

void TSpritesheetDecompressorSynchronous::decompress(const TSpritesheetID& spritesheetId,
                                                     const std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob>& data)
{
    Q_EMIT decompressingSpritesheetFinished(spritesheetId, decode(data));
}

}
