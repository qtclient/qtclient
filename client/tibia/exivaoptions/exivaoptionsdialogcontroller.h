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

/// The exiva options dialog, ExivaOptionsDialog.qml, opened by
/// OPEN_EXIVA_OPTIONS_DIALOG. It shows the TExivaOptionsStorage's flags and whitelists
/// (one name per line); Apply sends the edited options .
class TExivaOptionsDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TExivaOptionsDialogController>
{
    Q_OBJECT
public:
    explicit TExivaOptionsDialogController(QObject* parent = nullptr);
    ~TExivaOptionsDialogController() override;

    /// The storage the dialog edits and the session's world type.
    void setStorage(TExivaOptionsStorage* storage, std::function<int()> worldType);

    QString characterWhitelist() const override;
    QString guildWhitelist() const override;

public Q_SLOTS:
    /// Outside an Optional PvP world a message box instead.
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void applyButtonClicked(const QString& CharacterWhitelistString, const QString& GuildWhitelistString) override;
    void onAllowEveryoneChanged() override;
    void onAllowedGroupsChanged() override;

private:
    bool handleAction(const input::IGameAction& action);

    QPointer<TExivaOptionsStorage> m_storage;
    std::function<int()> m_worldType;
    gamewindow::TMessageDialogController m_onlyOptionalPvp;
};

}
}
