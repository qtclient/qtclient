#include "shared/iodevicewriter.h"

namespace tibia::shared {

TIODeviceWriter::TIODeviceWriter(std::shared_ptr<QIODevice> device, const QDataStream::ByteOrder byteOrder)
    : m_device(std::move(device))
{
    m_stream = std::make_shared<QDataStream>(m_device.get());
    m_stream->setByteOrder(byteOrder);
}

TIODeviceWriter::~TIODeviceWriter()
{
    if (m_device && m_openedByWriter && m_device->isOpen())
        close();
}

bool TIODeviceWriter::isOpen() const { return m_device->isOpen(); }

void TIODeviceWriter::open(const QIODevice::OpenMode mode)
{
    m_device->open(mode);
    m_openedByWriter = true;
}

void TIODeviceWriter::close() { m_device->close(); }

void TIODeviceWriter::writeUInt8(const uint8_t value) { *m_stream << static_cast<qint8>(value); }

void TIODeviceWriter::writeUInt16(const uint16_t value) { *m_stream << static_cast<qint16>(value); }

void TIODeviceWriter::writeUInt32(const uint32_t value) { *m_stream << static_cast<qint32>(value); }

void TIODeviceWriter::writeUInt64(const uint64_t value) { *m_stream << static_cast<qint64>(value); }

void TIODeviceWriter::writeString8(const QString& value)
{
    writeUInt8(static_cast<uint8_t>(value.size()));
    writeRawBytes(value.toLatin1());
}

void TIODeviceWriter::writeString16(const QString& value)
{
    writeUInt16(static_cast<uint16_t>(value.size()));
    writeRawBytes(value.toLatin1());
}

void TIODeviceWriter::writeRawBytes(const QByteArray& bytes) { m_stream->writeRawData(bytes.constData(), bytes.size()); }

void TIODeviceWriter::writeBool(const bool value) { writeUInt8(value); }

uint32_t TIODeviceWriter::pos() const { return static_cast<uint32_t>(m_device->pos()); }

uint32_t TIODeviceWriter::size() const { return static_cast<uint32_t>(m_device->size()); }

}
