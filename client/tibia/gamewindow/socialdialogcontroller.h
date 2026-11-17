#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/friendsaccountsearchpagecontroller.h"
#include "gamewindow/friendsbadgespagecontroller.h"
#include "gamewindow/friendsconfigurationpagecontroller.h"
#include "gamewindow/friendsfriendlistpagecontroller.h"
#include "gamewindow/friendsinvitespagecontroller.h"
#include "gamewindow/socialbuttoncontroller.h"
#include "gamewindow/teamassemblepagecontroller.h"
#include "gamewindow/teamjoinpagecontroller.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace input {
class IGameAction;
}
namespace gamewindow {

class TMessageDialogController;

/// SocialDialog.qml (CIP TSocialDialogController, show / close /
/// handleGameAction / requestTabSwitch).
class TSocialDialogController final : public TDialogController<cip::tibia::gamewindow::TSocialDialogController>
{
    Q_OBJECT
public:
    explicit TSocialDialogController(QObject* parent = nullptr);
    ~TSocialDialogController() override;

    /// Stock qtclient install: dialog + button bar controller under `parent`.
    /// Pack (IdleOT) owns the live social surfaces when mods are on.
    static TSocialDialogController* ensureInstalled(QObject* parent);

    TSocialButtonController* socialButton() const { return m_button; }
    TFriendsFriendListPageController* friendListPage() const { return m_friends; }
    TFriendsInvitesPageController* invitesPage() const { return m_invites; }
    TFriendsAccountSearchPageController* accountSearchPage() const { return m_search; }
    TFriendsBadgesPageController* badgesPage() const { return m_badges; }
    TFriendsConfigurationPageController* configurationPage() const { return m_config; }
    TTeamJoinPageController* joinTeamPage() const { return m_join; }
    TTeamAssemblePageController* assembleTeamPage() const { return m_assemble; }

    EDialogTab dialogTab() const override;
    QObject* friendListController() const override;
    QObject* invitesController() const override;
    QObject* accountSearchController() const override;
    QObject* badgesController() const override;
    QObject* configurationController() const override;
    QObject* joinTeamController() const override;
    QObject* assembleTeamController() const override;
    bool backButtonVisible() const override;
    bool refreshButtonVisible() const override;
    bool hasPremium() const override;

public Q_SLOTS:
    void showDialog(EDialogTab Tab) override;
    void closeDialog() override;
    void onShowBackButton(bool Value);

public:
    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void requestTabSwitch(EDialogTab NewTab) override;
    Q_INVOKABLE void signalBackButtonClicked() override;
    Q_INVOKABLE void onRefreshButtonClicked() override;

private:
    bool handleGameAction(const input::IGameAction& action);
    cip::tibia::gamewindow::IDialogTabPage* page(EDialogTab tab) const;
    /// Hides the shown page and shows the tab's page.
    void setTab(EDialogTab tab);
    /// Hides the shown page, leaves every tab and closes the dialog.
    void closeTabs();
    /// The friend system cannot be used: an error message, and the dialog closes.
    void onCannotUseFriendSystemError();
    /// A friend system error with the server's message.
    void onGenericFriendSystemError(const QString& errorMessage);
    void showErrorMessage(const QString& message);

    TFriendsFriendListPageController* m_friends = nullptr;
    TFriendsInvitesPageController* m_invites = nullptr;
    TFriendsAccountSearchPageController* m_search = nullptr;
    TFriendsBadgesPageController* m_badges = nullptr;
    TFriendsConfigurationPageController* m_config = nullptr;
    TTeamJoinPageController* m_join = nullptr;
    TTeamAssemblePageController* m_assemble = nullptr;
    TSocialButtonController* m_button = nullptr;
    TMessageDialogController* m_errorDialog = nullptr;

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
