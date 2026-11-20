#include "network/gameserverpacketprocessor.h"

#include "network/gameserverconnection.h"
#include "protocol/extendedopcode.h"
#include "protocol/protocolciptypes.h"

#include <QtGlobal>
#include <format>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <protocol.pb.h>

#include <algorithm>
#include <ctime>
#include <random>
#include <string>

namespace tibia::network {

namespace {

TGameserverNetworkPacketProcessor* g_live = nullptr;

constexpr uint8_t kQuitGameOpcode = 20;

}

TGameserverNetworkPacketProcessor::TGameserverNetworkPacketProcessor(QObject* parent)
    : cip::tibia::network::TGameserverNetworkPacketProcessor(parent)
{
    g_live = this;
}

TGameserverNetworkPacketProcessor::~TGameserverNetworkPacketProcessor()
{
    releaseConnection();
    if (g_live == this)
        g_live = nullptr;
}

TGameserverNetworkPacketProcessor* TGameserverNetworkPacketProcessor::live()
{
    return g_live;
}

void TGameserverNetworkPacketProcessor::connectToGameserver(const TGameserverLoginData& login)
{
    m_login = login;
    m_disconnected = false;
    m_replay = false;
    releaseConnection();
    auto* connection = new TGameserverNetworkPacketConnection();
    m_connection = connection;
    if (!login.recordTo.empty())
        connection->setRecordFile(QString::fromStdString(login.recordTo));
    // The browser build reaches the game server's WebSocket port; the login
    // reply carries it, so the port is used as given.
    connection->open(this, QString::fromStdString(login.host), login.port, true);
}

void TGameserverNetworkPacketProcessor::beginReplay()
{
    m_replay = true;
    m_disconnected = false;
    m_firstPacket = false; // recorded packets have no leading padding byte
    tibia::game::notify("onConnect");
}

void TGameserverNetworkPacketProcessor::disconnect()
{
    m_disconnected = true;
    releaseConnection();
}

void TGameserverNetworkPacketProcessor::retire()
{
    disconnect();
    if (g_live == this)
        g_live = nullptr;
    deleteLater();
}

void TGameserverNetworkPacketProcessor::releaseConnection()
{
    if (!m_connection)
        return;
    TGameserverNetworkPacketConnection* connection = m_connection;
    m_connection = nullptr;
    connection->shutdown();
    connection->deleteLater();
}

void TGameserverNetworkPacketProcessor::fail(const QString& message, const bool clean)
{
    if (m_disconnected)
        return;
    m_disconnected = true;
    releaseConnection();
    tibia::game::processConnectionError(message.isEmpty() && !clean ? std::string("connection lost") : message.toStdString(),
                                        clean);
}

void TGameserverNetworkPacketProcessor::connectionFailed(const QString& message, const bool clean)
{
    fail(message, clean);
}

bool TGameserverNetworkPacketProcessor::isConnected() const
{
    if (m_replay)
        return !m_disconnected;
    return m_connection && m_connection->connected();
}

bool TGameserverNetworkPacketProcessor::isConnecting() const
{
    return !m_replay && m_connection && m_connection->connecting();
}

qint64 TGameserverNetworkPacketProcessor::millisSinceRead() const
{
    return m_connection ? m_connection->millisSinceRead() : -1;
}

void TGameserverNetworkPacketProcessor::connectionEstablished()
{
    if (m_disconnected || !m_connection)
        return;
    // The game server routes the connection by world name first.
    QByteArray world = QByteArray::fromStdString(tibia::game::getWorldName());
    world.append('\n');
    write(world, true);
    m_connection->enableSequenced();
    tibia::game::notify("onConnect");
    m_firstPacket = true;
    m_connection->enableChecksum();
}

void TGameserverNetworkPacketProcessor::sendLogin(const uint32_t challengeTimestamp, const uint8_t challengeRandom)
{
    using namespace ::tibia::protobuf::protocol;
    if (!m_connection)
        return;
    m_connection->generateXtea(m_xteaKey.data());

    auto message = tibia::protocol::newGameclientMessage(10);
    auto* login = message->MutableExtension(GameclientMessageLogin::gameclient_message_login);
    login->set_client_os(tibia::game::clientOs());
    login->set_protocol_version(tibia::game::getProtocolVersion());
    login->set_client_version(tibia::game::clientRevision());
    login->set_client_version_string(std::to_string(tibia::game::clientRevision()));
    login->set_asset_identifier(tibia::game::assetIdentifier());
    login->set_preview_state(0);
    for (const uint32_t part : m_xteaKey)
        login->add_xtea_key(part);
    login->set_gamemaster(false);
    login->set_session_key(m_login.sessionKey);
    login->set_character_name(m_login.characterName);
    login->set_challenge_timestamp(challengeTimestamp);
    login->set_challenge_random(challengeRandom);
    // A pack that listens for extended opcodes asks for the product transport
    // in the RSA padding; the CIP client leaves the padding zero.
    if (tibia::protocol::ExtendedOpcodeReader::instance().hasListeners())
        login->set_product_transport("ExtendedOpcodes");

    m_connection->enableChecksum();
    // Encoded and written synchronously, so XTEA starts with the next frame.
    tibia::protocol::sendGameclientMessage(std::move(message));
    if (!m_connection)
        return;
    m_connection->enableXtea();
    m_connection->enableSequenced();
}

void TGameserverNetworkPacketProcessor::writeClientFrame(const QByteArray& frame)
{
    write(frame, false);
}

void TGameserverNetworkPacketProcessor::writeProductFrame(const QByteArray& frame)
{
    write(frame, false);
}

void TGameserverNetworkPacketProcessor::write(const QByteArray& frame, const bool raw)
{
    if (frame.isEmpty())
        return;
    if (m_replay) {
        // A recorded session ends when the player logs out of it.
        if (uint8_t(frame[0]) == kQuitGameOpcode)
            fail({}, true);
        return;
    }
    if (frame.size() > 0xffff) {
        return;
    }
    if (m_connection)
        m_connection->writePayload(reinterpret_cast<const uint8_t*>(frame.constData()), uint16_t(frame.size()), raw);
}

void TGameserverNetworkPacketProcessor::deliverPacket(const uint8_t* data, size_t size)
{
    if (m_disconnected || !data || size == 0)
        return;
    ++m_receivedPackets;
    m_receivedBytes += int(size);
    if (m_firstPacket) {
        m_firstPacket = false;
        // The first packet of a connection starts with one padding byte.
        ++data;
        if (--size == 0)
            return;
    }
    tibia::protocol::processServerPacket(data, size);
}

}
