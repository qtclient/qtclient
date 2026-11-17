#include "sessiondump/clientsessiondumprecorder.h"

#include "shared/iodevicewriter.h"

#include "clockservice.h"

#include <protocol.pb.h>

#include <QBuffer>
#include <QTimeZone>

namespace tibia::sessiondump {

namespace {

constexpr uint16_t RecordedVersion = 7;
constexpr uint32_t RecordedClientVersion = 0x604;
constexpr uint32_t RecordedMode = 1;
constexpr int LoginSuccessMessage = 23;

/// The game world's time zone.
const QTimeZone& serverTimeZone()
{
    static const QTimeZone zone(QByteArrayLiteral("Europe/Berlin"));
    return zone;
}

std::shared_ptr<shared::TIODeviceWriter> bufferWriter(const std::shared_ptr<QBuffer>& buffer)
{
    return std::make_shared<shared::TIODeviceWriter>(buffer, QDataStream::LittleEndian);
}

}

QByteArray* TClientPartialSessiondump::beginPacket(const uint32_t number, const int64_t& timestamp)
{
    finishPacket();
    m_current.number = number;
    m_current.timestamp = timestamp;
    return &m_current.data;
}

uint32_t TClientPartialSessiondump::currentPacketNumber() const { return m_current.number; }

void TClientPartialSessiondump::finishPacket()
{
    if (!m_current.data.isEmpty()) {
        auto buffer = std::make_shared<QBuffer>();
        const auto writer = bufferWriter(buffer);
        writer->open(QIODevice::WriteOnly);
        writer->writeUInt8('p');
        writer->writeUInt32(uint32_t(m_current.data.size()) + 5);
        writer->writeUInt8(1);
        writer->writeUInt32(uint32_t(m_current.timestamp));
        writer->writeRawBytes(m_current.data);
        writer->close();
        m_current.data = buffer->data();
        m_packets.push_back(m_current);
    }
    m_current = TPacket();
}

void TClientPartialSessiondump::setMetadata(const TClientSessiondumpSegment& segment, const int64_t oldestTimestamp)
{
    m_segment = segment;
    while (!m_packets.empty() && m_packets.front().timestamp < oldestTimestamp)
        m_packets.pop_front();
}

QByteArray TClientPartialSessiondump::sessiondump()
{
    finishPacket();
    const TSessiondumpMetadata& metadata = m_segment.metadata;
    auto metadataBuffer = std::make_shared<QBuffer>();
    const auto metadataWriter = bufferWriter(metadataBuffer);
    metadataWriter->open(QIODevice::WriteOnly | QIODevice::Append);
    metadataWriter->writeUInt16(metadata.version);
    metadataWriter->writeUInt32(metadata.clientVersion);
    metadataWriter->writeUInt8(uint8_t(metadata.recordingMode));
    metadataWriter->writeString8(metadata.sessionKey);
    // CipSoft writes the creation time's milliseconds into the 32-bit field
    // (TSessiondumpMetadataReader reads it as seconds).
    metadataWriter->writeUInt32(uint32_t(metadata.creationTime.toMSecsSinceEpoch()));
    metadataWriter->writeString8(metadata.characterName);
    metadataWriter->writeUInt32(metadata.playerId);
    metadataWriter->writeString8(metadata.worldName);
    metadataWriter->writeUInt8(uint8_t(metadata.extraFlag));
    metadataWriter->writeString8(metadata.extraText);
    metadataWriter->writeUInt16(metadata.extraValue1);
    metadataWriter->writeUInt16(metadata.extraValue2);
    metadataWriter->close();
    const QByteArray metadataBytes = metadataBuffer->data();

    auto buffer = std::make_shared<QBuffer>();
    const auto writer = bufferWriter(buffer);
    writer->open(QIODevice::WriteOnly);
    writer->writeUInt8('d');
    writer->writeUInt8('m');
    writer->writeUInt8('p');
    writer->writeUInt16(uint16_t(metadataBytes.size()));
    writer->writeRawBytes(metadataBytes);
    for (const TPacket& packet : m_packets)
        writer->writeRawBytes(packet.data);
    writer->close();
    return buffer->data();
}

TClientSessiondumpRecorder::TClientSessiondumpRecorder(const int64_t keepSeconds)
    : m_keepSeconds(keepSeconds)
{
}

TClientSessiondumpRecorder::~TClientSessiondumpRecorder() = default;

void TClientSessiondumpRecorder::openWriter(QByteArray* payload)
{
    m_writer = bufferWriter(std::make_shared<QBuffer>(payload));
    m_writer->open(QIODevice::WriteOnly);
}

void TClientSessiondumpRecorder::start(const QString& characterName, const QString& worldName, const QString& sessionKey)
{
    m_clockService = std::make_shared<shared::TClockService>();
    m_partialSessiondump = std::make_shared<TClientPartialSessiondump>();
    openWriter(m_partialSessiondump->beginPacket(0, 0));
    m_nextSegment.metadata.worldName = worldName;
    m_nextSegment.metadata.characterName = characterName;
    m_nextSegment.metadata.sessionKey = sessionKey;
    m_nextSegment.metadata.version = RecordedVersion;
    m_nextSegment.metadata.clientVersion = RecordedClientVersion;
    m_nextSegment.metadata.recordingMode = RecordedMode;
    m_currentSegment.metadata = m_nextSegment.metadata;
    m_partialSessiondump->setMetadata(m_currentSegment, 0);
}

void TClientSessiondumpRecorder::setPlayerId(const uint32_t playerId)
{
    m_currentSegment.metadata.playerId = playerId;
    m_nextSegment.metadata.playerId = playerId;
}

void TClientSessiondumpRecorder::startNewSegmentIfDue()
{
    const int64_t now = m_clockService->nowMillis();
    if (m_nextSegment.startMillis + m_keepSeconds * 1000 < now) {
        m_currentSegment = m_nextSegment;
        m_nextSegment.startMillis = now;
        m_nextSegment.metadata.creationTime = QDateTime::currentDateTimeUtc().toTimeZone(serverTimeZone());
        m_partialSessiondump->setMetadata(m_currentSegment, m_currentSegment.startMillis);
    }
}

void TClientSessiondumpRecorder::handleAnyMessage(std::shared_ptr<protobuf::protocol::GameserverMessage> pGameserverMessage)
{
    if (pGameserverMessage->type() == LoginSuccessMessage)
        setPlayerId(pGameserverMessage->GetExtension(protobuf::protocol::GameserverMessageLoginSuccess::gameserver_message_login_success)
                        .player_id());
    startNewSegmentIfDue();
    const uint32_t packetNumber = pGameserverMessage->meta_data().packet_number();
    if (m_partialSessiondump->currentPacketNumber() != packetNumber)
        openWriter(m_partialSessiondump->beginPacket(packetNumber, m_clockService->nowMillis()));
    pGameserverMessage->mutable_meta_data()->set_timestamp(m_clockService->nowMillis());
    writeMessage(QByteArray::fromStdString(pGameserverMessage->SerializeAsString()));
}

void TClientSessiondumpRecorder::writeMessage(const QByteArray& message)
{
    auto buffer = std::make_shared<QBuffer>();
    const auto writer = bufferWriter(buffer);
    writer->open(QIODevice::WriteOnly);
    writer->writeUInt8('m');
    writer->writeUInt16(uint16_t(message.size()));
    writer->writeRawBytes(message);
    writer->close();
    m_writer->writeRawBytes(buffer->data());
}

QByteArray TClientSessiondumpRecorder::sessiondump() { return m_partialSessiondump ? m_partialSessiondump->sessiondump() : QByteArray(); }

}
