#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QString>

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {

/// CIP `shared::TIODeviceReader`: typed reads from a QIODevice through a
/// QDataStream in the given byte order. Every read first checks that enough
/// bytes are available and throws std::runtime_error otherwise.
class TIODeviceReader
{
public:
    TIODeviceReader(std::shared_ptr<QIODevice> device, QDataStream::ByteOrder byteOrder);
    virtual ~TIODeviceReader();

    virtual bool isOpen() const;
    /// Opens the device; a reader that opened it closes it when destroyed.
    virtual void open(QIODevice::OpenMode mode);
    virtual void close();

    virtual uint8_t readUInt8();
    virtual uint8_t peekUInt8();
    virtual uint16_t readUInt16();
    virtual uint32_t readUInt32();
    virtual uint64_t readUInt64();
    virtual int readInt8();
    virtual int readInt16();
    virtual int32_t readInt32();
    virtual int64_t readInt64();
    /// Latin-1 string behind an 8-bit length.
    virtual QString readString8();
    /// Latin-1 string behind a 16-bit length.
    virtual QString readString16();
    virtual QByteArray readBytes(uint32_t count);
    virtual QByteArray peekBytes(uint32_t count);
    virtual bool readBool();
    /// An 8-bit precision followed by a 32-bit value offset by 0x7fffffff.
    virtual double readDouble();
    virtual uint32_t bytesAvailable() const;
    virtual bool hasBytesAvailable(uint32_t count);
    virtual bool atEnd() const;
    virtual void seek(uint32_t position);
    /// Reads and drops bytes up to `position`; throws when already past it.
    virtual void skipTo(uint32_t position);
    virtual uint32_t pos() const;
    virtual uint32_t size() const;
    virtual void ensureBytesAvailable(uint32_t count);

private:
    std::shared_ptr<QIODevice> m_device;
    std::shared_ptr<QDataStream> m_stream;
    bool m_openedByReader = false;
};

}
}
