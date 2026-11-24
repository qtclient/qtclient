#pragma once

#include <memory>

namespace tibia::shared {
class TIODeviceWriter;
}

namespace tibia {
namespace protocol {

/// CIP protocol::TProtocolWriter: the typed device writer a client message
/// translator writes a message's values to.
class TProtocolWriter
{
public:
    explicit TProtocolWriter(std::shared_ptr<shared::TIODeviceWriter> MessageWriter);
    virtual ~TProtocolWriter();

    virtual std::shared_ptr<shared::TIODeviceWriter> messageWriter() const;

private:
    std::shared_ptr<shared::TIODeviceWriter> m_messageWriter;
};

}
}
