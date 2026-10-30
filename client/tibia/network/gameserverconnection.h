#pragma once

#include <cip_gen.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <unordered_set>

#if defined(Q_OS_WASM)
#include <emscripten/websocket.h>
#else
#include <QTcpSocket>
#include <QThread>
#endif

#include <zlib.h>

class Protocol;

namespace tibia::network {

#if !defined(Q_OS_WASM)
class GameSocketWorker final : public QObject
{
    Q_OBJECT
public:
    void openHost(const QString& host, quint16 port);
    void writeBytes(const QByteArray& bytes);
    void closeSocket();

Q_SIGNALS:
    void connected();
    void failed(const QString& message);
    void disconnected();
    void received(const QByteArray& bytes);

private:
    QTcpSocket* m_socket = nullptr;
};
#endif

/// 1405 XTEA decrypt: strips the padding byte and trailing pad. Shared with the
/// secondary socket, which copies the primary session key.
bool decryptGameXtea1405(const std::array<uint32_t, 4>& key, QByteArray& body);

/// Secondary socket hands an already-decrypted precursor (post-XTEA, pre-inflate)
/// to the primary connection so both sockets share one parser.
void deliverSecondaryDecrypted(uint32_t sequenceWord, QByteArray precursor);

void installGameServerConnection();

/// Primary game socket. Desktop: QTcpSocket on a worker thread, bytes queued to
/// the GUI thread. Browser: WebSocket on the game port. Owns XTEA, sequence,
/// checksum, and the decoded-message recorder.
class TGameserverNetworkPacketConnection final : public cip::tibia::network::TGameserverNetworkPacketConnection
{
    Q_OBJECT
public:
    explicit TGameserverNetworkPacketConnection(QObject* parent = nullptr);
    ~TGameserverNetworkPacketConnection() override;

    void open(Protocol* proto, const QString& host, uint16_t port, bool gameWorld);
    void shutdown();

    bool connected() const { return m_connected; }
    bool connecting() const { return m_connecting; }
    qint64 millisSinceRead() const;

    void writePayload(const uint8_t* data, uint16_t size, bool raw);
    void generateXtea(uint32_t key[4]);
    void setXtea(const uint32_t key[4]);
    void enableXtea();
    void enableChecksum();
    void enableSequenced();
    void setRecordFile(const QString& file);

    void acceptDecrypted(uint32_t sequenceWord, QByteArray precursor);

#if !defined(Q_OS_WASM)
Q_SIGNALS:
    void outbound(QByteArray bytes);
    void requestClose();
#endif

private:
    void onTransportConnected();
    void flushEarly();
    void ingestWire(const QByteArray& chunk);
    void decodeFrame(const QByteArray& afterSize);
    void recordInput(const QByteArray& precursor);
    void recordOutput(const uint8_t* data, uint16_t size);
    QByteArray frame(const QByteArray& payload, bool raw);
    bool inflatePrecursor(QByteArray& body);
    void fail(const QString& message, bool clean);

    Protocol* m_proto = nullptr;
    QString m_host;
    uint16_t m_port = 0;
    bool m_gameWorld = true;
    bool m_connected = false;
    bool m_connecting = false;
    bool m_xtea = false;
    bool m_checksum = false;
    bool m_sequenced = false;
    uint32_t m_packetNumber = 0;
    std::array<uint32_t, 4> m_xteaKey{};
    QByteArray m_recv;
    QByteArray m_early;
    QElapsedTimer m_readTimer;
    std::unordered_set<uint32_t> m_seen;
    bool m_skipFirstOutput = true;
    qint64 m_recordStart = 0;
    std::unique_ptr<std::ofstream> m_record;
    z_stream m_zstream{};
    int m_compression = 0;

#if defined(Q_OS_WASM)
    void openWebSocket();
    QString webSocketUrl() const;
    EMSCRIPTEN_WEBSOCKET_T m_websocket = 0;
    bool m_wsConnected = false;
#else
    QThread* m_thread = nullptr;
    GameSocketWorker* m_worker = nullptr;
#endif
};

}
