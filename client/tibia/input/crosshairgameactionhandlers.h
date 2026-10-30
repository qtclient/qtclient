#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <cip_gen.h>

#include <client/const.h>

namespace tibia::input {

/// A local say (CIP FUN_100993990, the chat handler of
/// SEND_CHAT_MESSAGE_WITH_SPEECH_VOLUME): a crossHairTarget spell is cast by
/// its ESpellCastOptionalTargetType: at the mouse cursor (2) publishes
/// SPELL_AT_MOUSE_CURSOR, at target or self (3) talks with aim 3, else (0, 1)
/// publishes CROSSHAIR_SPELL. Anything else is a plain talk.
void say(Otc::MessageMode mode, const QString& text, int spellCastType);

/// CIP's TUseWithGameActionHandler (0x100e77d20: USE_WITH, TARGET_SELECTED,
/// CANCEL_USE_WITH) plus USE_ON_* from TCreaturesGameActionHandler.
class TUseWithGameActionHandler final : public cip::tibia::input::TUseWithGameActionHandler
{
    Q_OBJECT
public:
    explicit TUseWithGameActionHandler(QObject* parent = nullptr);
    ~TUseWithGameActionHandler() override;
    TUseWithGameActionHandler(const TUseWithGameActionHandler&) = delete;
    TUseWithGameActionHandler& operator=(const TUseWithGameActionHandler&) = delete;
};

/// CIP's TCrossHairSpellGameActionHandler (0x100e78cf0: CROSSHAIR_SPELL,
/// TARGET_SELECTED, CANCEL_CROSSHAIR_SPELL).
class TCrossHairSpellGameActionHandler final : public cip::tibia::input::TCrossHairSpellGameActionHandler
{
    Q_OBJECT
public:
    explicit TCrossHairSpellGameActionHandler(QObject* parent = nullptr);
    ~TCrossHairSpellGameActionHandler() override;
    TCrossHairSpellGameActionHandler(const TCrossHairSpellGameActionHandler&) = delete;
    TCrossHairSpellGameActionHandler& operator=(const TCrossHairSpellGameActionHandler&) = delete;
};

/// Owns CIP's use-with and crosshair-spell handlers for the action-bar lifetime.
class TCrossHairGameActionHandlers : public ::tibia::CipTyped
{
public:
    TCrossHairGameActionHandlers();
    ~TCrossHairGameActionHandlers() = default;
    TCrossHairGameActionHandlers(const TCrossHairGameActionHandlers&) = delete;
    TCrossHairGameActionHandlers& operator=(const TCrossHairGameActionHandlers&) = delete;

private:
    TUseWithGameActionHandler m_useWith;
    TCrossHairSpellGameActionHandler m_crossHairSpell;
};

}
