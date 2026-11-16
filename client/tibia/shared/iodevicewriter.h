#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QString>

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TIODeviceWriter`: typed writes to a QIODevice through a
/// QDataStream in the given byte order.
class TIODeviceWriter
{
public:
    TIODeviceWriter(std::shared_ptr<QIODevice> device, QDataStream::ByteOrder byteOrder);
    virtual ~TIODeviceWriter();

    virtual bool isOpen() const;
    /// Opens the device; a writer that opened it closes it when destroyed.
    virtual void open(QIODevice::OpenMode mode);
    virtual void close();

    virtual void writeUInt8(uint8_t value);
    virtual void writeUInt16(uint16_t value);
    virtual void writeUInt32(uint32_t value);
    virtual void writeUInt64(uint64_t value);
    /// Latin-1 string behind an 8-bit length.
    virtual void writeString8(const QString& value);
    /// Latin-1 string behind a 16-bit length.
    virtual void writeString16(const QString& value);
    virtual void writeRawBytes(const QByteArray& bytes);
    virtual void writeBool(bool value);
    virtual uint32_t pos() const;
    virtual uint32_t size() const;

private:
    std::shared_ptr<QIODevice> m_device;
    std::shared_ptr<QDataStream> m_stream;
    bool m_openedByWriter = false;
};

}
}
