#pragma once

// CIP-named creatures surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::creatures {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBattleListGameActionProvider final : public ::tibia::CipTyped {};

class TBattleListRenderProvider final : public cip::tibia::creatures::TBattleListRenderProvider
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TBattleListRenderProvider::TBattleListRenderProvider;
};

class TCreature final : public cip::tibia::creatures::TCreature
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TCreature::TCreature;
};

class TCreatureMovementSpeedCalculator final : public ::tibia::CipTyped {};

class TCreatureMovementTween final : public ::tibia::CipTyped {};

class TCreatureProtocolMessageHandler final : public cip::tibia::creatures::TCreatureProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TCreatureProtocolMessageHandler::TCreatureProtocolMessageHandler;
};

class TCreatureStorage final : public cip::tibia::creatures::TCreatureStorage
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TCreatureStorage::TCreatureStorage;
};

class TCreaturesGameActionHandler final : public cip::tibia::creatures::TCreaturesGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TCreaturesGameActionHandler::TCreaturesGameActionHandler;
};

class TInspectPlayerGameActionProvider final : public ::tibia::CipTyped {};

class TPartyGameActionProvider final : public ::tibia::CipTyped {};

class TUnjustifiedPointsProtocolMessageHandler final : public cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler::TUnjustifiedPointsProtocolMessageHandler;
};

class TUnjustifiedPointsStorage final : public cip::tibia::creatures::TUnjustifiedPointsStorage
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TUnjustifiedPointsStorage::TUnjustifiedPointsStorage;
};

class TVipGameActionHandler final : public cip::tibia::creatures::TVipGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TVipGameActionHandler::TVipGameActionHandler;
};

class TVipProtocolMessageHandler final : public cip::tibia::creatures::TVipProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TVipProtocolMessageHandler::TVipProtocolMessageHandler;
};

class TVipWidgetGameActionProvider final : public ::tibia::CipTyped {};

} // namespace tibia::creatures
