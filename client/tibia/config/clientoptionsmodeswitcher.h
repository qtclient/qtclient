#pragma once

#include "input/gameactiondispatch.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <functional>

namespace tibia {
namespace config {

class TClientOptions;
class TSidebarOptions;

/// SWITCH_CLIENTOPTIONS_MODE (published by the tutorial): mode 1 saves the
/// user's options and loads the tutorial's, mode 0 loads the user's back.
/// Both set the left smart click control scheme, announce every option, show
/// (mode 0) or hide (mode 1) the session's control buttons and ask the
/// sidebar to reconfigure itself.
class TClientOptionsModeSwitcher final : public cip::tibia::config::TClientOptionsModeSwitcher
{
    Q_OBJECT
public:
    explicit TClientOptionsModeSwitcher(TClientOptions* options, QObject* parent = nullptr);
    ~TClientOptionsModeSwitcher() override;

    /// The session's per-character sidebar options (null without a session).
    void connectToGameSession(std::function<TSidebarOptions*()> sidebarOptions);

    void handleGameAction(const input::IGameAction& action);

private:
    void switchClientOptionsMode(int currentMode, int requestedMode);
    void setControlButtonsVisible(bool visible);

    TClientOptions* m_options = nullptr;
    std::function<TSidebarOptions*()> m_sidebarOptions;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
