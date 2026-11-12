#include "socialdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

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
{
    m_friendListController = m_friends;
    m_invitesController = m_invites;
    m_accountSearchController = m_search;
    m_badgesController = m_badges;
    m_configurationController = m_config;
    m_joinTeamController = m_join;
    m_assembleTeamController = m_assemble;
    m_hasPremium = true;
    connect(m_search, &TFriendsAccountSearchPageController::showBackButton, this, &TSocialDialogController::onShowBackButton);
    connect(this, &TSocialDialogController::backButtonClicked, m_search, &TFriendsAccountSearchPageController::onBackButtonPressed);
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
    return m_refreshButtonVisible;
}

bool TSocialDialogController::hasPremium() const
{
    return m_hasPremium;
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
    if (tab == m_dialogTab)
        return;
    if (auto* old = page(m_dialogTab))
        old->hideDialogTab();
    m_dialogTab = tab;
    m_refreshButtonVisible = tab == FriendList || tab == Invites || tab == JoinTeam || tab == AssembleTeam;
    Q_EMIT dialogTabChanged();
    if (auto* current = page(tab))
        current->showDialogTab();
}

bool TSocialDialogController::handleGameAction(const input::TGameAction& action)
{
    // CIP : OPEN_SOCIAL_DIALOG parameter p opens tab p + 1.
    // FRIENDS_DIALOG_SEARCH_ACCOUNT searches `text`.
    if (action.type == input::EGameAction::FRIENDS_DIALOG_SEARCH_ACCOUNT) {
        showDialog(AccountSearch);
        if (!action.text.isEmpty())
            m_search->onClickedSearchForCharacter(action.text);
        return true;
    }
    const auto tab = static_cast<EDialogTab>(action.parameter + 1);
    showDialog(page(tab) ? tab : FriendList);
    return true;
}

void TSocialDialogController::showDialog(const EDialogTab Tab)
{
    // CIP .
    if (!dialog()->isInStack())
        dialog()->show();
    setTab(page(Tab) ? Tab : FriendList);
}

void TSocialDialogController::closeDialog()
{
    requestClose();
}

void TSocialDialogController::requestClose()
{
    // CIP .
    setTab(NoTabInitialState);
    dialog()->close();
}

void TSocialDialogController::requestTabSwitch(const EDialogTab NewTab)
{
    // CIP . Tabs without a page stay put.
    if (page(NewTab))
        setTab(NewTab);
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
