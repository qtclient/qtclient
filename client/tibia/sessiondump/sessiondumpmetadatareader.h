#pragma once

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace sessiondump {

/// The metadata block after the "dmp" magic (little-endian). Which fields a
/// file carries depends on `version`.
struct TSessiondumpMetadata
{
    bool read = false;
    uint16_t size = 0;
    uint16_t version = 0;
    uint32_t clientVersion = 0;
    /// Version 6 and later: how messages are recorded (the message processor's mode).
    uint32_t recordingMode = 0;
    /// Version 4 to 6: a number; version 7 and later: a string.
    QString sessionKey;
    QDateTime creationTime;
    QString characterName;
    /// The recording character's player id.
    uint32_t playerId = 0;
    QString worldName;
    /// Version 5 and later.
    uint32_t extraFlag = 0;
    QString extraText;
    uint16_t extraValue1 = 0;
    uint16_t extraValue2 = 0;
};

/// CIP `sessiondump::TSessiondumpMetadataReader`.
class TSessiondumpMetadataReader
{
public:
    explicit TSessiondumpMetadataReader(std::shared_ptr<shared::TIODeviceReader> reader);
    virtual ~TSessiondumpMetadataReader();

    virtual bool isRead() const;
    /// Throws std::runtime_error when the dump is not open or the magic is wrong.
    virtual void read();
    virtual TSessiondumpMetadata metadata() const;
    /// Stream position after the metadata.
    virtual uint64_t metadataEnd() const;

private:
    void readMagic();
    void readFields();

    TSessiondumpMetadata m_metadata;
    std::shared_ptr<shared::TIODeviceReader> m_reader;
    uint64_t m_metadataEnd = 0;
};

}
}
