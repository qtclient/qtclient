#include "socialdialogcontroller.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {
TSocialDialogController* g_socialDialog = nullptr;
}

TSocialDialogController* TSocialDialogController::ensureInstalled(QObject* parent)
{
    if (g_socialDialog)
        return g_socialDialog;
    g_socialDialog = new TSocialDialogController(parent);
    return g_socialDialog;
}

TSocialDialogController::TSocialDialogController(QObject* parent)
    : TDialogController(QStringLiteral("SocialDialog.qml"), parent)
    , m_friends(new TFriendsFriendListPageController(this))
    , m_invites(new TFriendsInvitesPageController(this))
    , m_search(new TFriendsAccountSearchPageController(this))
    , m_badges(new TFriendsBadgesPageController(this))
    , m_config(new TFriendsConfigurationPageController(this))
    , m_join(new TTeamJoinPageController(this))
    , m_assemble(new TTeamAssemblePageController(this))
    , m_button(new TSocialButtonController(this, this))
    , m_errorDialog(new TMessageDialogController(this))
{
    m_friendListController = m_friends;
    m_invitesController = m_invites;
    m_accountSearchController = m_search;
    m_badgesController = m_badges;
    m_configurationController = m_config;
    m_joinTeamController = m_join;
    m_assembleTeamController = m_assemble;
    connect(m_search, &TFriendsAccountSearchPageController::showBackButton, this, &TSocialDialogController::onShowBackButton);
    connect(this, &TSocialDialogController::backButtonClicked, m_search, &TFriendsAccountSearchPageController::onBackButtonPressed);
    m_errorDialog->setCloseOnButtonClicked(true);
    auto* friendsHandler = friends::TFriendsProtocolMessageHandler::instance();
    connect(friendsHandler, &friends::TFriendsProtocolMessageHandler::cannotUseFriendSystemError, this,
            &TSocialDialogController::onCannotUseFriendSystemError);
    connect(friendsHandler, &friends::TFriendsProtocolMessageHandler::genericFriendSystemError, this,
            &TSocialDialogController::onGenericFriendSystemError);
    for (const auto type : { input::EGameAction::OPEN_SOCIAL_DIALOG, input::EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT })
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleGameAction(action); });
    if (!g_socialDialog)
        g_socialDialog = this;
}

TSocialDialogController::~TSocialDialogController()
{
    input::setActionHandler(input::EGameAction::OPEN_SOCIAL_DIALOG, nullptr);
    input::setActionHandler(input::EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT, nullptr);
    if (g_socialDialog == this)
        g_socialDialog = nullptr;
}

TSocialDialogController::EDialogTab TSocialDialogController::dialogTab() const
{
    return m_dialogTab;
}

QObject* TSocialDialogController::friendListController() const
{
    return m_friendListController;
}

QObject* TSocialDialogController::invitesController() const
{
    return m_invitesController;
}

QObject* TSocialDialogController::accountSearchController() const
{
    return m_accountSearchController;
}

QObject* TSocialDialogController::badgesController() const
{
    return m_badgesController;
}

QObject* TSocialDialogController::configurationController() const
{
    return m_configurationController;
}

QObject* TSocialDialogController::joinTeamController() const
{
    return m_joinTeamController;
}

QObject* TSocialDialogController::assembleTeamController() const
{
    return m_assembleTeamController;
}

bool TSocialDialogController::backButtonVisible() const
{
    return m_backButtonVisible;
}

bool TSocialDialogController::refreshButtonVisible() const
{
    // Only the team list can be refreshed.
    return m_dialogTab == JoinTeam;
}

/// The local player's premium state, false without a player.
bool TSocialDialogController::hasPremium() const
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
}

cip::tibia::gamewindow::IDialogTabPage* TSocialDialogController::page(const EDialogTab tab) const
{
    switch (tab) {
        case FriendList: return m_friends;
        case Invites: return m_invites;
        case AccountSearch: return m_search;
        case Badges: return m_badges;
        case FriendsConfiguration: return m_config;
        case JoinTeam: return m_join;
        case AssembleTeam: return m_assemble;
        default: return nullptr;
    }
}

void TSocialDialogController::setTab(const EDialogTab tab)
{
    // The shown page is hidden and the new one shown (shown again when it is
    // the same); a tab without a page shows nothing.
    if (auto* old = page(m_dialogTab))
        old->hideDialogTab();
    if (auto* current = page(tab)) {
        current->showDialogTab();
        m_dialogTab = tab;
    } else {
        m_dialogTab = NoTabInitialState;
    }
    Q_EMIT dialogTabChanged();
}

void TSocialDialogController::closeTabs()
{
    if (auto* current = page(m_dialogTab))
        current->hideDialogTab();
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
    dialog()->close();
}

void TSocialDialogController::showErrorMessage(const QString& message)
{
    m_errorDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                         QKeySequence(QStringLiteral("Return"))) });
    m_errorDialog->showMessage(qtTrId("error"), message);
}

void TSocialDialogController::onCannotUseFriendSystemError()
{
    showErrorMessage(qtTrId("friends_function_not_available"));
    closeTabs();
}

void TSocialDialogController::onGenericFriendSystemError(const QString& errorMessage)
{
    showErrorMessage(errorMessage);
}

bool TSocialDialogController::handleGameAction(const input::TGameAction& action)
{
    if (action.type == input::EGameAction::OPEN_SOCIAL_DIALOG) {
        // Parameter p opens tab p + 1; anything else the friend list.
        const int parameter = action.parameter;
        showDialog(parameter >= 0 && parameter <= 6 ? static_cast<EDialogTab>(parameter + 1) : FriendList);
    } else if (action.type == input::EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT) {
        setTab(AccountSearch);
    }
    // The account search page runs its own actions (a new search).
    m_search->handleGameAction(action);
    return true;
}

void TSocialDialogController::showDialog(const EDialogTab Tab)
{
    // The dialog opens anew on the tab.
    closeTabs();
    dialog()->show();
    setTab(Tab);
}

void TSocialDialogController::closeDialog()
{
    requestClose();
}

void TSocialDialogController::requestClose()
{
    closeTabs();
}

void TSocialDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    setTab(NewTab);
    Q_EMIT buttonVisibleChanged();
}

void TSocialDialogController::signalBackButtonClicked()
{
    Q_EMIT backButtonClicked();
}

void TSocialDialogController::onRefreshButtonClicked()
{
    if (auto* current = page(m_dialogTab))
        current->showDialogTab();
}

void TSocialDialogController::onShowBackButton(const bool Value)
{
    if (Value == m_backButtonVisible)
        return;
    m_backButtonVisible = Value;
    Q_EMIT buttonVisibleChanged();
}

}
