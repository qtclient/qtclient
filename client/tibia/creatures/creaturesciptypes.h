#pragma once

// CIP-named creatures surfaces (promoted from cipwire).

#include "creatures/creaturestorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
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

class TCreatureMovementSpeedCalculator final : public ::tibia::CipTyped {};

class TCreatureMovementTween final : public ::tibia::CipTyped {};

/// CIP `creatures::TCreatureProtocolMessageHandler` — tip Q→H for filled creature
/// attribute opcodes + MoveCreature (109). CIP contract has queue
/// `receivedCreature*Message` / `receivedMoveCreatureMessage` only (no
/// `handleCreature*Message` / `handleMoveCreatureMessage` moc slots) — apply via
/// `protocolSet*` / `Map::applyCreatureMove` in `onServerMessageEnqueued`, do not
/// invent handle* names.
class TCreatureProtocolMessageHandler final : public cip::tibia::creatures::TCreatureProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TCreatureProtocolMessageHandler(QObject* parent = nullptr);

    static TCreatureProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TCreatureProtocolMessageHandler* handler);

    /// Tip Q→H wire: peel structured creature-attribute opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
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
