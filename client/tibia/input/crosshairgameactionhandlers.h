#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <cip_gen.h>

#include <client/const.h>
#include <protocol.pb.h>

#include <functional>
#include <memory>

namespace tibia {
namespace input {

/// CIP input::TUseWithGameActionHandler (USE_WITH, USE_ON_CREATURE,
/// TARGET_SELECTED, CANCEL_USE_WITH) plus USE_ON_TARGET / USE_ON_YOURSELF from
/// TCreaturesGameActionHandler; a router slot of the game window. A use with
/// without a target waits (+0x20) for the next TARGET_SELECTED.
class TUseWithGameActionHandler final : public cip::tibia::input::TUseWithGameActionHandler
{
    Q_OBJECT
public:
    explicit TUseWithGameActionHandler(QObject* parent = nullptr);
    ~TUseWithGameActionHandler() override;
    TUseWithGameActionHandler(const TUseWithGameActionHandler&) = delete;
    TUseWithGameActionHandler& operator=(const TUseWithGameActionHandler&) = delete;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);
    void sendUseTwoObjects(tibia::protobuf::protocol::GameclientMessageUseTwoObjects UseTwoObjectsMessage);
    void sendUseOnCreature(tibia::protobuf::protocol::GameclientMessageUseOnCreature UseOnCreatureMessage);

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    void handleUseWith(const std::shared_ptr<const TGameActionUseWith>& action);
    void handleUseOnCreature(const TGameActionUseOnCreature& action);
    void handleTargetSelected(const TTargetSelectedPtr& target);
    void startTargetSelectionBridge();

    std::shared_ptr<const TGameActionUseWith> m_pPendingAction;
};

/// CIP input::TCrossHairSpellGameActionHandler (CROSSHAIR_SPELL,
/// TARGET_SELECTED, CANCEL_CROSSHAIR_SPELL); a router slot of the game window.
/// A spell without a target waits (+0x30) for the next TARGET_SELECTED.
class TCrossHairSpellGameActionHandler final : public cip::tibia::input::TCrossHairSpellGameActionHandler
{
    Q_OBJECT
public:
    explicit TCrossHairSpellGameActionHandler(QObject* parent = nullptr);
    ~TCrossHairSpellGameActionHandler() override;
    TCrossHairSpellGameActionHandler(const TCrossHairSpellGameActionHandler&) = delete;
    TCrossHairSpellGameActionHandler& operator=(const TCrossHairSpellGameActionHandler&) = delete;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);
    /// The spell's words aimed (aim mode 1) at the target's field.
    void sendTalkMessage(const ::tibia::protobuf::protocol::GameclientMessageTalk& TalkMessage);

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    /// Without a target the target is selected first. A target on the world
    /// map is aimed at on its field, a creature in the battle list on the
    /// creature's field; other targets are dropped.
    void handleCrossHairSpell(const std::shared_ptr<const TGameActionCrossHairSpell>& action);
    void handleTargetSelected(const TTargetSelectedPtr& target);

    std::shared_ptr<const TGameActionCrossHairSpell> m_pPendingAction;
};

}
}
