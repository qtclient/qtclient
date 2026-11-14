#include "extendedopcoderouter.h"

#include "tibia/protocol/extendedopcode.h"
#include "tibia/protocol/extendedopcode_json.h"

#include <tibia/game/gamesession.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace tibia::network {

using Args = tibia::game::Args;

namespace {

constexpr size_t MaxPacketSize = 65000;

}

ExtendedOpcodeRouter::ExtendedOpcodeRouter() = default;

ExtendedOpcodeRouter::~ExtendedOpcodeRouter()
{
    if (m_readerSubscription)
        tibia::protocol::ExtendedOpcodeReader::instance().unsubscribe(m_readerSubscription);
}

// The reader subscription doubles as the login request for extended opcodes,
// so it is only held while a raw or JSON handler is registered.
void ExtendedOpcodeRouter::updateReaderSubscription()
{
    const bool needed = std::ranges::any_of(m_raw, [](const auto& h) { return static_cast<bool>(h); })
        || std::ranges::any_of(m_json, [](const auto& h) { return static_cast<bool>(h); });
    auto& reader = tibia::protocol::ExtendedOpcodeReader::instance();
    if (needed && !m_readerSubscription) {
        m_readerSubscription = reader.subscribe(
            [this](const uint8_t opcode, const std::string& buffer) { onExtendedOpcode(opcode, buffer); });
    } else if (!needed && m_readerSubscription) {
        reader.unsubscribe(m_readerSubscription);
        m_readerSubscription = 0;
    }
}

bool ExtendedOpcodeRouter::registerExtendedOpcode(const uint8_t opcode, RawHandler handler)
{
    if (m_raw[opcode] || !handler)
        return false;
    m_raw[opcode] = std::move(handler);
    updateReaderSubscription();
    return true;
}

void ExtendedOpcodeRouter::unregisterExtendedOpcode(const uint8_t opcode)
{
    m_raw[opcode] = nullptr;
    updateReaderSubscription();
}

bool ExtendedOpcodeRouter::registerExtendedJsonOpcode(const uint8_t opcode, JsonHandler handler)
{
    if (m_json[opcode] || !handler)
        return false;
    m_json[opcode] = std::move(handler);
    updateReaderSubscription();
    return true;
}

void ExtendedOpcodeRouter::unregisterExtendedJsonOpcode(const uint8_t opcode)
{
    m_json[opcode] = nullptr;
    m_jsonParts[opcode].reset();
    updateReaderSubscription();
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
    return tibia::protocol::sendExtendedJsonOpcode(opcode, data);
}

}
