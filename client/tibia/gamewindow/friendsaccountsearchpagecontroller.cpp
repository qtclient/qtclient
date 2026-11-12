#include "friendsaccountsearchpagecontroller.h"

#include "gamewindow/characterinfodialogcontroller.h"

namespace tibia::gamewindow {

namespace {

class TAccountCharacterList final : public cip::tibia::gamewindow::TAccountCharacterListData
{
    Q_OBJECT
public:
    explicit TAccountCharacterList(QObject* parent)
        : TAccountCharacterListData(parent)
    {
        m_accountCharactersModel = new cip::tibia::gamewindow::TAccountCharactersModel(this);
    }
};

class TAccountBadgeList final : public cip::tibia::gamewindow::TAccountBadgeListData
{
    Q_OBJECT
public:
    explicit TAccountBadgeList(QObject* parent)
        : TAccountBadgeListData(parent)
    {
        m_accountBadgesModel = new cip::tibia::gamewindow::TAccountBadgesModel(this);
    }
};

} // namespace

TFriendsAccountSearchPageController::TFriendsAccountSearchPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsAccountSearchPageController(parent)
    , m_characters(new TAccountCharacterList(this))
    , m_badges(new TAccountBadgeList(this))
    , m_characterInfo(new TCharacterInfoDialogController(this))
{
    m_accountCharacterList = m_characters;
    m_accountBadgeList = m_badges;
    m_characterInfoDialogController = m_characterInfo;
    m_showEmptyTab = true;
}

bool TFriendsAccountSearchPageController::showAccountInfo() const
{
    return m_showAccountInfo;
}

bool TFriendsAccountSearchPageController::isOnline() const
{
    return m_isOnline;
}

bool TFriendsAccountSearchPageController::isPremium() const
{
    return m_isPremium;
}

bool TFriendsAccountSearchPageController::isFriendOrInvited() const
{
    return m_isFriendOrInvited;
}

bool TFriendsAccountSearchPageController::isOnBlacklist() const
{
    return m_isOnBlacklist;
}

bool TFriendsAccountSearchPageController::showEmptyTab() const
{
    return m_showEmptyTab;
}

bool TFriendsAccountSearchPageController::showNoResultsFound() const
{
    return m_showNoResultsFound;
}

bool TFriendsAccountSearchPageController::showLoadingComponent() const
{
    return m_showLoadingComponent;
}

bool TFriendsAccountSearchPageController::hasMainCharacter() const
{
    return m_hasMainCharacter;
}

bool TFriendsAccountSearchPageController::disableButtonsAfterClick() const
{
    return m_disableButtonsAfterClick;
}

bool TFriendsAccountSearchPageController::showCharacterInfo() const
{
    return m_showCharacterInfo;
}

void TFriendsAccountSearchPageController::setShowCharacterInfo(const bool show)
{
    cip::tibia::gamewindow::TFriendsAccountSearchPageController::setShowCharacterInfo(show);
}

QString TFriendsAccountSearchPageController::loyaltyTitle() const
{
    return m_loyaltyTitle;
}

QString TFriendsAccountSearchPageController::lastGameLogin() const
{
    return m_lastGameLogin;
}

cip::tibia::gamewindow::TAccountBadgeListData* TFriendsAccountSearchPageController::accountBadgeList() const
{
    return m_accountBadgeList;
}

cip::tibia::gamewindow::TAccountCharacterListData* TFriendsAccountSearchPageController::accountCharacterList() const
{
    return m_accountCharacterList;
}

QObject* TFriendsAccountSearchPageController::characterInfoDialogController() const
{
    return m_characterInfoDialogController;
}

void TFriendsAccountSearchPageController::showDialogTab()
{
}

void TFriendsAccountSearchPageController::hideDialogTab()
{
    onBackButtonPressed();
}

void TFriendsAccountSearchPageController::setLoading(const bool loading)
{
    if (loading == m_showLoadingComponent)
        return;
    m_showLoadingComponent = loading;
    Q_EMIT showLoadingComponentChanged();
}

void TFriendsAccountSearchPageController::setEmpty(const bool empty, const bool noResults)
{
    if (empty != m_showEmptyTab) {
        m_showEmptyTab = empty;
        Q_EMIT showEmptyTabChanged();
    }
    if (noResults != m_showNoResultsFound) {
        m_showNoResultsFound = noResults;
        Q_EMIT showNoResultsFoundChanged();
    }
}

void TFriendsAccountSearchPageController::onClickedSearchForCharacter(const QString& SearchString)
{
    // CIP : loading on, then the storage's search request.
    if (SearchString.trimmed().isEmpty())
        return;
    setEmpty(false, false);
    setLoading(true);
    if (m_disableButtonsAfterClick) {
        m_disableButtonsAfterClick = false;
        Q_EMIT disableButtonsAfterClickChanged();
    }
    // Tip stock: no friend-system wire yet — surface the empty/no-results path.
    setLoading(false);
    setEmpty(false, true);
}

void TFriendsAccountSearchPageController::openCyclopediaCharacterInfo(const int CharacterID)
{
    // CIP .
    m_characterInfo->setCharacterId(static_cast<uint32_t>(CharacterID));
    if (m_showCharacterInfo)
        return;
    m_showCharacterInfo = true;
    m_characterInfo->setNextDataType(TCharacterInfoDialogController::Inspection);
    m_characterInfo->showDialogTab();
    Q_EMIT showBackButton(true);
    Q_EMIT showCharacterInfoChanged();
}

void TFriendsAccountSearchPageController::refreshCurrentCharacterInfo()
{
    if (m_characterInfo)
        m_characterInfo->showDialogTab();
}

void TFriendsAccountSearchPageController::onClickedInviteAsFriend()
{
    // CIP .
    if (m_disableButtonsAfterClick)
        return;
    m_disableButtonsAfterClick = true;
    Q_EMIT disableButtonsAfterClickChanged();
}

void TFriendsAccountSearchPageController::onClickedSetOnBlacklist()
{
}

void TFriendsAccountSearchPageController::onBackButtonPressed()
{
    // CIP .
    if (!m_showCharacterInfo)
        return;
    m_showCharacterInfo = false;
    m_characterInfo->hideDialogTab();
    Q_EMIT showBackButton(false);
    Q_EMIT showCharacterInfoChanged();
}

void TFriendsAccountSearchPageController::handleGameAction()
{
    // CIP moc handleGameAction(shared_ptr) — qtclient action path is the dialog controller.
}

void TFriendsAccountSearchPageController::onErrorLoadingCharacter()
{
    setLoading(false);
    setEmpty(false, true);
}

void TFriendsAccountSearchPageController::onNoPermissionToDisplayCharacterSummary()
{
}

void TFriendsAccountSearchPageController::onNoPermissionToInspectCharacter()
{
}

}

#include "friendsaccountsearchpagecontroller.moc"
