#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "skillwheel/skillwheelciptypes.h"
#include "skillwheel/skillwheelcontrollers.h"

#include <QObject>
#include <QTimer>

#include <vector>

namespace tibia {
namespace skillwheel {

/// The Wheel of Destiny: its storage and dialog, and the game actions 306..313
/// (CIP: TSkillWheelGameActionHandler and the dialog's handler
/// ).
class TSkillWheelManager final : public QObject
{
    Q_OBJECT
public:
    explicit TSkillWheelManager(QObject* parent = nullptr);
    ~TSkillWheelManager() override;

    TSkillWheelStorage* storage() const { return m_storage; }
    TSkillWheelDialogController* dialog() const { return m_dialog; }
    TSkillWheelGameActionHandler* actions() { return &m_actions; }

    /// The login session's premium flag (CIP TPremiumTimeStorage::isPremium).
    void setSessionPremium(bool premium) { m_actions.setSessionPremium(premium); }

private:
    void clearSkipNextOwnOpen();

    TSkillWheelGameActionHandler m_actions;
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
