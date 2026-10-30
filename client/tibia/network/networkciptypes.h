#pragma once

// CIP-named network surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::network {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TDualConnectionInfo final : public ::tibia::CipTyped {};

class TDualConnectionUrlInfo final : public ::tibia::CipTyped {};

class TGameserverNetworkPacketProcessor final : public cip::tibia::network::TGameserverNetworkPacketProcessor
{
    Q_OBJECT
public:
    using cip::tibia::network::TGameserverNetworkPacketProcessor::TGameserverNetworkPacketProcessor;
};

class TGameserverNetworkPacketRawDataProcessor final : public ::tibia::CipTyped {};

class TGameserverNetworkPacketReader final : public ::tibia::CipTyped {};

class TGameserverNetworkPacketSequenceFlowProcessor final : public cip::tibia::network::TGameserverNetworkPacketSequenceFlowProcessor
{
    Q_OBJECT
public:
    using cip::tibia::network::TGameserverNetworkPacketSequenceFlowProcessor::TGameserverNetworkPacketSequenceFlowProcessor;
};

class TGameserverNetworkPacketWriter final : public ::tibia::CipTyped {};

class TGameserverTCPConnection final : public cip::tibia::network::TGameserverTCPConnection
{
    Q_OBJECT
public:
    using cip::tibia::network::TGameserverTCPConnection::TGameserverTCPConnection;
};

class TNetworkQualityProtocolMessageHandler final : public cip::tibia::network::TNetworkQualityProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::network::TNetworkQualityProtocolMessageHandler::TNetworkQualityProtocolMessageHandler;
};

class TUnencryptedRawMessageStream final : public ::tibia::CipTyped {};

} // namespace tibia::network

