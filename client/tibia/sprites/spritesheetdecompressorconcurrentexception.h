#pragma once

#include <QException>

namespace tibia {
namespace sprites {

/// What decoding a spritesheet reports when it failed (CIP
/// sprites::TSpritesheetDecompressorConcurrentException). The decode's future
/// keeps it; reading the future's result throws it again on the reading
/// thread.
class TSpritesheetDecompressorConcurrentException : public QException
{
public:
    /// CIP v3.
    void raise() const override { throw *this; }
    /// CIP v4.
    TSpritesheetDecompressorConcurrentException* clone() const override
    {
        return new TSpritesheetDecompressorConcurrentException(*this);
    }
};

}
}
