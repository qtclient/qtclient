#pragma once

#include "client/gamecore.h"
#include "skillwheel/skillwheelciptypes.h"
#include "skillwheel/skillwheelcontrollers.h"

#include <QObject>

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
    TSkillWheelStorage* m_storage;
    TSkillWheelDialogController* m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;

public Q_SLOTS:
    /// The router's game actions go to the dialog (CIP: TSkillWheelDialogController::handleGameAction,
    /// connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction) { m_dialog->handleGameAction(pGameAction); }
};

}
}
