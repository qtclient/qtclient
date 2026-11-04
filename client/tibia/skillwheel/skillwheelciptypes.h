#pragma once

// CIP-named skillwheel surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>

namespace tibia::protobuf::protocol {
class GameserverMessageGemAtelierGemRevealed;
}

namespace tibia::skillwheel {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TModListSortFilterProxyModel final : public cip::tibia::skillwheel::TModListSortFilterProxyModel
{
    Q_OBJECT
public:
    using cip::tibia::skillwheel::TModListSortFilterProxyModel::TModListSortFilterProxyModel;
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

    /// Tip Q→H wire: peel structured opcodes from `serverMessageEnqueued`.
    void onServerMessageEnqueued(int opcode, const QByteArray& classicFrame, bool applyClassic);
};

} // namespace tibia::skillwheel
