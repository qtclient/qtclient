#pragma once

#include "client/gamecore.h"
#include "skillwheel/skillwheelcontrollers.h"

#include <QObject>
#include <QTimer>

#include <vector>

namespace tibia::skillwheel {

/// The Wheel of Destiny: its storage and dialog, and the game actions 306..313
/// (CIP: TSkillWheelGameActionHandler 0x100f9fb80 and the dialog's handler
/// 0x100ddb940).
class TSkillWheelManager final : public QObject
{
    Q_OBJECT
public:
    explicit TSkillWheelManager(QObject* parent = nullptr);
    ~TSkillWheelManager() override;

    TSkillWheelStorage* storage() const { return m_storage; }
    TSkillWheelDialogController* dialog() const { return m_dialog; }

    /// The login session's premium flag (CIP TPremiumTimeStorage::isPremium 0x100b47bf0).
    void setSessionPremium(bool premium) { m_sessionPremium = premium; }

private:
    /// CIP 0x100f9fe10: the own wheel, or the no-wheel / preview message.
    void requestOwnWheel();
    void onServerData(bool canView);
    void saveWheel();
    void clearSkipNextOwnOpen();

    TSkillWheelStorage* m_storage;
    TSkillWheelDialogController* m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP dialog +0x78 / QTimer +0x68 (0x100dda3b0: single shot, 1000 ms).
    bool m_skipNextOwnOpen = false;
    QTimer m_skipNextOwnOpenTimer;
    bool m_sessionPremium = false;
};

}
