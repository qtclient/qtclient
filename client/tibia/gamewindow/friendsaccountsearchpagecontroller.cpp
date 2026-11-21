#include "friendsaccountsearchpagecontroller.h"
#include "game/gamesessionbase.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/characterinfomodels.h"
#include "input/gameaction.h"
#include "protocol/clientmessages.h"

#include <QDateTime>

namespace tibia::gamewindow {

namespace {

// FriendSystemAction numbers.
constexpr uint8_t kSendInvite = 3;
constexpr uint8_t kAddToBlacklist = 7;


/// CIP gamewindow::TAccountCharacterListData: the characters model of one result.
class TAccountCharacterList final : public cip::tibia::gamewindow::TAccountCharacterListData
{
    Q_OBJECT
public:
    TAccountCharacterList(const std::vector<friends::TAccountCharacterListEntry>& characters, QObject* parent)
        : TAccountCharacterListData(parent)
    {
        auto* model = new ::tibia::TAccountCharactersModel(this);
        model->setItems(characters);
        m_accountCharactersModel = model;
    }
};

/// The badges model of one result.
class TAccountBadgeListData final : public cip::tibia::gamewindow::TAccountBadgeListData
{
    Q_OBJECT
public:
    TAccountBadgeListData(const std::vector<friends::TAccountBadge>& badges, QObject* parent)
        : cip::tibia::gamewindow::TAccountBadgeListData(parent)
    {
        std::vector<AccountBadgeEntry> entries;
        entries.reserve(badges.size());
        for (const auto& badge : badges)
            entries.push_back({ badge.id, badge.name });
        m_accountBadgesModel = new TAccountBadgesModel(std::move(entries), this);
    }
};

} // namespace

TFriendsAccountSearchPageController::TFriendsAccountSearchPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsAccountSearchPageController(parent)
    , m_characterInfo(new TCharacterInfoDialogController(this))
{
    m_characterInfoDialogController = m_characterInfo;
    connect(m_characterInfo, &TCharacterInfoDialogController::publishGameAction, this, &TFriendsAccountSearchPageController::publishGameAction);
    m_showEmptyTab = true;
    m_accountCharacterList = new TAccountCharacterList({}, this);
    m_accountBadgeList = new TAccountBadgeListData({}, this);
    connect(m_characterInfo, &TCharacterInfoDialogController::errorLoadingCharacter, this,
            &TFriendsAccountSearchPageController::onErrorLoadingCharacter);
    connect(m_characterInfo, &TCharacterInfoDialogController::noPermissionsToViewCharacter, this,
            &TFriendsAccountSearchPageController::onNoPermissionToDisplayCharacterSummary);
    connect(m_characterInfo, &TCharacterInfoDialogController::noPermissionsToInspectCharacter, this,
            &TFriendsAccountSearchPageController::onNoPermissionToInspectCharacter);
}

bool TFriendsAccountSearchPageController::showAccountInfo() const
{
    return searchResult().showAccountInfo;
}

bool TFriendsAccountSearchPageController::isOnline() const
{
    return searchResult().isOnline;
}

bool TFriendsAccountSearchPageController::isPremium() const
{
    return searchResult().isPremium;
}

bool TFriendsAccountSearchPageController::isFriendOrInvited() const
{
    return searchResult().isFriendOrInvited;
}

bool TFriendsAccountSearchPageController::isOnBlacklist() const
{
    return searchResult().isOnBlacklist;
}

bool TFriendsAccountSearchPageController::hasMainCharacter() const
{
    return searchResult().hasMainCharacter;
}

QString TFriendsAccountSearchPageController::loyaltyTitle() const
{
    return searchResult().loyaltyTitle;
}

QString TFriendsAccountSearchPageController::lastGameLogin() const
{
    return QDateTime::fromSecsSinceEpoch(searchResult().lastGameLogin).toString(QStringLiteral("yyyy-MM-dd"));
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

bool TFriendsAccountSearchPageController::disableButtonsAfterClick() const
{
    return m_disableButtonsAfterClick;
}

bool TFriendsAccountSearchPageController::showCharacterInfo() const
{
    return m_showCharacterInfo;
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

void TFriendsAccountSearchPageController::setShowEmptyTab(const bool show)
{
    if (show == m_showEmptyTab)
        return;
    m_showEmptyTab = show;
    Q_EMIT showEmptyTabChanged();
}

void TFriendsAccountSearchPageController::setShowNoResultsFound(const bool show)
{
    if (show == m_showNoResultsFound)
        return;
    m_showNoResultsFound = show;
    Q_EMIT showNoResultsFoundChanged();
}

void TFriendsAccountSearchPageController::setShowLoadingComponent(const bool show)
{
    if (show == m_showLoadingComponent)
        return;
    m_showLoadingComponent = show;
    Q_EMIT showLoadingComponentChanged();
}

void TFriendsAccountSearchPageController::setDisableButtonsAfterClick(const bool disable)
{
    if (disable == m_disableButtonsAfterClick)
        return;
    m_disableButtonsAfterClick = disable;
    Q_EMIT disableButtonsAfterClickChanged();
}

void TFriendsAccountSearchPageController::setShowCharacterInfo(const bool show)
{
    if (show == m_showCharacterInfo)
        return;
    m_showCharacterInfo = show;
    if (show) {
        m_characterInfo->showDialogTab(TCharacterInfoDialogController::Inspection);
    } else {
        m_characterInfo->hideDialogTab();
    }
    Q_EMIT showBackButton(show);
    Q_EMIT showCharacterInfoChanged();
}

void TFriendsAccountSearchPageController::closeCharacterInfo()
{
    if (!m_showCharacterInfo)
        return;
    m_showCharacterInfo = false;
    m_characterInfo->hideDialogTab();
    Q_EMIT showBackButton(false);
    Q_EMIT showCharacterInfoChanged();
}

void TFriendsAccountSearchPageController::showDialogTab()
{
    // An open tab starts empty.
    setShowEmptyTab(true);
    setShowLoadingComponent(false);
    setShowNoResultsFound(false);
}

void TFriendsAccountSearchPageController::hideDialogTab()
{
    setShowEmptyTab(true);
    setShowLoadingComponent(false);
    setShowNoResultsFound(false);
    closeCharacterInfo();
}

void TFriendsAccountSearchPageController::replaceLists()
{
    const friends::TAccountSearchResult& result = searchResult();
    auto* characters = m_accountCharacterList;
    auto* badges = m_accountBadgeList;
    m_accountCharacterList = new TAccountCharacterList(result.characters, this);
    m_accountBadgeList = new TAccountBadgeListData(result.badges, this);
    // QML may still read the old lists until it sees dataChanged.
    if (characters)
        characters->deleteLater();
    if (badges)
        badges->deleteLater();
}

void TFriendsAccountSearchPageController::onAccountSearchResults()
{
    setShowEmptyTab(false);
    setShowNoResultsFound(false);
    setShowLoadingComponent(false);
    setDisableButtonsAfterClick(false);
    replaceLists();
    Q_EMIT dataChanged();
}

void TFriendsAccountSearchPageController::onNoResultsFound()
{
    setShowEmptyTab(true);
    setShowNoResultsFound(true);
}

void TFriendsAccountSearchPageController::onFriendSystemError()
{
    setShowLoadingComponent(false);
    setDisableButtonsAfterClick(false);
}

void TFriendsAccountSearchPageController::onClickedSearchForCharacter(const QString& SearchString)
{
    setShowLoadingComponent(true);
    protocol::gameclient::sendFriendSystemSearch(SearchString.toStdString());
}

void TFriendsAccountSearchPageController::openCyclopediaCharacterInfo(const int CharacterID)
{
    m_characterInfo->setCharacterId(static_cast<uint32_t>(CharacterID));
    setShowCharacterInfo(true);
}

void TFriendsAccountSearchPageController::refreshCurrentCharacterInfo()
{
    // Asks again for the page that is shown.
    m_characterInfo->showDialogTab();
}

void TFriendsAccountSearchPageController::handleGameAction(const input::IGameAction& action)
{
    const auto* search = input::gameActionAs<input::TGameActionFriendsDialogSearchAccount>(&action);
    if (!search)
        return;
    showDialogTab();
    setShowLoadingComponent(true);
    protocol::gameclient::sendFriendSystemSearch(search->characterName().toStdString());
}

void TFriendsAccountSearchPageController::onClickedInviteAsFriend()
{
    protocol::gameclient::sendFriendSystemAccountAction(kSendInvite, searchResult().accountId);
    setDisableButtonsAfterClick(true);
}

void TFriendsAccountSearchPageController::onClickedSetOnBlacklist()
{
    protocol::gameclient::sendFriendSystemAccountAction(kAddToBlacklist, searchResult().accountId);
    setDisableButtonsAfterClick(true);
}

void TFriendsAccountSearchPageController::onBackButtonPressed()
{
    closeCharacterInfo();
}

void TFriendsAccountSearchPageController::onErrorLoadingCharacter()
{
    closeCharacterInfo();
}

void TFriendsAccountSearchPageController::onNoPermissionToDisplayCharacterSummary()
{
    closeCharacterInfo();
}

void TFriendsAccountSearchPageController::onNoPermissionToInspectCharacter()
{
    // Without the inspection the general stats are shown.
    m_characterInfo->showDialogTab(TCharacterInfoDialogController::GeneralStats);
}


void TFriendsAccountSearchPageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    friends::TFriendsAccountSearchStorage* search = session->friendsAccountSearchStorage().get();
    m_gameSessionConnections.push_back(connect(search, &friends::TFriendsAccountSearchStorage::receivedAccountSearchResults, this,
                                               &TFriendsAccountSearchPageController::onAccountSearchResults));
    m_gameSessionConnections.push_back(connect(search, &friends::TFriendsAccountSearchStorage::receivedNoResultsFound, this,
                                               &TFriendsAccountSearchPageController::onNoResultsFound));
    m_gameSessionConnections.push_back(connect(session->friendsProtocolMessageHandler().get(),
                                               &friends::TFriendsProtocolMessageHandler::errorReceived, this,
                                               &TFriendsAccountSearchPageController::onFriendSystemError));
}

const friends::TAccountSearchResult& TFriendsAccountSearchPageController::searchResult() const
{
    static const friends::TAccountSearchResult none;
    friends::TFriendsStorage* storage = friendsStorage();
    return storage ? storage->accountSearch()->result() : none;
}

friends::TFriendsStorage* TFriendsAccountSearchPageController::friendsStorage() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->friendsStorage().get() : nullptr;
}

}

#include "friendsaccountsearchpagecontroller.moc"
