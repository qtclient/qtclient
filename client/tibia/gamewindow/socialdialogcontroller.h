#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/friendsaccountsearchpagecontroller.h"
#include "gamewindow/friendsbadgespagecontroller.h"
#include "gamewindow/friendsconfigurationpagecontroller.h"
#include "gamewindow/friendsfriendlistpagecontroller.h"
#include "gamewindow/friendsinvitespagecontroller.h"
#include "gamewindow/teamassemblepagecontroller.h"
#include "gamewindow/teamjoinpagecontroller.h"

#include <cip_gen.h>

#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
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
    /// CIP: made with the client options, the engine and the dialog parent
    /// item; the button bar makes the social button from it.
    TSocialDialogController(config::TClientOptions* ClientOptions, QQmlEngine* engine, QQuickItem* dialogParentItem,
                            QObject* parent = nullptr);
    ~TSocialDialogController() override;

    /// CIP connector: the dialog closes, its pages follow `GameSession`, and
    /// the session's friend system errors show here.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The account's premium state: hasPremium follows it, and both team
    /// pages get it.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);

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
    void beforeShow() override;

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
    TMessageDialogController* m_errorDialog = nullptr;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    /// CIP +0x40: the current session; +0x60: its connections.
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;

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
