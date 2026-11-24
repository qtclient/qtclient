#include "network/gameserverconnection.h"

#include "protocol/protocolciptypes.h"

#include "clockservice.h"
#include "rsahelper.h"

#include <QTimer>

#include <filesystem>
#include <iomanip>
#include <utility>

namespace tibia::network {

namespace {

TGameserverSessionNetwork* g_live = nullptr;

constexpr uint8_t QuitGameOpcode = 20;

}

TGameserverSessionNetwork::TGameserverSessionNetwork(QObject* parent)
    : QObject(parent)
{
    g_live = this;
}

TGameserverSessionNetwork::~TGameserverSessionNetwork()
{
    if (g_live == this)
        g_live = nullptr;
}

std::shared_ptr<TGameserverSessionNetwork> TGameserverSessionNetwork::create()
{
    return { new TGameserverSessionNetwork(), [](TGameserverSessionNetwork* connection) { connection->retire(); } };
}

TGameserverSessionNetwork* TGameserverSessionNetwork::live()
{
    return g_live;
}

void TGameserverSessionNetwork::setFrameWriter(std::function<void(const QByteArray&)> writer)
{
    m_frameWriter = std::move(writer);
}

void TGameserverSessionNetwork::setDisconnector(std::function<void()> disconnector)
{
    m_disconnector = std::move(disconnector);
}

void TGameserverSessionNetwork::setFailureHandler(std::function<void()> handler)
{
    m_failureHandler = std::move(handler);
}

void TGameserverSessionNetwork::setConnected(const bool connected)
{
    m_connected = connected;
    if (connected && !m_readTimer.isValid())
        m_readTimer.start();
}

void TGameserverSessionNetwork::notePacketRead()
{
    m_readTimer.start();
}

void TGameserverSessionNetwork::startRecording(const std::string& recordTo)
{
    if (recordTo.empty())
        return;
    std::error_code ec;
    std::filesystem::create_directory("records", ec);
    const auto path = std::filesystem::path("records") / recordTo;
    m_record = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
    m_recordStart = shared::TClockService::frameMillis();
    m_skipFirstOutput = true;
}

void TGameserverSessionNetwork::beginReplay()
{
    m_replay = true;
    m_disconnected = false;
}

void TGameserverSessionNetwork::disconnect()
{
    m_disconnected = true;
    m_connected = false;
    m_record.reset();
    // The session closes its connection; it may run inside one of the
    // connection's own signals.
    if (const auto disconnector = std::exchange(m_disconnector, {}))
        disconnector();
    m_frameWriter = {};
}

void TGameserverSessionNetwork::retire()
{
    disconnect();
    if (g_live == this)
        g_live = nullptr;
    deleteLater();
}

void TGameserverSessionNetwork::fail(const QString& message, const bool clean)
{
    if (m_disconnected)
        return;
    // The session reports the error itself (IGameSession::connectionError);
    // its game ends here.
    (void)message;
    (void)clean;
    disconnect();
    if (const auto handler = std::exchange(m_failureHandler, {}))
        handler();
}

bool TGameserverSessionNetwork::isConnected() const
{
    if (m_replay)
        return !m_disconnected;
    return m_connected && !m_disconnected;
}

bool TGameserverSessionNetwork::isConnecting() const
{
    return !m_replay && !m_connected && !m_disconnected && m_frameWriter;
}

qint64 TGameserverSessionNetwork::millisSinceRead() const
{
    if (!isConnected() || !m_readTimer.isValid())
        return -1;
    return m_readTimer.elapsed();
}

void TGameserverSessionNetwork::writeClientFrame(const QByteArray& frame)
{
    sendFrame(frame);
}

void TGameserverSessionNetwork::writeProductFrame(const QByteArray& frame)
{
    sendFrame(frame);
}

void TGameserverSessionNetwork::sendFrame(const QByteArray& frame)
{
    if (frame.isEmpty())
        return;
    if (m_replay) {
        // A recorded session ends when the player logs out of it.
        if (static_cast<uint8_t>(frame.at(0)) == QuitGameOpcode)
            fail({}, true);
        return;
    }
    if (m_disconnected || !m_connected || !m_frameWriter)
        return;
    recordOutput(frame);
    m_frameWriter(frame);
}

bool TGameserverSessionNetwork::noteServerData(const QByteArray& data)
{
    if (data.isEmpty() || data.size() > 0xffff)
        return false;
    recordInput(data);
    if (m_disconnected)
        return false;
    ++m_receivedPackets;
    m_receivedBytes += int(data.size());
    return true;
}

void TGameserverSessionNetwork::deliverPacket(const uint8_t* data, const size_t size)
{
    if (m_disconnected || !data || size == 0)
        return;
    ++m_receivedPackets;
    m_receivedBytes += int(size);
    tibia::protocol::processServerPacket(data, size);
}

void TGameserverSessionNetwork::recordInput(const QByteArray& data)
{
    if (!m_record || !*m_record)
        return;
    *m_record << "< " << (shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (const char byte : data)
        *m_record << std::setw(2) << int(uint8_t(byte));
    *m_record << std::dec << '\n';
}

void TGameserverSessionNetwork::recordOutput(const QByteArray& data)
{
    if (!m_record || !*m_record)
        return;
    if (m_skipFirstOutput) {
        m_skipFirstOutput = false;
        return;
    }
    *m_record << "> " << (shared::TClockService::frameMillis() - m_recordStart) << ' ';
    *m_record << std::hex << std::setfill('0');
    for (const char byte : data)
        *m_record << std::setw(2) << int(uint8_t(byte));
    *m_record << std::dec << '\n';
}

shared::TRsaHelper& loginRsaHelper()
{
    static shared::TRsaHelper helper;
    return helper;
}

QByteArray loginRsaPublicKey()
{
    // Crystal's login RSA modulus, big-endian; the exponent is 65537.
    static const QByteArray LoginModulus = QByteArray::fromHex(
        "9b646903b45b07ac956568d87353bd7165139dd7940703b03e6dd079399661b4"
        "a837aa60561d7ccb9452fa0080594909882ab5bca58a1a1b35f8b1059b72b121"
        "2611c6152ad3dbb3cfbee7adc142a75d3d75971509c321c5c24a5bd51fd460f0"
        "1b4e15beb0de1930528a5d3f15c1e3cbf5c401d6777e10acaab33dbe8d5b7ff5");
    return LoginModulus;
}

void installGameServerConnection()
{
    loginRsaHelper().setPublicKey(loginRsaPublicKey());
}

}
