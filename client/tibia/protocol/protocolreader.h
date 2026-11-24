#pragma once

#include "worldmap/coordinates.h"

#include <QColor>

#include <cstdint>
#include <memory>

namespace tibia::shared {
class TIODeviceReader;
}

namespace tibia {
namespace protocol {

/// CIP protocol::TProtocolReader: the protocol's compound values (coordinates,
/// colours) read through a typed device reader, which the translators also use
/// for the plain values.
class TProtocolReader
{
public:
    explicit TProtocolReader(std::shared_ptr<shared::TIODeviceReader> MessageReader);
    virtual ~TProtocolReader();

    virtual std::shared_ptr<shared::TIODeviceReader> messageReader() const;
    /// u16 x, u16 y, u8 z. A caller that has read x already passes it; 0xffff
    /// reads it too.
    virtual worldmap::TWorldMapCoordinate readCoordinate(uint32_t X = 0xffff);
    /// An eight-bit colour index.
    virtual QColor readColor();

private:
    std::shared_ptr<shared::TIODeviceReader> m_messageReader;
};

}
}
