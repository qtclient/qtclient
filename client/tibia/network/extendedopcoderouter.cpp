#include "extendedopcoderouter.h"

#include "tibia/protocol/extendedopcode.h"

#include <tibia/world/game.h>
#include <client/protocolgame.h>
#include <framework/net/inputmessage.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace tibia::network {

namespace {

constexpr size_t MaxPacketSize = 65000;

std::any toAny(const bool handled)
{
    return handled ? std::any(true) : std::any{};
}

}

ExtendedOpcodeRouter::ExtendedOpcodeRouter()
{
    m_readerSubscription = tibia::protocol::ExtendedOpcodeReader::instance().subscribe(
        [this](const uint8_t opcode, const std::string& buffer) { onExtendedOpcode(opcode, buffer); });
}

ExtendedOpcodeRouter::~ExtendedOpcodeRouter()
{
    tibia::protocol::ExtendedOpcodeReader::instance().unsubscribe(m_readerSubscription);
}

bool ExtendedOpcodeRouter::registerOpcode(const uint8_t opcode, OpcodeHandler handler)
{
    if (m_opcode[opcode] || !handler)
        return false;
    m_opcode[opcode] = std::move(handler);
    updateOpcodeHook();
    return true;
}

void ExtendedOpcodeRouter::unregisterOpcode(const uint8_t opcode)
{
    m_opcode[opcode] = nullptr;
    updateOpcodeHook();
}

bool ExtendedOpcodeRouter::registerExtendedOpcode(const uint8_t opcode, RawHandler handler)
{
    if (m_raw[opcode] || !handler)
        return false;
    m_raw[opcode] = std::move(handler);
    return true;
}

void ExtendedOpcodeRouter::unregisterExtendedOpcode(const uint8_t opcode)
{
    m_raw[opcode] = nullptr;
}

bool ExtendedOpcodeRouter::registerExtendedJsonOpcode(const uint8_t opcode, JsonHandler handler)
{
    if (m_json[opcode] || !handler)
        return false;
    m_json[opcode] = std::move(handler);
    return true;
}

void ExtendedOpcodeRouter::unregisterExtendedJsonOpcode(const uint8_t opcode)
{
    m_json[opcode] = nullptr;
    m_jsonParts[opcode].reset();
}

// Every game opcode passes through ProtocolGame's onOpcode hook, so it is only
// subscribed while a handler needs it.
void ExtendedOpcodeRouter::updateOpcodeHook()
{
    const bool needed = std::ranges::any_of(m_opcode, [](const auto& h) { return static_cast<bool>(h); });
    if (!needed) {
        m_opcodeHook = {};
        return;
    }
    m_opcodeHook = client::gamecore::Hook("object", "onOpcode", [this](LuaObject* self, const std::vector<std::any>& args) {
        if (!dynamic_cast<ProtocolGame*>(self) || args.size() < 2)
            return std::any{};
        const auto opcode = static_cast<uint8_t>(client::gamecore::argInt(args, 0));
        const auto& handler = m_opcode[opcode];
        const auto* msg = std::any_cast<InputMessagePtr>(&args[1]);
        if (!handler || !msg)
            return std::any{};
        handler(opcode, *msg);
        return toAny(true);
    });
}

void ExtendedOpcodeRouter::onExtendedOpcode(const uint8_t opcode, const std::string& buffer)
{
    if (const auto& raw = m_raw[opcode])
        raw(opcode, buffer);

    const auto& json = m_json[opcode];
    if (!json)
        return;

    // O: one message, S: first part, P: middle part, E: last part. Anything else
    // is a whole message without a status byte.
    const char status = buffer.empty() ? '\0' : buffer.front();
    auto& parts = m_jsonParts[opcode];
    if (status == 'S' || status == 'P' || status == 'E') {
        if (status == 'S' || !parts)
            parts = std::string{};
        parts->append(buffer, 1);
        if (status != 'E')
            return;
    } else if (status == 'O') {
        parts = buffer.substr(1);
    } else {
        parts = buffer;
    }

    const std::string raw = std::move(*parts);
    parts.reset();
    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(raw.data(), static_cast<qsizetype>(raw.size())), &error);
    // Crystal's NetworkMessage::addString transcodes every string to ISO-8859-1.
    if (error.error == QJsonParseError::IllegalUTF8String)
        doc = QJsonDocument::fromJson(QString::fromLatin1(raw.data(), static_cast<qsizetype>(raw.size())).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning().noquote() << "extended JSON opcode" << opcode << "invalid:" << error.errorString();
        return;
    }
    const JsonHandler handler = json;
    handler(opcode, doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()));
}

bool ExtendedOpcodeRouter::sendExtendedOpcode(const uint8_t opcode, const std::string& buffer)
{
    return tibia::protocol::ExtendedOpcodeReader::instance().send(opcode, buffer);
}

bool ExtendedOpcodeRouter::sendExtendedJsonOpcode(const uint8_t opcode, const QJsonValue& data)
{
    const QByteArray bytes = data.isArray() ? QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact)
                                            : QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact);
    // Crystal's NetworkMessage::getString decodes ISO-8859-1.
    const std::string buffer = QString::fromUtf8(bytes).toLatin1().toStdString();
    if (buffer.size() <= MaxPacketSize)
        return sendExtendedOpcode(opcode, buffer);

    for (size_t offset = 0; offset < buffer.size(); offset += MaxPacketSize) {
        const char status = offset == 0 ? 'S' : (offset + MaxPacketSize >= buffer.size() ? 'E' : 'P');
        if (!sendExtendedOpcode(opcode, status + buffer.substr(offset, MaxPacketSize)))
            return false;
    }
    return true;
}

}
