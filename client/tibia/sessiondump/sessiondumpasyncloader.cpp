#include "sessiondump/sessiondumpasyncloader.h"

#include "sessiondump/sessiondumpfactory.h"
#include "sessiondump/sessiondumpmessageprocessor.h"
#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumppacketprocessor.h"
#include "sessiondump/sessiondumpprocessor.h"
#include "sessiondump/sessiondumpreader.h"
#include "shared/iodevicereader.h"

#include "clockservice.h"

#include <protocol.pb.h>

#include <QDebug>
#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkRequest>

#include <limits>

namespace tibia::sessiondump {

namespace {

constexpr int ChunkIntervalMillis = 1000;
constexpr int PacketsPerChunk = 1001;

std::shared_ptr<shared::TIODeviceReader> fileReader(const QTemporaryFile& file)
{
    auto device = std::make_shared<QFile>();
    device->setFileName(file.fileName());
    return std::make_shared<shared::TIODeviceReader>(device, QDataStream::LittleEndian);
}

}

TSessiondumpAsyncLoader::TSessiondumpAsyncLoader(const QUrl& url)
    : m_url(url)
{
    m_file.setAutoRemove(true);
    static_cast<void>(m_file.open());
}

TSessiondumpAsyncLoader::~TSessiondumpAsyncLoader()
{
    if (m_preprocessingDevice)
        m_preprocessingDevice->close();
    m_processor.reset();
    for (const auto& connection : m_connections)
        disconnect(connection);
}

void TSessiondumpAsyncLoader::startLoading()
{
    m_preprocessor.reset();
    QNetworkRequest request(m_url);
    request.setDecompressedSafetyCheckThreshold(-1);
    m_networkAccessManager = std::make_unique<QNetworkAccessManager>();
    m_reply = m_networkAccessManager->get(request);
    m_replyOwner = std::shared_ptr<QNetworkReply>(m_reply, [](QNetworkReply* reply) { reply->deleteLater(); });
    m_clockService = std::make_unique<shared::TClockService>();

    m_connections.push_back(connect(m_reply, &QNetworkReply::downloadProgress, this, &TSessiondumpAsyncLoader::downloadProgress));
    m_connections.push_back(connect(m_reply, &QNetworkReply::finished, this, &TSessiondumpAsyncLoader::downloadFinished));
    m_connections.push_back(connect(m_reply, &QNetworkReply::errorOccurred, this, &TSessiondumpAsyncLoader::downloadError));
    // Host: CipSoft also routes the manager's authenticationRequired to its
    // TNetworkAccessManagerAuthenticationHandler; qtclient's dumps need no login.
    m_chunkTimer.setInterval(ChunkIntervalMillis);
    m_chunkTimer.setSingleShot(true);
    m_connections.push_back(connect(&m_chunkTimer, &QTimer::timeout, this, &TSessiondumpAsyncLoader::processNextChunk));

    m_preprocessingDevice = fileReader(m_file);
    m_preprocessingDevice->open(QIODevice::ReadOnly);
    const auto factory = std::make_shared<TSessiondumpFactory>();
    const auto reader = std::make_shared<TSessiondumpReader>(m_preprocessingDevice, factory);
    const auto messageProcessor = std::make_shared<TLightweightSessiondumpMessageProcessor>(
        reader->messageReader(),
        [this](const TSessiondumpPacketInfo& packet, const TSessiondumpMessageInfo& message,
               const std::function<QByteArray()>& bytes) { onSessiondumpNewMessage(packet, message, bytes); });
    const auto packetProcessor = std::make_shared<TSessiondumpPacketProcessor>(reader->packetReader(), messageProcessor);
    m_preprocessor = std::make_shared<TSessiondumpProcessor>(reader, packetProcessor);
}

void TSessiondumpAsyncLoader::downloadProgress(qlonglong, qlonglong) { m_chunkTimer.start(); }

void TSessiondumpAsyncLoader::downloadFinished()
{
    m_chunkTimer.start();
    if (!m_chunkTimer.isActive())
        m_chunkTimer.start();
}

void TSessiondumpAsyncLoader::downloadError(const QNetworkReply::NetworkError)
{
    m_chunkTimer.stop();
    QString message;
    if (m_reply)
        message = m_reply->errorString();
    qDebug() << "QNetworkReply::error" << message;
    Q_EMIT sessiondumpLoadingError(message);
}

void TSessiondumpAsyncLoader::processNextChunk()
{
    if (m_reply && m_reply->isOpen() && m_reply->error() == QNetworkReply::NoError) {
        const QByteArray data = m_reply->readAll();
        if (!data.isEmpty()) {
            m_file.write(data);
            m_file.flush();
        }
    }
    const auto metadataReader = m_preprocessor->reader()->metadataReader();
    if (!metadataReader->isRead()) {
        metadataReader->read();
        if (metadataReader->isRead()) {
            m_metadata = metadataReader->metadata();
            const QString text = QStringLiteral("Sessiondump read:\nWorldName: %1\nTimestamp:%2\nHeader length:%3\nCharacter:%4")
                                     .arg(m_metadata.worldName)
                                     .arg(QLocale::system().toString(m_metadata.creationTime, QLocale::ShortFormat))
                                     .arg(m_metadata.size)
                                     .arg(m_metadata.characterName);
            qDebug().noquote() << text;
            Q_EMIT sessiondumpMetadataLoaded();
        }
    }
    int remaining = PacketsPerChunk;
    while (m_preprocessor->hasNextPacket()) {
        if (remaining-- == 0) {
            if (!m_chunkTimer.isActive())
                m_chunkTimer.start();
            break;
        }
        m_preprocessor->processNextPacket();
    }
    if (m_reply) {
        if (m_reply->bytesAvailable() == 0 && m_reply->isFinished()) {
            m_reply = nullptr;
            m_replyOwner.reset();
            Q_EMIT finished();
        } else if (m_reply->bytesAvailable() > 0 && m_reply->error() == QNetworkReply::NoError && !m_chunkTimer.isActive()) {
            m_chunkTimer.start();
        }
    }
}

void TSessiondumpAsyncLoader::onSessiondumpNewMessage(const TSessiondumpPacketInfo& PacketInfo,
                                                      const TSessiondumpMessageInfo& MessageInfo,
                                                      const std::function<QByteArray()>& RawMessageBytesReaderFunction)
{
    Q_EMIT preprocessingMessageAvailable(PacketInfo, MessageInfo, RawMessageBytesReaderFunction);
}

void TSessiondumpAsyncLoader::startProcessing()
{
    // Host: CipSoft passes its protocol settings to the translator; qtclient's
    // reads them from the appearances manager.
    const auto factory = std::make_shared<TSessiondumpFactory>();
    m_processingDevice = fileReader(m_file);
    m_processingDevice->open(QIODevice::ReadOnly);
    const auto reader = std::make_shared<TSessiondumpReader>(m_processingDevice, factory);
    createProtocolMessageConsumer();
    const auto messageProcessor = std::make_shared<TSessiondumpMessageProcessor>(reader->messageReader(), m_messageConsumer);
    const auto packetProcessor = std::make_shared<TSessiondumpPacketProcessor>(reader->packetReader(), messageProcessor);
    m_processor = std::make_shared<TSessiondumpProcessor>(reader, packetProcessor);
    m_processor->start();
}

const TSessiondumpMetadata& TSessiondumpAsyncLoader::metadata() const { return m_metadata; }

bool TSessiondumpAsyncLoader::processUntil(const int64_t& timestamp, const int64_t& maxDurationMillis, bool seekKeyframe)
{
    const int64_t start = m_clockService->nowMillis();
    if (m_processor) {
        while (m_processor->hasNextPacket()) {
            if (seekKeyframe) {
                m_processor->seekKeyframeBegin();
                seekKeyframe = false;
            }
            m_nextTimestamp = int64_t(m_processor->nextPacketInfo().timestamp);
            if (timestamp <= m_nextTimestamp)
                return true;
            m_processor->processNextPacket();
            m_currentTimestamp = int64_t(m_processor->currentPacketInfo().timestamp);
            if (maxDurationMillis < m_clockService->nowMillis() - start)
                return false;
        }
        m_nextTimestamp = std::numeric_limits<int64_t>::max();
        if (m_processor->atEnd())
            Q_EMIT sessiondumpFinished();
    }
    return true;
}

const int64_t& TSessiondumpAsyncLoader::currentTimestamp() const { return m_currentTimestamp; }

const int64_t& TSessiondumpAsyncLoader::nextTimestamp() const { return m_nextTimestamp; }

void TSessiondumpAsyncLoader::seek(const uint32_t position)
{
    m_processor->reader()->packetReader()->reset();
    m_processor->reader()->messageReader()->reset();
    m_currentTimestamp = 0;
    m_processingDevice->seek(position);
}

uint32_t TSessiondumpAsyncLoader::packetsBegin() const
{
    return uint32_t(m_processor->reader()->metadataReader()->metadataEnd());
}

}
