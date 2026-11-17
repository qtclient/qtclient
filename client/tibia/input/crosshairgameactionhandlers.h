#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <cip_gen.h>

#include <client/const.h>
#include <protocol.pb.h>

#include <memory>

namespace tibia {
namespace input {

/// A local say (CIP , the chat handler of
/// SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME): crossHairTarget spells use
/// ESpellCastOptionalTargetType (mouse → SPELL_AT_MOUSE_CURSOR, target → aim 3,
/// else CROSSHAIR_SPELL). aimattarget waves use the same cast types toward the
/// mouse tile or attack target (needDirection on the server). Else plain talk.
void say(tibia::client::MessageMode mode, const QString& text, int spellCastType);

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

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

private:
    void handleCrossHairSpell(const std::shared_ptr<const TGameActionCrossHairSpell>& action);
    void handleTargetSelected(const TTargetSelectedPtr& target);

    std::shared_ptr<const TGameActionCrossHairSpell> m_pPendingAction;
};

}
}
