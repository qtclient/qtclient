#include "friendsaccountsearchpagecontroller.h"

#include "gamewindow/characterinfodialogcontroller.h"
#include "friends/friendsciptypes.h"
#include "protocol/clientmessages.h"
#include "client/outfit.h"

#include <QDateTime>
#include <QLocale>

namespace tibia::gamewindow {

namespace {

// CIP FriendSystemAction numbers.
constexpr uint8_t kSendInvite = 3;
constexpr uint8_t kAddToBlacklist = 7;

QColor outfitColor(const uint32_t index)
{
    const Color color = Outfit::getColor(static_cast<int>(index));
    return QColor(color.r(), color.g(), color.b());
}

class TAccountCharacterList final : public cip::tibia::gamewindow::TAccountCharacterListData
{
    Q_OBJECT
public:
    TAccountCharacterList(QObject* model, QObject* parent)
        : TAccountCharacterListData(parent)
    {
        m_accountCharactersModel = model;
    }
};

class TAccountBadgeList final : public cip::tibia::gamewindow::TAccountBadgeListData
{
    Q_OBJECT
public:
    TAccountBadgeList(QObject* model, QObject* parent)
        : TAccountBadgeListData(parent)
    {
        m_accountBadgesModel = model;
    }
};

} // namespace

TFriendsAccountSearchPageController::TFriendsAccountSearchPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsAccountSearchPageController(parent)
    , m_characterInfo(new TCharacterInfoDialogController(this))
    , m_characterRows(new TFriendsRowsModel({ "characterID", "characterName", "characterTitle", "isMainCharacter", "isOnline", "level",
                                              "vocation", "world", "outfitID", "headColor", "torsoColor", "legsColor", "detailColor",
                                              "addon" },
                                            this))
    , m_badgeRows(new TFriendsRowsModel({ "badgeIcon", "badgeName" }, this))
{
    m_characters = new TAccountCharacterList(m_characterRows, this);
    m_badges = new TAccountBadgeList(m_badgeRows, this);
    m_accountCharacterList = m_characters;
    m_accountBadgeList = m_badges;
    m_characterInfoDialogController = m_characterInfo;
    m_showEmptyTab = true;
    auto* search = friends::TFriendsStorage::instance()->accountSearch();
    connect(search, &friends::TFriendsAccountSearchStorage::receivedAccountSearchResults, this,
            &TFriendsAccountSearchPageController::onSearchResults);
    connect(search, &friends::TFriendsAccountSearchStorage::receivedNoResultsFound, this,
            &TFriendsAccountSearchPageController::onNoResultsFound);
}

void TFriendsAccountSearchPageController::onSearchResults()
{
    const auto& result = friends::TFriendsStorage::instance()->accountSearch()->result();
    m_accountId = result.account_id();
    m_showAccountInfo = result.show_account_info();
    m_isOnline = result.online();
    m_isPremium = result.premium();
    m_isFriendOrInvited = result.friend_or_invited();
    m_isOnBlacklist = result.on_blacklist();
    m_loyaltyTitle = QString::fromStdString(result.loyalty_title());
    m_lastGameLogin = result.last_login() == 0
        ? QString()
        : QLocale().toString(QDateTime::fromSecsSinceEpoch(result.last_login()), QLocale::ShortFormat);
    m_hasMainCharacter = false;
    QVector<QVariantMap> characters;
    for (const auto& character : result.characters()) {
        m_hasMainCharacter = m_hasMainCharacter || character.main_character();
        const auto& outfit = character.outfit();
        characters.push_back({
            { QStringLiteral("characterID"), static_cast<uint>(character.character_id()) },
            { QStringLiteral("characterName"), QString::fromStdString(character.name()) },
            { QStringLiteral("characterTitle"), QString::fromStdString(character.title()) },
            { QStringLiteral("isMainCharacter"), character.main_character() },
            { QStringLiteral("isOnline"), character.online() },
            { QStringLiteral("level"), static_cast<int>(character.level()) },
            { QStringLiteral("vocation"), QString::fromStdString(character.vocation()) },
            { QStringLiteral("world"), QString::fromStdString(character.world()) },
            { QStringLiteral("outfitID"), static_cast<int>(outfit.look_type()) },
            { QStringLiteral("headColor"), outfitColor(outfit.head()) },
            { QStringLiteral("torsoColor"), outfitColor(outfit.body()) },
            { QStringLiteral("legsColor"), outfitColor(outfit.legs()) },
            { QStringLiteral("detailColor"), outfitColor(outfit.feet()) },
            { QStringLiteral("addon"), static_cast<int>(outfit.addons()) },
        });
    }
    m_characterRows->setRows(std::move(characters));
    QVector<QVariantMap> badges;
    for (const auto& badge : result.badges())
        badges.push_back({ { QStringLiteral("badgeIcon"), QString::fromStdString(badge.icon()) },
                           { QStringLiteral("badgeName"), QString::fromStdString(badge.name()) } });
    m_badgeRows->setRows(std::move(badges));
    m_disableButtonsAfterClick = false;
    Q_EMIT disableButtonsAfterClickChanged();
    Q_EMIT dataChanged();
    setLoading(false);
    setEmpty(false, false);
}

void TFriendsAccountSearchPageController::onNoResultsFound()
{
    m_accountId = 0;
    setLoading(false);
    setEmpty(false, true);
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
    protocol::gameclient::sendFriendSystemSearch(SearchString.trimmed().toStdString());
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
    if (m_disableButtonsAfterClick || m_accountId == 0)
        return;
    protocol::gameclient::sendFriendSystemAccountAction(kSendInvite, m_accountId);
    m_disableButtonsAfterClick = true;
    Q_EMIT disableButtonsAfterClickChanged();
}

void TFriendsAccountSearchPageController::onClickedSetOnBlacklist()
{
    if (m_disableButtonsAfterClick || m_accountId == 0)
        return;
    protocol::gameclient::sendFriendSystemAccountAction(kAddToBlacklist, m_accountId);
    m_disableButtonsAfterClick = true;
    Q_EMIT disableButtonsAfterClickChanged();
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
