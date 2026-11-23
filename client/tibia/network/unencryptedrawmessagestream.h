#pragma once

#include <QBuffer>
#include <QByteArray>

namespace tibia {
namespace network {

/// CIP network::TUnencryptedRawMessageStream: the decoded bytes of game server
/// packets (or of encoded client messages), read like a device. Opened for
/// reading and writing from the start.
class TUnencryptedRawMessageStream : public QBuffer
{
public:
    TUnencryptedRawMessageStream();
    ~TUnencryptedRawMessageStream() override;

    /// Appends at the end; the read position stays where it is.
    virtual void appendData(const QByteArray& Data);
    /// Drops everything before the read position and reads from the start again.
    virtual void removeReadData();
    /// Everything from the read position on.
    virtual QByteArray readAllData();
};

}
}
