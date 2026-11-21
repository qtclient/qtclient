#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "skillwheel/skillwheelciptypes.h"
#include "skillwheel/skillwheelcontrollers.h"

#include <QObject>
#include <QTimer>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace skillwheel {

/// The Wheel of Destiny: its storage and dialog, and the dialog's game
/// actions (the session's TSkillWheelGameActionHandler asks the server).
class TSkillWheelManager final : public QObject
{
    Q_OBJECT
public:
    explicit TSkillWheelManager(QObject* parent = nullptr);
    ~TSkillWheelManager() override;

    TSkillWheelStorage* storage() const { return m_storage; }
    TSkillWheelDialogController* dialog() const { return m_dialog; }

    /// Host: CipSoft's skill wheel storage belongs to the session (its
    /// character configuration manager) and the dialog's pages follow the
    /// session. Until they do, the client's storage starts over for each
    /// session and the session's handler fills it.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    void clearSkipNextOwnOpen();

    TSkillWheelStorage* m_storage;
    TSkillWheelDialogController* m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP dialog +0x78 / QTimer +0x68 (: single shot, 1000 ms).
    bool m_skipNextOwnOpen = false;
    QTimer m_skipNextOwnOpenTimer;

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
