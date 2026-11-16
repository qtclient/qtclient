#include "gameserverconnection.h"

#include "network/gameserverpacketprocessor.h"

#include <tibia/game/gamesession.h>
#include "shared/sharedciptypes.h"
#include "shared/clockservice.h"
#include "rsahelper.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <cstring>
#include <filesystem>
#include <iomanip>
#include <random>
#include <unordered_map>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#endif

Q_LOGGING_CATEGORY(lcGameConn, "tibia.gameconnection")

namespace tibia::network {

namespace {

constexpr uint32_t XteaDelta = 0x9E3779B9;

void xteaBlocks(uint8_t* data, size_t length, const std::array<uint32_t, 4>& key, bool encrypt)
{
    for (size_t j = 0; j < length; j += 8) {
        uint32_t left = uint32_t(data[j]) | (uint32_t(data[j + 1]) << 8) | (uint32_t(data[j + 2]) << 16) | (uint32_t(data[j + 3]) << 24);
        uint32_t right = uint32_t(data[j + 4]) | (uint32_t(data[j + 5]) << 8) | (uint32_t(data[j + 6]) << 16) | (uint32_t(data[j + 7]) << 24);
        if (encrypt) {
            for (uint32_t i = 0, sum = 0, next = XteaDelta; i < 32; ++i, sum = next, next += XteaDelta) {
                left += ((right << 4 ^ right >> 5) + right) ^ (sum + key[sum & 3]);
                right += ((left << 4 ^ left >> 5) + left) ^ (next + key[(next >> 11) & 3]);
            }
        } else {
            for (uint32_t i = 0, sum = XteaDelta << 5, next = sum - XteaDelta; i < 32; ++i, sum = next, next -= XteaDelta) {
                right -= ((left << 4 ^ left >> 5) + left) ^ (sum + key[(sum >> 11) & 3]);
                left -= ((right << 4 ^ right >> 5) + right) ^ (next + key[next & 3]);
            }
        }
        data[j] = uint8_t(left);
        data[j + 1] = uint8_t(left >> 8);
        data[j + 2] = uint8_t(left >> 16);
        data[j + 3] = uint8_t(left >> 24);
        data[j + 4] = uint8_t(right);
        data[j + 5] = uint8_t(right >> 8);
        data[j + 6] = uint8_t(right >> 16);
        data[j + 7] = uint8_t(right >> 24);
    }
}

uint32_t readU32(const QByteArray& b, int at)
{
    return uint32_t(uint8_t(b[at]))
        | (uint32_t(uint8_t(b[at + 1])) << 8)
        | (uint32_t(uint8_t(b[at + 2])) << 16)
        | (uint32_t(uint8_t(b[at + 3])) << 24);
}

uint16_t readU16(const QByteArray& b, int at)
{
    return uint16_t(uint8_t(b[at]) | (uint16_t(uint8_t(b[at + 1])) << 8));
}

void appendU16(QByteArray& out, uint16_t value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
}

void prependU16(QByteArray& out, uint16_t value)
{
    QByteArray prefix;
    appendU16(prefix, value);
    out.prepend(prefix);
}

void prependU32(QByteArray& out, uint32_t value)
{
    QByteArray prefix;
    prefix.append(char(value & 0xff));
    prefix.append(char((value >> 8) & 0xff));
    prefix.append(char((value >> 16) & 0xff));
    prefix.append(char((value >> 24) & 0xff));
    out.prepend(prefix);
}

#if defined(Q_OS_WASM)
bool isLoopbackOrLan(const QString& host)
{
    if (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1") || host == QLatin1String("0.0.0.0"))
        return true;
    if (host.startsWith(QLatin1String("192.168.")) || host.startsWith(QLatin1String("10.")))
        return true;
    if (host.startsWith(QLatin1String("172."))) {
        const QStringList parts = host.split(QLatin1Char('.'));
        if (parts.size() >= 2) {
            bool ok = false;
            const int oct = parts[1].toInt(&ok);
            if (ok && oct >= 16 && oct <= 31)
                return true;
        }
    }
    return false;
}
#endif

}

bool decryptGameXtea1405(const std::array<uint32_t, 4>& key, QByteArray& body)
{
    if (body.size() < 8 || (body.size() % 8) != 0)
        return false;
    xteaBlocks(reinterpret_cast<uint8_t*>(body.data()), size_t(body.size()), key, false);
    const auto pad = uint8_t(body.at(0));
    if (pad >= body.size())
        return false;
    body = body.mid(1, body.size() - 1 - int(pad));
    return true;
}

void deliverSecondaryDecrypted(const uint32_t sequenceWord, QByteArray precursor)
{
    auto* processor = TGameserverNetworkPacketProcessor::live();
    auto* conn = processor ? processor->connection() : nullptr;
    if (!conn)
        return;
    conn->acceptDecrypted(sequenceWord, std::move(precursor));
}

#if !defined(Q_OS_WASM)
void GameSocketWorker::openHost(const QString& host, const quint16 port)
{
    m_socket = new QTcpSocket(this);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(m_socket, &QTcpSocket::connected, this, [this] { Q_EMIT connected(); });
    connect(m_socket, &QTcpSocket::readyRead, this, [this] {
        if (m_socket)
            Q_EMIT received(m_socket->readAll());
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        Q_EMIT failed(m_socket ? m_socket->errorString() : QStringLiteral("socket error"));
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] { Q_EMIT disconnected(); });
    m_socket->connectToHost(host, port);
}

void GameSocketWorker::writeBytes(const QByteArray& bytes)
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->write(bytes);
}

void GameSocketWorker::closeSocket()
{
    if (!m_socket)
        return;
    m_socket->disconnectFromHost();
    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
}
#endif

TGameserverNetworkPacketConnection::TGameserverNetworkPacketConnection(QObject* parent)
    : cip::tibia::network::TGameserverNetworkPacketConnection(parent)
{
    std::memset(&m_zstream, 0, sizeof(m_zstream));
    inflateInit2(&m_zstream, -15);
}

TGameserverNetworkPacketConnection::~TGameserverNetworkPacketConnection()
{
    shutdown();
    inflateEnd(&m_zstream);
}

void TGameserverNetworkPacketConnection::open(TGameserverNetworkPacketProcessor* processor, const QString& host,
                                              const uint16_t port, const bool gameWorld)
{
    m_processor = processor;
    m_host = host;
    m_port = port;
    m_gameWorld = gameWorld;
    m_connecting = true;

#if defined(Q_OS_WASM)
    openWebSocket();
#else
    m_thread = new QThread();
    m_worker = new GameSocketWorker();
    m_worker->moveToThread(m_thread);
    connect(m_worker, &GameSocketWorker::connected, this, [this] { onTransportConnected(); }, Qt::QueuedConnection);
    connect(m_worker, &GameSocketWorker::received, this, [this](const QByteArray& bytes) { ingestWire(bytes); }, Qt::QueuedConnection);
    connect(m_worker, &GameSocketWorker::failed, this, [this](const QString& message) { fail(message, false); }, Qt::QueuedConnection);
    connect(m_worker, &GameSocketWorker::disconnected, this, [this] { fail({}, true); }, Qt::QueuedConnection);
    connect(this, &TGameserverNetworkPacketConnection::outbound, m_worker, &GameSocketWorker::writeBytes, Qt::QueuedConnection);
    connect(this, &TGameserverNetworkPacketConnection::requestClose, m_worker, &GameSocketWorker::closeSocket, Qt::QueuedConnection);
    m_thread->start();
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, host, port] {
        worker->openHost(host, port);
    }, Qt::QueuedConnection);
#endif
}

void TGameserverNetworkPacketConnection::shutdown()
{
    m_connected = false;
    m_connecting = false;
    m_record.reset();
#if defined(Q_OS_WASM)
    m_wsConnected = false;
    const EMSCRIPTEN_WEBSOCKET_T sock = m_websocket;
    m_websocket = 0;
    if (sock > 0) {
        emscripten_websocket_close(sock, 1000, "close");
        emscripten_websocket_delete(sock);
    }
#else
    if (m_worker && m_thread) {
        if (m_thread->isRunning()) {
            // moveToThread may only push an object off the thread that owns it.
            QMetaObject::invokeMethod(m_worker, [worker = m_worker, home = thread() ? thread() : QThread::currentThread()] {
                worker->closeSocket();
                worker->moveToThread(home);
            }, Qt::BlockingQueuedConnection);
            m_thread->quit();
            m_thread->wait(2000);
        }
        delete m_worker;
        m_worker = nullptr;
        delete m_thread;
        m_thread = nullptr;
    }
#endif
    QCoreApplication::removePostedEvents(this);
}

void TGameserverNetworkPacketConnection::flushEarly()
{
    if (m_early.isEmpty())
        return;
    const QByteArray pending = std::move(m_early);
    m_early.clear();
    ingestWire(pending);
}

void TGameserverNetworkPacketConnection::onTransportConnected()
{
    if (m_connected)
        return;
    m_connecting = false;
    m_connected = true;
    m_readTimer.start();
    if (m_processor)
        m_processor->connectionEstablished();
    flushEarly();
}

qint64 TGameserverNetworkPacketConnection::millisSinceRead() const
{
    if (!m_connected || !m_readTimer.isValid())
        return -1;
    return m_readTimer.elapsed();
}

void TGameserverNetworkPacketConnection::writePayload(const uint8_t* data, const uint16_t size, const bool raw)
{
    if (!m_processor)
        return;
    recordOutput(data, size);
    const QByteArray framed = frame(QByteArray(reinterpret_cast<const char*>(data), size), raw);
    if (framed.isEmpty())
        return;
#if defined(Q_OS_WASM)
    if (m_websocket > 0 && m_wsConnected)
        emscripten_websocket_send_binary(m_websocket, const_cast<char*>(framed.constData()), framed.size());
#else
    if (m_connected)
        Q_EMIT outbound(framed);
#endif
}

void TGameserverNetworkPacketConnection::generateXtea(uint32_t key[4])
{
    std::random_device rd;
    std::uniform_int_distribution<uint32_t> unif;
    for (uint32_t& part : m_xteaKey)
        part = unif(rd);
    std::memcpy(key, m_xteaKey.data(), sizeof(m_xteaKey));
}

void TGameserverNetworkPacketConnection::setXtea(const uint32_t key[4])
{
    std::memcpy(m_xteaKey.data(), key, sizeof(m_xteaKey));
}

void TGameserverNetworkPacketConnection::enableXtea() { m_xtea = true; }
void TGameserverNetworkPacketConnection::enableChecksum() { m_checksum = true; }
void TGameserverNetworkPacketConnection::enableSequenced() { m_sequenced = true; }

void TGameserverNetworkPacketConnection::setRecordFile(const QString& file)
{
    if (file.isEmpty())
        return;
    std::error_code ec;
    std::filesystem::create_directory("records", ec);
    const auto path = std::filesystem::path("records") / file.toStdString();
    m_record = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
    m_recordStart = tibia::shared::TClockService::frameMillis();
    m_skipFirstOutput = true;
}

void TGameserverNetworkPacketConnection::ingestWire(const QByteArray& chunk)
{
    if (!m_processor || chunk.isEmpty())
        return;
    // XTEA is enabled during onConnect. Bytes that arrive before that must wait,
    // or the first server message is decrypted with the wrong state.
    if (!m_connected) {
        m_early.append(chunk);
        return;
    }
    m_recv.append(chunk);
    const bool modern = true;
    while (m_recv.size() >= 2) {
        const auto header = readU16(m_recv, 0);
        const int remaining = modern ? int(header) * 8 + 4 : int(header);
        if (remaining <= 0 || remaining > 0xffff) {
            qCWarning(lcGameConn) << "invalid frame header" << header;
            fail(QStringLiteral("invalid packet size"), false);
            return;
        }
        if (m_recv.size() < 2 + remaining)
            return;
        const QByteArray frame = m_recv.mid(2, remaining);
        m_recv.remove(0, 2 + remaining);
        decodeFrame(frame);
        if (!m_processor)
            return;
    }
}

void TGameserverNetworkPacketConnection::decodeFrame(const QByteArray& afterSize)
{
    const bool modern = true;
    const bool hasWord = m_sequenced || m_checksum || modern;
    if (hasWord && afterSize.size() < 4) {
        qCWarning(lcGameConn) << "short game frame" << afterSize.size();
        return;
    }
    uint32_t word = 0;
    QByteArray body = afterSize;
    if (hasWord) {
        word = readU32(afterSize, 0);
        body = afterSize.mid(4);
    }
    if (!m_sequenced && m_checksum) {
        const auto sum = uint32_t(::adler32(::adler32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(body.constData()), uInt(body.size())));
        if (sum != word) {
            qCWarning(lcGameConn) << "invalid game checksum";
            return;
        }
    }
    if (m_xtea) {
        if (modern) {
            if (!decryptGameXtea1405(m_xteaKey, body)) {
                qCWarning(lcGameConn) << "XTEA decrypt failed";
                return;
            }
        } else {
            if (body.size() < 8 || (body.size() % 8) != 0) {
                qCWarning(lcGameConn) << "invalid encrypted frame";
                return;
            }
            xteaBlocks(reinterpret_cast<uint8_t*>(body.data()), size_t(body.size()), m_xteaKey, false);
            if (body.size() < 2)
                return;
            const int inner = readU16(body, 0);
            if (inner < 0 || 2 + inner > body.size()) {
                qCWarning(lcGameConn) << "invalid decrypted size";
                return;
            }
            body = body.mid(2, inner);
        }
    }
    acceptDecrypted(word, std::move(body));
}

bool TGameserverNetworkPacketConnection::inflatePrecursor(QByteArray& body)
{
    static constexpr int PerPacket = 1;
    static constexpr int Stream = 2;
    uint8_t zbuffer[65536];
    auto tryPerPacket = [&]() -> int {
        m_zstream.next_in = reinterpret_cast<Bytef*>(body.data());
        m_zstream.next_out = zbuffer;
        m_zstream.avail_in = uInt(body.size());
        m_zstream.avail_out = sizeof(zbuffer);
        m_zstream.total_out = 0;
        const int ret = inflate(&m_zstream, Z_FINISH);
        const int produced = int(m_zstream.total_out);
        if (ret == Z_STREAM_END && produced > 0) {
            m_compression = PerPacket;
            inflateReset(&m_zstream);
            body = QByteArray(reinterpret_cast<const char*>(zbuffer), produced);
            return 1;
        }
        inflateReset(&m_zstream);
        return 0;
    };

    if (m_compression == 0 || m_compression == PerPacket) {
        if (tryPerPacket())
            return true;
        if (m_compression == PerPacket) {
            qCWarning(lcGameConn) << "per-packet inflate failed";
            return false;
        }
        m_compression = Stream;
    }

    if (body.size() < 4 || uint8_t(body[body.size() - 4]) != 0x00 || uint8_t(body[body.size() - 3]) != 0x00
        || uint8_t(body[body.size() - 2]) != 0xff || uint8_t(body[body.size() - 1]) != 0xff)
        body.append("\x00\x00\xff\xff", 4);

    m_zstream.next_in = reinterpret_cast<Bytef*>(body.data());
    m_zstream.next_out = zbuffer;
    m_zstream.avail_in = uInt(body.size());
    m_zstream.avail_out = sizeof(zbuffer);
    m_zstream.total_out = 0;
    const int ret = inflate(&m_zstream, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        qCWarning(lcGameConn) << "stream inflate failed";
        return false;
    }
    if (m_zstream.total_out == 0)
        return false;
    body = QByteArray(reinterpret_cast<const char*>(zbuffer), int(m_zstream.total_out));
    return true;
}

void TGameserverNetworkPacketConnection::acceptDecrypted(const uint32_t sequenceWord, QByteArray precursor)
{
    if (!m_processor)
        return;
    if (m_sequenced) {
        const uint32_t seq = sequenceWord & 0x7fffffffu;
        if (!m_seen.insert(seq).second)
            return;
        if ((sequenceWord & 0x80000000u) != 0) {
            if (!inflatePrecursor(precursor))
                return;
        }
    }
    if (precursor.isEmpty())
        return;
    m_readTimer.start();
    recordInput(precursor);
    if (precursor.size() > 0xffff)
        return;
    m_processor->deliverPacket(reinterpret_cast<const uint8_t*>(precursor.constData()), size_t(precursor.size()));
}

void TGameserverNetworkPacketConnection::recordInput(const QByteArray& precursor)
{
    if (!m_record || !*m_record)
        return;
    *m_record << "< " << (tibia::shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (const char byte : precursor)
        *m_record << std::setw(2) << int(uint8_t(byte));
    *m_record << std::dec << '\n';
}

void TGameserverNetworkPacketConnection::recordOutput(const uint8_t* data, const uint16_t size)
{
    if (!m_record || !*m_record || !data)
        return;
    if (m_skipFirstOutput) {
        m_skipFirstOutput = false;
        return;
    }
    *m_record << "> " << (tibia::shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (uint16_t i = 0; i < size; ++i)
        *m_record << std::setw(2) << int(data[i]);
    *m_record << std::dec << '\n';
}

QByteArray TGameserverNetworkPacketConnection::frame(const QByteArray& payload, const bool raw)
{
    if (raw)
        return payload;
    const bool modern = true;
    QByteArray body = payload;
    if (modern) {
        const int padAmt = 8 - (body.size() % 8) - 1;
        if (padAmt > 0)
            body.append(QByteArray(padAmt, '\0'));
        body.prepend(char(padAmt));
    } else if (m_xtea) {
        prependU16(body, uint16_t(body.size()));
    }
    if (m_xtea) {
        if ((body.size() % 8) != 0)
            body.append(QByteArray(8 - (body.size() % 8), '\0'));
        xteaBlocks(reinterpret_cast<uint8_t*>(body.data()), size_t(body.size()), m_xteaKey, true);
    }
    if (m_sequenced)
        prependU32(body, m_packetNumber++);
    else if (m_checksum) {
        const auto sum = uint32_t(::adler32(::adler32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(body.constData()), uInt(body.size())));
        prependU32(body, sum);
    }
    if (modern) {
        if (body.size() < 4)
            return {};
        const auto header = uint16_t((body.size() - 4) / 8);
        prependU16(body, header);
    } else {
        prependU16(body, uint16_t(body.size()));
    }
    return body;
}

void TGameserverNetworkPacketConnection::fail(const QString& message, const bool clean)
{
    if (!m_processor)
        return;
    TGameserverNetworkPacketProcessor* processor = m_processor;
    m_processor = nullptr;
    m_connected = false;
    m_connecting = false;
    processor->connectionFailed(message, clean);
}

#if defined(Q_OS_WASM)
QString TGameserverNetworkPacketConnection::webSocketUrl() const
{
    QString prefix = QStringLiteral("ws://");
    if (!isLoopbackOrLan(m_host)) {
        const bool forceWss = m_port == 443
            || m_host.contains(QLatin1String("trycloudflare.com"))
            || m_host.contains(QLatin1String("cfargotunnel.com"));
        const int pageIsHttps = EM_ASM_INT({
            try {
                return (typeof location !== 'undefined' && location.protocol === 'https:') ? 1 : 0;
            } catch (e) {
                return 0;
            }
        });
        if (forceWss || pageIsHttps)
            prefix = QStringLiteral("wss://");
    }
    return prefix + m_host + QLatin1Char(':') + QString::number(m_port);
}

void TGameserverNetworkPacketConnection::openWebSocket()
{
    const QByteArray urlUtf8 = webSocketUrl().toUtf8();
    EmscriptenWebSocketCreateAttributes attributes = {
        urlUtf8.constData(),
        nullptr,
        EM_FALSE,
    };
    m_websocket = emscripten_websocket_new(&attributes);
    m_wsConnected = false;
    if (m_websocket < 1) {
        m_websocket = 0;
        fail(QStringLiteral("WebSocket create failed"), false);
        return;
    }
    emscripten_websocket_set_onopen_callback(m_websocket, this, [](int, const EmscriptenWebSocketOpenEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverNetworkPacketConnection*>(userData);
        QMetaObject::invokeMethod(self, [self] {
            // writePayload sends only after this flag. Setting it in the same
            // turn as notifyConnected is what actually transmits the login packet.
            self->m_wsConnected = true;
            self->onTransportConnected();
        }, Qt::QueuedConnection);
        return EM_TRUE;
    });
    emscripten_websocket_set_onmessage_callback(m_websocket, this, [](int, const EmscriptenWebSocketMessageEvent* event, void* userData) -> EM_BOOL {
        if (!event || event->numBytes <= 0 || event->isText)
            return EM_TRUE;
        auto* self = static_cast<TGameserverNetworkPacketConnection*>(userData);
        const QByteArray chunk(reinterpret_cast<const char*>(event->data), int(event->numBytes));
        QMetaObject::invokeMethod(self, [self, chunk] { self->ingestWire(chunk); }, Qt::QueuedConnection);
        return EM_TRUE;
    });
    emscripten_websocket_set_onerror_callback(m_websocket, this, [](int, const EmscriptenWebSocketErrorEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverNetworkPacketConnection*>(userData);
        QMetaObject::invokeMethod(self, [self] { self->fail(QStringLiteral("WebSocket error"), false); }, Qt::QueuedConnection);
        return EM_TRUE;
    });
    emscripten_websocket_set_onclose_callback(m_websocket, this, [](int, const EmscriptenWebSocketCloseEvent*, void* userData) -> EM_BOOL {
        auto* self = static_cast<TGameserverNetworkPacketConnection*>(userData);
        QMetaObject::invokeMethod(self, [self] {
            self->m_wsConnected = false;
            const EMSCRIPTEN_WEBSOCKET_T sock = self->m_websocket;
            self->m_websocket = 0;
            if (sock > 0) {
                emscripten_websocket_close(sock, 1000, "peer close");
                emscripten_websocket_delete(sock);
            }
            self->fail({}, true);
        }, Qt::QueuedConnection);
        return EM_TRUE;
    });
}
#endif

shared::TRsaHelper& loginRsaHelper()
{
    static shared::TRsaHelper helper;
    return helper;
}

void installGameServerConnection()
{
    // Crystal's login RSA modulus, big-endian; the exponent is 65537.
    static const QByteArray LoginModulus = QByteArray::fromHex(
        "9b646903b45b07ac956568d87353bd7165139dd7940703b03e6dd079399661b4"
        "a837aa60561d7ccb9452fa0080594909882ab5bca58a1a1b35f8b1059b72b121"
        "2611c6152ad3dbb3cfbee7adc142a75d3d75971509c321c5c24a5bd51fd460f0"
        "1b4e15beb0de1930528a5d3f15c1e3cbf5c401d6777e10acaab33dbe8d5b7ff5");
    loginRsaHelper().setPublicKey(LoginModulus);
}

}
