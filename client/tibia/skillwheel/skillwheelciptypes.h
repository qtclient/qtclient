#pragma once

// CIP-named skillwheel surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageGemAtelierGemRevealed;
}
}
}

namespace tibia {
namespace skillwheel {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


/// TModListSortFilterProxyModel product twin lives in skillwheelmodels.h (FragmentWorkshop).

class TGridConfiguration final : public ::tibia::CipTyped
{
public:
    TGridConfiguration() = default;
};

class TSkillWheelGameActionHandler final : public cip::tibia::skillwheel::TSkillWheelGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::skillwheel::TSkillWheelGameActionHandler::TSkillWheelGameActionHandler;
};

/// CIP `skillwheel::TSkillWheelProtocolMessageHandler` — native Q→H for GemAtelierGemRevealed.
class TSkillWheelProtocolMessageHandler final : public cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TSkillWheelProtocolMessageHandler(QObject* parent = nullptr);

    static TSkillWheelProtocolMessageHandler* liveInstance();
    static void setLiveInstance(TSkillWheelProtocolMessageHandler* handler);

    /// CIP slot: GemAtelierGemRevealed (S 0xC5 / 197) — native Q→H apply (skips classic bridge).
    void handleGemAtelierGemRevealedMessage(
        const ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed& Message);

    /// Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode);

    /// The Gem Atelier requests: dismantle (0), reveal a gem of a quality 0..2
    /// (1), reshape one mod (2), lock or unlock (3) and enhance a fragment mod
    /// grade (4).
    static void dismantleGem(int gemId);
    static void revealGem(int quality);
    static void reshapeGem(int gemId, int modIndex);
    static void toggleGemLock(int gemId);
    static void enhanceModGrade(int modType, int modId);
};

}
}
