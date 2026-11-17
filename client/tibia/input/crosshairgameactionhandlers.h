#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <cip_gen.h>

#include <client/const.h>

namespace tibia {
namespace input {

/// A local say (CIP , the chat handler of
/// SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME): crossHairTarget spells use
/// ESpellCastOptionalTargetType (mouse → SPELL_AT_MOUSE_CURSOR, target → aim 3,
/// else CROSSHAIR_SPELL). aimattarget waves use the same cast types toward the
/// mouse tile or attack target (needDirection on the server). Else plain talk.
void say(tibia::client::MessageMode mode, const QString& text, int spellCastType);

/// CIP input::TUseWithGameActionHandler (USE_WITH, TARGET_SELECTED) plus
/// USE_ON_* from TCreaturesGameActionHandler; a router slot of the game window.
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

public Q_SLOTS:
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
};

/// CIP input::TCrossHairSpellGameActionHandler (CROSSHAIR_SPELL); a router slot
/// of the game window.
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
};

}
}
