#pragma once

#include "gamewindow/dialogmanager.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

#include <functional>

namespace tibia {
namespace exivaoptions {

class TExivaOptionsStorage;

/// The exiva options dialog, ExivaOptionsDialog.qml (CIP:
/// TExivaOptionsDialogController, vtable , OPEN_EXIVA_OPTIONS_DIALOG
/// ). It shows the TExivaOptionsStorage's flags and whitelists
/// (one name per line); Apply sends the edited options .
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
    /// CIP : outside an Optional PvP world a message box instead.
    void showDialog() override;
    void closeDialog() override;
    /// CIP .
    void closeButtonClicked() override;
    void applyButtonClicked(const QString& CharacterWhitelistString, const QString& GuildWhitelistString) override;
    /// CIP .
    void onAllowEveryoneChanged() override;
    /// CIP .
    void onAllowedGroupsChanged() override;

private:
    bool handleAction(const input::TGameAction& action);

    QPointer<TExivaOptionsStorage> m_storage;
    std::function<int()> m_worldType;
    gamewindow::TMessageDialogController m_onlyOptionalPvp;
};

}
}
