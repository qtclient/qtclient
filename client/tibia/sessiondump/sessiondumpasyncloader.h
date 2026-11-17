#pragma once

#include "sessiondump/lightweightsessiondumpmessageprocessor.h"
#include "sessiondump/sessiondumpmetadatareader.h"

#include <QNetworkReply>
#include <QObject>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QNetworkAccessManager;

namespace tibia {
namespace protobuf::protocol {
class GameserverMessage;
class GameclientMessage;
}
namespace shared {
class IClockService;
class TIODeviceReader;
}
namespace sessiondump {

class TSessiondumpMessageConsumer;
class TSessiondumpProcessor;

using TGameserverMessagePointer = std::shared_ptr<protobuf::protocol::GameserverMessage>;
using TGameclientMessagePointer = std::shared_ptr<protobuf::protocol::GameclientMessage>;

/// CIP `sessiondump::TSessiondumpAsyncLoader`: downloads a dump into a temporary
/// file and reads it twice. The preprocessing pass hands every message's raw
/// bytes out as the file grows (1001 packets per chunk); the processing pass
/// turns the messages into protobuf messages up to a timestamp.
class TSessiondumpAsyncLoader : public QObject
{
    Q_OBJECT
public:
    using TMessageRawBytesReaderFunction = std::function<QByteArray()>;

    explicit TSessiondumpAsyncLoader(const QUrl& url);
    ~TSessiondumpAsyncLoader() override;

    /// Starts the download and the preprocessing pass.
    virtual void startLoading();
    /// Opens the processing pass over the downloaded file.
    virtual void startProcessing();
    /// The dump's metadata once sessiondumpMetadataLoaded was emitted.
    virtual const TSessiondumpMetadata& metadata() const;
    /// Processes packets recorded before `timestamp` for at most `maxDurationMillis`;
    /// `seekKeyframe` first skips to the next keyframe. False when time ran out
    /// first; emits sessiondumpFinished at the end of the dump.
    virtual bool processUntil(const int64_t& timestamp, const int64_t& maxDurationMillis, bool seekKeyframe);
    /// The timestamp of the packet processed last.
    virtual const int64_t& currentTimestamp() const;
    /// The timestamp of the next packet, INT64_MAX at the end of the read data.
    virtual const int64_t& nextTimestamp() const;
    /// Moves the processing pass to `position` of the file.
    virtual void seek(uint32_t position);
    /// Where the packets of the file begin.
    virtual uint32_t packetsBegin() const;

Q_SIGNALS:
    void finished();
    void sessiondumpFinished();
    void sessiondumpLoadingError(QString ErrorMessage);
    void sessiondumpMetadataLoaded();
    void preprocessingMessageAvailable(tibia::sessiondump::TSessiondumpPacketInfo PacketInfo,
                                       tibia::sessiondump::TSessiondumpMessageInfo MessageInfo,
                                       std::function<QByteArray()> RawMessageBytesReaderFunction);
    void gameserverMessageAvailable(tibia::sessiondump::TGameserverMessagePointer pGameserverMessage);
    void gameclientMessageAvailable(tibia::sessiondump::TGameclientMessagePointer pGameclientMessage);

private Q_SLOTS:
    void downloadProgress(qlonglong BytesReceived, qlonglong BytesTotal);
    void downloadFinished();
    void downloadError(QNetworkReply::NetworkError NetworkError);
    void processNextChunk();
    void onSessiondumpNewMessage(const tibia::sessiondump::TSessiondumpPacketInfo& PacketInfo,
                                 const tibia::sessiondump::TSessiondumpMessageInfo& MessageInfo,
                                 const std::function<QByteArray()>& RawMessageBytesReaderFunction);

private:
    QUrl m_url;
    std::unique_ptr<shared::IClockService> m_clockService;
    std::unique_ptr<QNetworkAccessManager> m_networkAccessManager;
    QNetworkReply* m_reply = nullptr;
    std::shared_ptr<QNetworkReply> m_replyOwner;
    std::shared_ptr<TSessiondumpProcessor> m_preprocessor;
    std::shared_ptr<TSessiondumpProcessor> m_processor;
    QTimer m_chunkTimer;
    std::shared_ptr<TSessiondumpMessageConsumer> m_messageConsumer;
    TSessiondumpMetadata m_metadata;
    QTemporaryFile m_file;
    std::shared_ptr<shared::TIODeviceReader> m_preprocessingDevice;
    std::shared_ptr<shared::TIODeviceReader> m_processingDevice;
    int64_t m_currentTimestamp = 0;
    int64_t m_nextTimestamp = 0;
    std::vector<QMetaObject::Connection> m_connections;
};

}
}
