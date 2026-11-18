#pragma once

#include "sessiondump/sessiondumpmetadatareader.h"

#include <QByteArray>
#include <QObject>

#include <cstdint>
#include <deque>
#include <memory>

namespace tibia {
namespace protobuf::protocol {
class GameserverMessage;
}
namespace shared {
class IClockService;
class TIODeviceWriter;
}
namespace sessiondump {

/// The metadata of a recorded segment with where and when the segment began.
struct TClientSessiondumpSegment
{
    int64_t position = 0;
    int64_t startMillis = 0;
    TSessiondumpMetadata metadata;
};

/// CIP `sessiondump::TClientPartialSessiondump`: the last packets of a running
/// session, framed like a dump file, behind the metadata of the oldest segment.
class TClientPartialSessiondump
{
public:
    struct TPacket
    {
        uint32_t number = 0;
        int64_t timestamp = 0;
        /// The payload; once finished the whole 'p' packet.
        QByteArray data;
    };

    /// Finishes the current packet and starts packet `number` at `timestamp`;
    /// the messages are written into the returned payload.
    QByteArray* beginPacket(uint32_t number, const int64_t& timestamp);
    uint32_t currentPacketNumber() const;
    /// Sets the metadata the dump is written with and drops the packets older than `oldestTimestamp`.
    void setMetadata(const TClientSessiondumpSegment& segment, int64_t oldestTimestamp);
    /// The dump: "dmp", the metadata and every finished packet.
    QByteArray sessiondump();

    const std::deque<TPacket>& packets() const { return m_packets; }

private:
    void finishPacket();

    TPacket m_current;
    TClientSessiondumpSegment m_segment;
    std::deque<TPacket> m_packets;
};

/// CIP `sessiondump::TClientSessiondumpRecorder`: records every server message
/// of the session into a partial dump that keeps between `keepSeconds` and
/// twice that much of the session, for crash reports. Messages of one network
/// packet go into one dump packet.
class TClientSessiondumpRecorder : public QObject
{
    Q_OBJECT
public:
    explicit TClientSessiondumpRecorder(int64_t keepSeconds);
    ~TClientSessiondumpRecorder() override;

    /// Starts a new recording. Host: CipSoft reads the names from the session's
    /// TGameserverGameSessionMetaInfo; qtclient's session passes them.
    virtual void start(const QString& characterName, const QString& worldName, const QString& sessionKey);
    /// The partial dump as a dump file.
    QByteArray sessiondump();
    void setPlayerId(uint32_t playerId);

public Q_SLOTS:
    virtual void handleAnyMessage(std::shared_ptr<tibia::protobuf::protocol::GameserverMessage> pGameserverMessage);

private:
    void startNewSegmentIfDue();
    void writeMessage(const QByteArray& message);
    void openWriter(QByteArray* payload);

    TClientSessiondumpSegment m_currentSegment;
    TClientSessiondumpSegment m_nextSegment;
    int64_t m_keepSeconds = 0;
    std::shared_ptr<shared::TIODeviceWriter> m_writer;
    std::shared_ptr<TClientPartialSessiondump> m_partialSessiondump;
    std::shared_ptr<shared::IClockService> m_clockService;
};

}
}
