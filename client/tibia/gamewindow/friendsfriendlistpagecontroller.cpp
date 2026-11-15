#include "friendsfriendlistpagecontroller.h"

#include "friends/friendsciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"

#include <memory>

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kLoadFriendList = 0;
constexpr uint8_t kRemoveFriend = 9;
constexpr uint8_t kChangeFriendGroup = 10;

friends::TFriendsFriendListStorage* friendListStorage()
{
    return friends::TFriendsStorage::instance()->friendList();
}

}

TFriendsFriendListPageController::TFriendsFriendListPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsFriendListPageController(parent)
    , m_model(new TFriendListSortProxyModel(this))
    , m_confirmDialog(new TMessageDialogController(this))
{
    m_friendList = m_model;
    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, &TFriendsFriendListPageController::onConfirmDialogAccepted);
}

QObject* TFriendsFriendListPageController::friendList() const
{
    return m_friendList;
}

TFriendsFriendListPageController::EShowFilter TFriendsFriendListPageController::showFilter() const
{
    return static_cast<EShowFilter>(m_model->showFilter());
}

void TFriendsFriendListPageController::setShowFilter(const EShowFilter ShowFilter)
{
    if (!m_model->setShowFilter(static_cast<int>(ShowFilter)))
        return;
    m_showFilter = ShowFilter;
    Q_EMIT showFilterChanged();
}

void TFriendsFriendListPageController::updateFriendList()
{
    m_model->setFriendList(friendListStorage()->friendList());
}

void TFriendsFriendListPageController::showDialogTab()
{
    // Follows the stored list while the tab is open and asks for a fresh one.
    disconnect(m_friendListConnection);
    m_friendListConnection = connect(friendListStorage(), &friends::TFriendsFriendListStorage::friendListChanged, this,
                                     &TFriendsFriendListPageController::updateFriendList);
    updateFriendList();
    protocol::gameclient::sendFriendSystemAction(kLoadFriendList);
}

void TFriendsFriendListPageController::hideDialogTab()
{
}

void TFriendsFriendListPageController::showAccountDetails(const int AccountIDRaw)
{
    // The account search shows the friend's account (looked up by the main character's name).
    QString name;
    for (const auto& entry : friendListStorage()->friendList()) {
        if (entry.character.accountId == static_cast<uint32_t>(AccountIDRaw)) {
            name = entry.character.name;
            break;
        }
    }
    auto action = std::make_shared<input::TGameActionFriendsDialogSearchAccount>();
    action->text = name;
    input::publish(action);
}

void TFriendsFriendListPageController::showContextMenuForAccountID(const int AccountIDRaw)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const auto accountId = static_cast<uint32_t>(AccountIDRaw);
    const int currentGroup = friendListStorage()->friendGroupOf(accountId);
    menu->beginCustomMenu();
    // The three groups are check entries; the account's group is checked.
    const auto group = [this, menu, accountId, currentGroup](const char* label, const int friendGroup) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(label));
        entry->setCheckable(true);
        entry->setChecked(currentGroup == friendGroup);
        connect(entry, &TContextMenuEntryItem::triggered, this,
                [this, accountId, friendGroup] { changeFriendGroup(accountId, friendGroup); });
    };
    group("friends_changefriendgroup_contacts", 1);
    group("friends_changefriendgroup_friends", 2);
    group("friends_changefriendgroup_close_friends", 3);
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("friends_unfriend")), &TContextMenuEntryItem::triggered, this, [this, accountId] {
        m_pendingUnfriend = static_cast<int>(accountId);
        showUnfriendConfirmation();
    });
    menu->showCustomMenu();
}

void TFriendsFriendListPageController::changeFriendGroup(const uint32_t accountId, const int friendGroup)
{
    protocol::gameclient::sendFriendSystemAccountAction(kChangeFriendGroup, accountId,
                                                         static_cast<uint8_t>(friends::friendGroupToProtocol(friendGroup)));
    friendListStorage()->setFriendGroup(accountId, friendGroup);
}

void TFriendsFriendListPageController::showUnfriendConfirmation()
{
    if (m_pendingUnfriend == 0)
        return;
    QString name;
    for (const auto& entry : friendListStorage()->friendList()) {
        if (entry.character.accountId == static_cast<uint32_t>(m_pendingUnfriend)) {
            name = entry.character.name;
            break;
        }
    }
    m_confirmDialog->showMessage(qtTrId("friends_unfriend_confirmation_title"), qtTrId("friends_unfriend_confirmation").arg(name));
}

void TFriendsFriendListPageController::onConfirmDialogAccepted()
{
    removeFriend(static_cast<uint32_t>(m_pendingUnfriend));
    m_pendingUnfriend = 0;
}

void TFriendsFriendListPageController::removeFriend(const uint32_t accountId)
{
    protocol::gameclient::sendFriendSystemAccountAction(kRemoveFriend, accountId);
    friendListStorage()->removeFriend(accountId);
}

}
