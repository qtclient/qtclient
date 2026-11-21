#pragma once

#include "friends/friendlistmodels.h"

#include "friends/friendsciptypes.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

#include <QObject>
#include <QString>

namespace tibia {
namespace game {
class IGameSession;
}
namespace input {
class IGameAction;
}
namespace gamewindow {

class TCharacterInfoDialogController;

/// FriendsAccountSearchPage.qml (CIP TFriendsAccountSearchPageController): the
/// account search result from friends::TFriendsAccountSearchStorage, and the
/// character info of one of its characters.
class TFriendsAccountSearchPageController final : public cip::tibia::gamewindow::TFriendsAccountSearchPageController
{
    Q_OBJECT
public:
    explicit TFriendsAccountSearchPageController(QObject* parent = nullptr);
    /// CIP v12: the page reads the session's friends storages while shown.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool showAccountInfo() const override;
    bool isOnline() const override;
    bool isPremium() const override;
    bool isFriendOrInvited() const override;
    bool isOnBlacklist() const override;
    bool showEmptyTab() const override;
    bool showNoResultsFound() const override;
    bool showLoadingComponent() const override;
    bool hasMainCharacter() const override;
    bool disableButtonsAfterClick() const override;
    bool showCharacterInfo() const override;
    void setShowCharacterInfo(bool show) override;
    QString loyaltyTitle() const override;
    QString lastGameLogin() const override;
    cip::tibia::gamewindow::TAccountBadgeListData* accountBadgeList() const override;
    cip::tibia::gamewindow::TAccountCharacterListData* accountCharacterList() const override;
    QObject* characterInfoDialogController() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

    /// FRIENDS_DIALOG_SEARCH_ACCOUNT: a new search for the action's character name.
    void handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    void onClickedSearchForCharacter(const QString& SearchString) override;
    void openCyclopediaCharacterInfo(int CharacterID) override;
    void refreshCurrentCharacterInfo() override;
    void onClickedInviteAsFriend() override;
    void onClickedSetOnBlacklist() override;
    void onBackButtonPressed() override;
    /// CIP slots: the character info could not be shown.
    void onErrorLoadingCharacter();
    void onNoPermissionToDisplayCharacterSummary();
    void onNoPermissionToInspectCharacter();

private:
    /// The current session's friends storage, or null.
    friends::TFriendsStorage* friendsStorage() const;
    const friends::TAccountSearchResult& searchResult() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    void setShowEmptyTab(bool show);
    void setShowNoResultsFound(bool show);
    void setShowLoadingComponent(bool show);
    void setDisableButtonsAfterClick(bool disable);
    /// Closes the character info (back to the search result).
    void closeCharacterInfo();
    /// A search result arrived: new character and badge lists.
    void onAccountSearchResults();
    void onNoResultsFound();
    /// A friend system error ends the search and enables the buttons again.
    void onFriendSystemError();
    /// Replaces the character and badge lists with the stored result's.
    void replaceLists();

    TCharacterInfoDialogController* m_characterInfo = nullptr;
};

}
}
