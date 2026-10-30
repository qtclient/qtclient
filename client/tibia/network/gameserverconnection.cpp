#include "gameserverconnection.h"

#include "framework/net/gamelink.h"
#include "framework/net/protocol.h"

#include <tibia/world/game.h>
#include <framework/core/clock.h>
#include <framework/stdext/math.h>
#include <framework/util/crypt.h>

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

// modules/gamelib/const.lua OTSERV_RSA — Crystal's login packet key.
constexpr const char* OtservRsa =
    "1091201329673994292788609605089955415282375029027981291234687579"
    "3726629149257644633073969600111060390723088861007265581882535850"
    "3429057592827629436413108566029093628212635953836686562675849720"
    "6207862794310902180176810615217550567108238764764442605581471797"
    "07119674283982419152118103759076030616683978566631413";

struct Pending
{
    QString record;
    std::array<uint32_t, 4> key{};
    bool hasKey = false;
    bool xtea = false;
    bool checksum = false;
    bool sequenced = false;
};

std::unordered_map<Protocol*, TGameserverNetworkPacketConnection*> g_byProto;
std::unordered_map<Protocol*, Pending> g_pending;

TGameserverNetworkPacketConnection* findConn(Protocol* proto)
{
    const auto it = g_byProto.find(proto);
    return it == g_byProto.end() ? nullptr : it->second;
}

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
    const auto proto = g_game.getProtocolGame();
    auto* conn = proto ? findConn(proto.get()) : nullptr;
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

void TGameserverNetworkPacketConnection::open(Protocol* proto, const QString& host, const uint16_t port, const bool gameWorld)
{
    m_proto = proto;
    m_host = host;
    m_port = port;
    m_gameWorld = gameWorld;
    m_connecting = true;
    if (const auto pending = g_pending.find(proto); pending != g_pending.end()) {
        if (!pending->second.record.isEmpty())
            setRecordFile(pending->second.record);
        if (pending->second.hasKey)
            m_xteaKey = pending->second.key;
        m_xtea = pending->second.xtea;
        m_checksum = pending->second.checksum;
        m_sequenced = pending->second.sequenced;
        g_pending.erase(pending);
    }

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
    if (m_proto)
        m_proto->notifyConnected();
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
    if (!m_proto || m_proto->isReplay())
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
    m_recordStart = g_clock.millis();
    m_skipFirstOutput = true;
}

void TGameserverNetworkPacketConnection::ingestWire(const QByteArray& chunk)
{
    if (!m_proto || chunk.isEmpty())
        return;
    // XTEA is enabled during onConnect. Bytes that arrive before that must wait,
    // or the first server message is decrypted with the wrong state.
    if (!m_connected) {
        m_early.append(chunk);
        return;
    }
    m_recv.append(chunk);
    const bool modern = g_game.getClientVersion() >= 1405;
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
        if (!m_proto)
            return;
    }
}

void TGameserverNetworkPacketConnection::decodeFrame(const QByteArray& afterSize)
{
    const bool modern = g_game.getClientVersion() >= 1405;
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
        const auto sum = stdext::computeChecksum({ reinterpret_cast<const uint8_t*>(body.constData()), size_t(body.size()) });
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
    if (!m_proto)
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
    m_proto->deliverParserBytes(reinterpret_cast<const uint8_t*>(precursor.constData()), uint16_t(precursor.size()));
}

void TGameserverNetworkPacketConnection::recordInput(const QByteArray& precursor)
{
    if (!m_record || !*m_record)
        return;
    *m_record << "< " << (g_clock.millis() - m_recordStart) << ' ';
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
    *m_record << "> " << (g_clock.millis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (uint16_t i = 0; i < size; ++i)
        *m_record << std::setw(2) << int(data[i]);
    *m_record << std::dec << '\n';
}

QByteArray TGameserverNetworkPacketConnection::frame(const QByteArray& payload, const bool raw)
{
    if (raw)
        return payload;
    const bool modern = g_game.getClientVersion() >= 1405;
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
        const auto sum = stdext::computeChecksum({ reinterpret_cast<const uint8_t*>(body.constData()), size_t(body.size()) });
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
    if (!m_proto)
        return;
    Protocol* proto = m_proto;
    m_proto = nullptr;
    m_connected = false;
    m_connecting = false;
    proto->fail(message.toStdString(), clean);
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

void installGameServerConnection()
{
    g_crypt.rsaSetPublicKey(OtservRsa, "65537");
    gamelink::install({
        .connect = [](Protocol* proto, const char* host, uint16_t port, bool gameWorld) {
            if (!proto || findConn(proto))
                return;
            auto* conn = new TGameserverNetworkPacketConnection();
            g_byProto.emplace(proto, conn);
            conn->open(proto, QString::fromUtf8(host ? host : ""), port, gameWorld);
        },
        .disconnect = [](Protocol* proto) {
            const auto it = g_byProto.find(proto);
            if (it == g_byProto.end())
                return;
            auto* conn = it->second;
            g_byProto.erase(it);
            g_pending.erase(proto);
            conn->shutdown();
            conn->deleteLater();
        },
        .isConnected = [](Protocol* proto) {
            const auto* conn = findConn(proto);
            return conn && conn->connected();
        },
        .isConnecting = [](Protocol* proto) {
            const auto* conn = findConn(proto);
            return conn && conn->connecting();
        },
        .write = [](Protocol* proto, const uint8_t* data, uint16_t size, bool raw) {
            if (auto* conn = findConn(proto))
                conn->writePayload(data, size, raw);
        },
        .generateXtea = [](Protocol* proto, uint32_t key[4]) {
            if (auto* conn = findConn(proto)) {
                conn->generateXtea(key);
                return;
            }
            auto& pending = g_pending[proto];
            std::random_device rd;
            std::uniform_int_distribution<uint32_t> unif;
            for (uint32_t& part : pending.key)
                part = unif(rd);
            pending.hasKey = true;
            std::memcpy(key, pending.key.data(), sizeof(pending.key));
        },
        .setXtea = [](Protocol* proto, const uint32_t key[4]) {
            if (auto* conn = findConn(proto))
                conn->setXtea(key);
            else {
                auto& pending = g_pending[proto];
                std::memcpy(pending.key.data(), key, sizeof(pending.key));
                pending.hasKey = true;
            }
        },
        .enableXtea = [](Protocol* proto) {
            if (auto* conn = findConn(proto))
                conn->enableXtea();
            else
                g_pending[proto].xtea = true;
        },
        .enableChecksum = [](Protocol* proto) {
            if (auto* conn = findConn(proto))
                conn->enableChecksum();
            else
                g_pending[proto].checksum = true;
        },
        .enableSequenced = [](Protocol* proto) {
            if (auto* conn = findConn(proto))
                conn->enableSequenced();
            else
                g_pending[proto].sequenced = true;
        },
        .setRecordFile = [](Protocol* proto, const char* file) {
            const QString path = QString::fromUtf8(file ? file : "");
            if (auto* conn = findConn(proto))
                conn->setRecordFile(path);
            else
                g_pending[proto].record = path;
        },
        .millisSinceRead = [](Protocol* proto) -> int64_t {
            const auto* conn = findConn(proto);
            return conn ? conn->millisSinceRead() : -1;
        },
    });
}

}
