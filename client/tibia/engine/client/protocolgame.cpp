/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "framework/net/inputmessage.h"
#include "framework/net/outputmessage.h"
#include "protocolgame.h"
#include <tibia/game/gamesession.h>

#include <QByteArray>
#include <algorithm>
#include <string_view>

void ProtocolGame::login(const std::string_view accountName, const std::string_view accountPassword, const std::string_view host, uint16_t port,
                         const std::string_view characterName, const std::string_view authenticatorToken, const std::string_view sessionKey)
{
    m_accountName = accountName;
    m_accountPassword = accountPassword;
    m_authenticatorToken = authenticatorToken;
    m_sessionKey = sessionKey;
    m_characterName = characterName;

#ifndef __EMSCRIPTEN__
    connect(host, port);
#else
    // Browser can only speak Canary's WebSocket game port.
    // Do NOT remap 7172→443 here: that assumes production TLS termination and
    // turns a mistaken TCP advertisement into ws://host:443 garbage packets
    // (Unhandled opcode 0x09 / invalid packet size). Prefer correct port from
    // login JSON (websocketport) instead.
    connect(host, port, true);
#endif
}

void ProtocolGame::beginReplay()
{
    setReplay(true);
    m_disconnected = false;
    onConnect();
}

void ProtocolGame::onConnect()
{
    m_firstRecv = true;
    Protocol::onConnect();

    m_localPlayer = tibia::game::localPlayer();

    if (g_game.getFeature(tibia::client::GameProtocolChecksum))
        enableChecksum();

    if (!g_game.getFeature(tibia::client::GameChallengeOnLogin))
        sendLoginPacket(0, 0);

    recv();
}

void ProtocolGame::onRecv(const InputMessagePtr& inputMessage)
{
    m_recivedPackeds += 1;
    m_recivedPackedsSize += inputMessage->getMessageSize();

    if (m_firstRecv) {
        m_firstRecv = false;

        // Recorded packets are stored without these header bytes.
        if (isReplay()) {
        } else if (g_game.getClientVersion() >= 1405) {
            inputMessage->getU8(); // padding
        } else if (g_game.getFeature(tibia::client::GameMessageSizeCheck)) {
            const int size = inputMessage->getU16();
            if (size != inputMessage->getUnreadSize()) {
                g_logger.traceError("invalid message size");
                return;
            }
        }
    }

    parseMessage(inputMessage);
    recv();
}

void ProtocolGame::onError(const std::error_code& error)
{
    g_game.processConnectionError(error);
    disconnect();
}

void ProtocolGame::applyClassicServerFrame(const QByteArray& classicFrame)
{
    if (classicFrame.isEmpty())
        return;
    m_applyingClassicBridge = true;
    const auto msg = std::make_shared<InputMessage>();
    msg->assignBody(reinterpret_cast<const uint8_t*>(classicFrame.constData()),
                    uint16_t(std::min(classicFrame.size(), qsizetype(0xffff))));
    parseMessage(msg);
    m_applyingClassicBridge = false;
}

void ProtocolGame::writeClassicClientFrame(const QByteArray& classicFrame)
{
    if (classicFrame.isEmpty() || m_replay)
        return;
    m_writingClientLoadSplit = true;
    const auto msg = std::make_shared<OutputMessage>();
    msg->addBytes(std::string_view(classicFrame.constData(), size_t(classicFrame.size())));
    send(msg);
    m_writingClientLoadSplit = false;
}
