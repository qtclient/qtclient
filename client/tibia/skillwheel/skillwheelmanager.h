#pragma once

#include "client/gamecore.h"
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

    /// The login session's premium flag (CIP TPremiumTimeStorage::isPremium).
    void setSessionPremium(bool premium) { m_sessionPremium = premium; }

private:
    /// CIP : the own wheel, or the no-wheel / preview message.
    void requestOwnWheel();
    void onServerData(bool canView);
    void saveWheel();
    void clearSkipNextOwnOpen();

    TSkillWheelStorage* m_storage;
    TSkillWheelDialogController* m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP dialog +0x78 / QTimer +0x68 (: single shot, 1000 ms).
    bool m_skipNextOwnOpen = false;
    QTimer m_skipNextOwnOpenTimer;
    bool m_sessionPremium = false;
};

}
}
