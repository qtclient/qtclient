#include "friendsfriendlistpagecontroller.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "protocol/clientmessages.h"

#include "client/outfit.h"

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kChangeFriendGroup = 10;
constexpr uint8_t kRemoveFriend = 9;
constexpr uint8_t kLoadFriendList = 0;

QColor outfitColor(const uint32_t index)
{
    const Color color = Outfit::getColor(static_cast<int>(index));
    return QColor(color.r(), color.g(), color.b());
}

bool shownBy(const TFriendsFriendListPageController::EShowFilter filter, const int group)
{
    using EShowFilter = TFriendsFriendListPageController::EShowFilter;
    switch (filter) {
    case EShowFilter::ShowCloseFriends:
        return group == 3;
    case EShowFilter::ShowFriends:
        return group == 2;
    case EShowFilter::ShowContacts:
        return group == 1;
    default:
        return true;
    }
}

}

TFriendsFriendListPageController::TFriendsFriendListPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsFriendListPageController(parent)
    , m_model(new TFriendListModel(this))
    , m_confirmDialog(new TMessageDialogController(this))
{
    m_friendList = m_model;
    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, &TFriendsFriendListPageController::onConfirmDialogAccepted);
    connect(friends::TFriendsStorage::instance()->friendList(), &friends::TFriendsFriendListStorage::friendListChanged, this,
            &TFriendsFriendListPageController::rebuildRows);
    rebuildRows();
}

QObject* TFriendsFriendListPageController::friendList() const
{
    return m_friendList;
}

TFriendsFriendListPageController::EShowFilter TFriendsFriendListPageController::showFilter() const
{
    return m_showFilter;
}

void TFriendsFriendListPageController::setShowFilter(const EShowFilter ShowFilter)
{
    if (ShowFilter == m_showFilter)
        return;
    cip::tibia::gamewindow::TFriendsFriendListPageController::setShowFilter(ShowFilter);
    rebuildRows();
}

void TFriendsFriendListPageController::rebuildRows()
{
    QVector<TFriendListModel::Row> rows;
    for (const auto& entry : friends::TFriendsStorage::instance()->friendList()->friends()) {
        const int group = static_cast<int>(entry.friend_group());
        if (!shownBy(m_showFilter, group))
            continue;
        const auto& account = entry.account();
        const auto& outfit = account.outfit();
        TFriendListModel::Row row;
        row.accountId = static_cast<int>(account.account_id());
        row.maincharName = QString::fromStdString(account.name());
        row.title = QString::fromStdString(account.title());
        row.level = static_cast<int>(account.level());
        row.vocation = QString::fromStdString(account.vocation());
        row.world = QString::fromStdString(account.world());
        row.friendGroup = group;
        row.outfitId = static_cast<int>(outfit.look_type());
        row.outfitHeadColor = outfitColor(outfit.head());
        row.outfitTorsoColor = outfitColor(outfit.body());
        row.outfitLegsColor = outfitColor(outfit.legs());
        row.outfitDetailColor = outfitColor(outfit.feet());
        row.outfitFirstAddOn = (outfit.addons() & 1) != 0;
        row.outfitSecondAddOn = (outfit.addons() & 2) != 0;
        row.length = static_cast<int>(entry.friendship_length());
        rows.push_back(std::move(row));
    }
    m_model->setRows(std::move(rows));
}

void TFriendsFriendListPageController::showDialogTab()
{
    // The list is fetched each time the tab opens.
    protocol::gameclient::sendFriendSystemAction(kLoadFriendList);
}

void TFriendsFriendListPageController::hideDialogTab()
{
}

void TFriendsFriendListPageController::showAccountDetails(const int AccountIDRaw)
{
    for (const auto& entry : friends::TFriendsStorage::instance()->friendList()->friends()) {
        if (static_cast<int>(entry.account().account_id()) == AccountIDRaw) {
            Q_EMIT accountDetailsRequested(QString::fromStdString(entry.account().name()));
            return;
        }
    }
}

void TFriendsFriendListPageController::showContextMenuForAccountID(const int AccountIDRaw)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const auto accountId = static_cast<uint32_t>(AccountIDRaw);
    menu->beginCustomMenu();
    const auto group = [this, menu, accountId](const char* label, const uint8_t friendGroup) {
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [accountId, friendGroup] {
            protocol::gameclient::sendFriendSystemAccountAction(kChangeFriendGroup, accountId, friendGroup);
        });
    };
    group("friends_changefriendgroup_contacts", 1);
    group("friends_changefriendgroup_friends", 2);
    group("friends_changefriendgroup_close_friends", 3);
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("friends_unfriend")), &TContextMenuEntryItem::triggered, this, [this, accountId] {
        QString name;
        for (const auto& entry : friends::TFriendsStorage::instance()->friendList()->friends()) {
            if (entry.account().account_id() == accountId)
                name = QString::fromStdString(entry.account().name());
        }
        m_pendingUnfriend = static_cast<int>(accountId);
        m_confirmDialog->showMessage(qtTrId("friends_unfriend"), qtTrId("friends_unfriend_confirmation").arg(name));
    });
    menu->showCustomMenu();
}

void TFriendsFriendListPageController::onConfirmDialogAccepted()
{
    if (m_pendingUnfriend == 0)
        return;
    protocol::gameclient::sendFriendSystemAccountAction(kRemoveFriend, static_cast<uint32_t>(m_pendingUnfriend));
    m_pendingUnfriend = 0;
}

}
