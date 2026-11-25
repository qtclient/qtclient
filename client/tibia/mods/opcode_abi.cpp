// Host API v9: the extended opcode table (abi/opcode_api.h) over the host's
// protocol::ExtendedOpcodeReader (received messages, the live connection's sender) and its
// JSON chunked send. A subscription holds the reader subscription that also asks the login
// for extended opcodes (game/gameservergamesession.cpp).
#include "mods/abi_internal.h"

#include "client/gameclient.h"
#include "protocol/extendedopcode.h"
#include "protocol/extendedopcode_json.h"

#include <QJsonValue>

#include <functional>
#include <string>

namespace tibia::mods {
namespace {

struct TOpcodeWatch
{
    Subscribers<std::function<void(uint8_t, const std::string&)>> listeners;
    int readerSubscription = 0;
};

TOpcodeWatch& watch()
{
    static TOpcodeWatch w;
    return w;
}

void updateReaderSubscription()
{
    TOpcodeWatch& w = watch();
    auto& reader = tibia::protocol::ExtendedOpcodeReader::instance();
    if (!w.listeners.empty() && w.readerSubscription == 0) {
        w.readerSubscription = reader.subscribe([](const uint8_t opcode, const std::string& buffer) {
            watch().listeners.call(opcode, buffer);
        });
    } else if (w.listeners.empty() && w.readerSubscription != 0) {
        reader.unsubscribe(w.readerSubscription);
        w.readerSubscription = 0;
    }
}

int opLive()
{
    const HostObjects* host = hostObjects();
    return host && host->gameClient && host->gameClient->opcodes() ? 1 : 0;
}

int opSubscribe(int opcode, QtClientOpcodeFn fn, void* user)
{
    if (!fn || opcode < -1 || opcode > 255)
        return 0;
    const int id = watch().listeners.add([opcode, fn, user](const uint8_t op, const std::string& buffer) {
        if (opcode == -1 || opcode == op)
            fn(user, op, buffer.data(), buffer.size());
    });
    updateReaderSubscription();
    return id;
}

void opUnsubscribe(int id)
{
    if (watch().listeners.remove(id))
        updateReaderSubscription();
}

int opSend(uint8_t opcode, const char* data, size_t len)
{
    if (!data && len != 0)
        return 0;
    return tibia::protocol::ExtendedOpcodeReader::instance().send(opcode, std::string(data ? data : "", len)) ? 1 : 0;
}

int opSendJson(uint8_t opcode, const char* json_utf8)
{
    QJsonValue value;
    if (!json_utf8 || !parseJsonValue(json_utf8, value, false) || (!value.isObject() && !value.isArray()))
        return 0;
    return tibia::protocol::sendExtendedJsonOpcode(opcode, value) ? 1 : 0;
}

const QtClientOpcodeApiV1 g_opcodeApi = {
    .struct_size = sizeof(QtClientOpcodeApiV1),
    .live = &opLive,
    .subscribe = &opSubscribe,
    .unsubscribe = &opUnsubscribe,
    .send = &opSend,
    .send_json = &opSendJson,
};

} // namespace

const QtClientOpcodeApiV1* opcodeApiV1()
{
    return &g_opcodeApi;
}

} // namespace tibia::mods
