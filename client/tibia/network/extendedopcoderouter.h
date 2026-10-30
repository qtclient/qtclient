#pragma once

#include "client/gamecore.h"

#include <QJsonValue>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class InputMessage;

namespace tibia::network {

/// Server-to-client custom opcodes, what modules/gamelib/protocolgame.lua did
/// for Lua: game opcodes the engine does not parse (ProtocolGame.registerOpcode),
/// raw extended opcodes and JSON extended opcodes (S/P/E chunked, 65000-byte parts).
class ExtendedOpcodeRouter
{
public:
    using OpcodeHandler = std::function<void(uint8_t opcode, const std::shared_ptr<InputMessage>& msg)>;
    using RawHandler = std::function<void(uint8_t opcode, const std::string& buffer)>;
    using JsonHandler = std::function<void(uint8_t opcode, const QJsonValue& data)>;

    ExtendedOpcodeRouter();
    ~ExtendedOpcodeRouter();

    /// A game opcode handled here instead of by ProtocolGame (the handler reads the whole message).
    bool registerOpcode(uint8_t opcode, OpcodeHandler handler);
    void unregisterOpcode(uint8_t opcode);
    bool registerExtendedOpcode(uint8_t opcode, RawHandler handler);
    void unregisterExtendedOpcode(uint8_t opcode);
    bool registerExtendedJsonOpcode(uint8_t opcode, JsonHandler handler);
    void unregisterExtendedJsonOpcode(uint8_t opcode);

    /// False when there is no game connection or the server has not enabled
    /// extended opcodes yet (it sends opcode 0 at login).
    static bool sendExtendedOpcode(uint8_t opcode, const std::string& buffer);
    static bool sendExtendedJsonOpcode(uint8_t opcode, const QJsonValue& data);

private:
    void onExtendedOpcode(uint8_t opcode, const std::string& buffer);
    void updateOpcodeHook();

    std::array<OpcodeHandler, 256> m_opcode;
    std::array<RawHandler, 256> m_raw;
    std::array<JsonHandler, 256> m_json;
    std::array<std::optional<std::string>, 256> m_jsonParts;
    int m_readerSubscription = 0;
    client::gamecore::Hook m_opcodeHook;
};

}
