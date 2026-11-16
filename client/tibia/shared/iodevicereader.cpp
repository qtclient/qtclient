#include "shared/iodevicereader.h"

#include <QAbstractSocket>

#include <cmath>
#include <stdexcept>

namespace tibia::shared {

TIODeviceReader::TIODeviceReader(std::shared_ptr<QIODevice> device, const QDataStream::ByteOrder byteOrder)
    : m_device(std::move(device))
{
    m_stream = std::make_shared<QDataStream>(m_device.get());
    m_stream->setByteOrder(byteOrder);
}

TIODeviceReader::~TIODeviceReader()
{
    if (m_device && m_openedByReader && m_device->isOpen())
        close();
}

bool TIODeviceReader::isOpen() const { return m_device->isOpen(); }

void TIODeviceReader::open(const QIODevice::OpenMode mode)
{
    m_device->open(mode);
    m_openedByReader = true;
}

void TIODeviceReader::close() { m_device->close(); }

uint8_t TIODeviceReader::readUInt8()
{
    ensureBytesAvailable(1);
    qint8 value;
    *m_stream >> value;
    return static_cast<uint8_t>(value);
}

uint8_t TIODeviceReader::peekUInt8()
{
    const QByteArray bytes = peekBytes(1);
    return static_cast<uint8_t>(bytes.data()[0]);
}

uint16_t TIODeviceReader::readUInt16()
{
    ensureBytesAvailable(2);
    qint16 value;
    *m_stream >> value;
    return static_cast<uint16_t>(value);
}

uint32_t TIODeviceReader::readUInt32()
{
    ensureBytesAvailable(4);
    qint32 value;
    *m_stream >> value;
    return static_cast<uint32_t>(value);
}

uint64_t TIODeviceReader::readUInt64()
{
    ensureBytesAvailable(8);
    qint64 value;
    *m_stream >> value;
    return static_cast<uint64_t>(value);
}

int TIODeviceReader::readInt8()
{
    ensureBytesAvailable(1);
    qint8 value;
    *m_stream >> value;
    return value;
}

int TIODeviceReader::readInt16()
{
    ensureBytesAvailable(2);
    qint16 value;
    *m_stream >> value;
    return value;
}

int32_t TIODeviceReader::readInt32() { return static_cast<int32_t>(readUInt32()); }

int64_t TIODeviceReader::readInt64() { return static_cast<int64_t>(readUInt64()); }

QString TIODeviceReader::readString8()
{
    const QByteArray bytes = readBytes(readUInt8());
    return QString::fromLatin1(bytes);
}

QString TIODeviceReader::readString16()
{
    const QByteArray bytes = readBytes(readUInt16());
    return QString::fromLatin1(bytes);
}

QByteArray TIODeviceReader::readBytes(const uint32_t count)
{
    ensureBytesAvailable(count);
    QByteArray bytes;
    bytes.resize(count);
    m_stream->readRawData(bytes.data(), bytes.size());
    return bytes;
}

QByteArray TIODeviceReader::peekBytes(const uint32_t count)
{
    ensureBytesAvailable(count);
    return m_device->peek(count);
}

bool TIODeviceReader::readBool() { return readUInt8() != 0; }

double TIODeviceReader::readDouble()
{
    const uint8_t precision = readUInt8();
    const uint32_t value = readUInt32();
    return static_cast<double>(static_cast<int64_t>(value) - 0x7fffffff) / std::pow(10.0, precision);
}

uint32_t TIODeviceReader::bytesAvailable() const { return static_cast<uint32_t>(m_device->bytesAvailable()); }

bool TIODeviceReader::hasBytesAvailable(const uint32_t count)
{
    // Peeking needs a buffer; requests larger than it fall back to bytesAvailable().
    static QByteArray s_peekBuffer(0x100000, ' ');
    if (count > static_cast<uint32_t>(s_peekBuffer.size()))
        return count <= bytesAvailable();
    return static_cast<uint64_t>(m_device->peek(s_peekBuffer.data(), count)) == count;
}

bool TIODeviceReader::atEnd() const { return size() - 1U <= pos(); }

void TIODeviceReader::seek(const uint32_t position) { m_device->seek(position); }

void TIODeviceReader::skipTo(const uint32_t position)
{
    const uint32_t current = pos();
    if (position < current)
        throw std::runtime_error("can't skip. pos is already bigger than skip position.");
    readBytes(position - current);
}

uint32_t TIODeviceReader::pos() const { return static_cast<uint32_t>(m_device->pos()); }

uint32_t TIODeviceReader::size() const { return static_cast<uint32_t>(m_device->size()); }

void TIODeviceReader::ensureBytesAvailable(const uint32_t count)
{
    if (m_device) {
        if (const auto* socket = dynamic_cast<const QAbstractSocket*>(m_device.get());
            socket && socket->state() == QAbstractSocket::UnconnectedState)
            throw std::runtime_error("attempting to read from unconnected socket");
    }
    if (hasBytesAvailable(count))
        return;
    const QString message = QStringLiteral("not enough bytes (%1) available at position %2").arg(count).arg(pos());
    throw std::runtime_error(message.toStdString());
}

}
