#pragma once

// CIP-named creatures surfaces (promoted from cipwire).

#include "creatures/creaturestorage.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace creatures {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TBattleListGameActionProvider final : public ::tibia::CipTyped {};

class TBattleListRenderProvider final : public cip::tibia::creatures::TBattleListRenderProvider
{
    Q_OBJECT
public:
    using cip::tibia::creatures::TBattleListRenderProvider::TBattleListRenderProvider;
};

/// CipSoft motion types: composed on each `TCreature` (see creaturemovement*.h).

/// CIP `creatures::TCreatureProtocolMessageHandler` — qtclient Q→H for filled creature
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

private:
    /// A creature mark: timed or static square, or the attack animation.
    void handleCreatureMarksMessage(uint32_t creatureId, uint32_t kind, uint32_t value);

    /// Option gameWindowShowAttackAnimation.
    bool m_showAttackAnimation{ true };
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
public:
    /// Tip Q→H wire: PvpSituations from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
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
public:
    /// Tip Q→H wire: BuddyStatusChange from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

class TVipWidgetGameActionProvider final : public ::tibia::CipTyped {};

}
}
