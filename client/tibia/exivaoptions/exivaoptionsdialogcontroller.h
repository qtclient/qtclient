#pragma once

#include "gamewindow/dialogmanager.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

#include <functional>

namespace tibia::exivaoptions {

class TExivaOptionsStorage;

/// The exiva options dialog, ExivaOptionsDialog.qml (CIP:
/// TExivaOptionsDialogController, vtable 0x102631d70, OPEN_EXIVA_OPTIONS_DIALOG
/// 0x100d2e260). It shows the TExivaOptionsStorage's flags and whitelists
/// (one name per line); Apply sends the edited options (0x100d2e490).
class TExivaOptionsDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TExivaOptionsDialogController>
{
    Q_OBJECT
public:
    explicit TExivaOptionsDialogController(QObject* parent = nullptr);
    ~TExivaOptionsDialogController() override;

    /// The storage the dialog edits (CIP +0xd8) and the world type (+0xc8 vslot 0x60).
    void setStorage(TExivaOptionsStorage* storage, std::function<int()> worldType);

    QString characterWhitelist() const override;
    QString guildWhitelist() const override;

public Q_SLOTS:
    /// CIP 0x100d2e2c0: outside an Optional PvP world a message box instead.
    void showDialog() override;
    void closeDialog() override;
    /// CIP 0x100d2e480.
    void closeButtonClicked() override;
    void applyButtonClicked(const QString& CharacterWhitelistString, const QString& GuildWhitelistString) override;
    /// CIP 0x100d2dd10.
    void onAllowEveryoneChanged() override;
    /// CIP 0x100d2dd50.
    void onAllowedGroupsChanged() override;

private:
    bool handleAction(const input::TGameAction& action);

    QPointer<TExivaOptionsStorage> m_storage;
    std::function<int()> m_worldType;
    gamewindow::TMessageDialogController m_onlyOptionalPvp;
};

}
