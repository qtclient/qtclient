#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace prey {

class TPreyProtocolMessageHandler;

/// Turns the prey dialog's game actions into the slot's PreyAction requests.
class TPreyGameActionHandler final : public cip::tibia::prey::TPreyGameActionHandler
{
    Q_OBJECT
public:
    explicit TPreyGameActionHandler(std::shared_ptr<TPreyProtocolMessageHandler> protocolMessageHandler);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void handleRequestListRerollGameAction(const ::tibia::input::TGameActionPreySystem& gameAction);
    void handleSelectMonsterFromListGameAction(const ::tibia::input::TGameActionSelectPreyMonsterFromList& gameAction);
    void handleRequestBonusRerollGameAction(const ::tibia::input::TGameActionPreySystem& gameAction);
    void handleRequestFreezeAndShowBestiaryMonstersGameAction(const ::tibia::input::TGameActionPreySystem& gameAction);
    void handleSelectPreyMonsterFromBestiaryGameAction(const ::tibia::input::TGameActionSelectPreyMonsterFromBestiary& gameAction);
    void handleRequestChangeAutoPreyExtendTypeGameAction(const ::tibia::input::TGameActionChangeAutoPreyExtendType& gameAction);

private:
    std::shared_ptr<TPreyProtocolMessageHandler> m_protocolMessageHandler;
};

}
}
