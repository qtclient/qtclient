#pragma once

#include "input/gameactiondispatch.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <memory>

namespace tibia::exaltation {

class TExaltationProtocolMessageHandler;
class TExaltationStorage;

/// The forge's game actions 293-299: sends the fusions, transfers and
/// history pages through the protocol message handler and resets the
/// storage when the dialog closes.
class TExaltationGameActionHandler final : public cip::tibia::exaltation::TExaltationGameActionHandler
{
    Q_OBJECT
public:
    TExaltationGameActionHandler(std::shared_ptr<TExaltationProtocolMessageHandler> protocolMessageHandler,
                                 std::shared_ptr<TExaltationStorage> exaltationStorage);

    bool handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    /// The router's game actions.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        if (pGameAction)
            handleGameAction(*pGameAction);
    }

    void handleExaltationForgeDialogClosedGameAction() override;
    void handleExaltationForgeHistoryNextPageGameAction() override;
    void handleExaltationForgeHistoryPreviousPageGameAction() override;

private:
    void handleExaltationForgeFusionGameAction(const input::TGameActionExaltationForgeFusion& action);
    void handleExaltationForgeConvergenceFusionGameAction(const input::TGameActionExaltationForgeConvergenceFusion& action);
    void handleExaltationForgeTransferGameAction(const input::TGameActionExaltationForgeTransfer& action);

    std::shared_ptr<TExaltationProtocolMessageHandler> m_protocolMessageHandler;
    std::shared_ptr<TExaltationStorage> m_exaltationStorage;
};

}
