#include "tibia/protocol/extendedopcode_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace tibia::protocol {
namespace {

constexpr size_t kMaxPacketSize = 65000;

bool sendRaw(const uint8_t opcode, const std::string& buffer)
{
    return ExtendedOpcodeReader::instance().send(opcode, buffer);
}

} // namespace

bool sendExtendedJsonOpcode(const uint8_t opcode, const QJsonValue& data)
{
    const QByteArray bytes = data.isArray() ? QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact)
                                            : QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact);
    const std::string buffer = QString::fromUtf8(bytes).toLatin1().toStdString();
    if (buffer.size() <= kMaxPacketSize)
        return sendRaw(opcode, buffer);

    for (size_t offset = 0; offset < buffer.size(); offset += kMaxPacketSize) {
        const char status = offset == 0 ? 'S' : (offset + kMaxPacketSize >= buffer.size() ? 'E' : 'P');
        if (!sendRaw(opcode, status + buffer.substr(offset, kMaxPacketSize)))
            return false;
    }
    return true;
}

} // namespace tibia::protocol
