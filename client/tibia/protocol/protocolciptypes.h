#pragma once

// CIP-named protocol surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::protocol {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TGameActionContextHelper final : public ::tibia::CipTyped {
public:
    TGameActionContextHelper() = default;
};

class TProtobufClientMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufClientMessageTranslator() = default;
};

class TProtobufObjectPositionHelper final : public ::tibia::CipTyped {
public:
    TProtobufObjectPositionHelper() = default;
};

class TProtobufServerMessageTranslator final : public ::tibia::CipTyped {
public:
    TProtobufServerMessageTranslator() = default;
};

class TProtocolClientMessageProcessor final : public ::tibia::CipTyped {
public:
    TProtocolClientMessageProcessor() = default;
};

class TProtocolMessageQueue final : public cip::tibia::protocol::TProtocolMessageQueue
{
    Q_OBJECT
public:
    using cip::tibia::protocol::TProtocolMessageQueue::TProtocolMessageQueue;
};

class TProtocolReader final : public ::tibia::CipTyped {
public:
    TProtocolReader() = default;
};

class TProtocolServerMessageProcessor final : public ::tibia::CipTyped {
public:
    TProtocolServerMessageProcessor() = default;
};

class TProtocolWriter final : public ::tibia::CipTyped {
public:
    TProtocolWriter() = default;
};

class TUnknownGameserverException final : public ::tibia::CipTyped {
public:
    TUnknownGameserverException() = default;
};

} // namespace tibia::protocol

