#pragma once

#include <cip_gen.h>

#include <QObject>
#include <QString>

namespace tibia {
namespace gamewindow {

class TCharacterInfoDialogController;

/// FriendsAccountSearchPage.qml (CIP TFriendsAccountSearchPageController).
class TFriendsAccountSearchPageController final : public cip::tibia::gamewindow::TFriendsAccountSearchPageController
{
    Q_OBJECT
public:
    explicit TFriendsAccountSearchPageController(QObject* parent = nullptr);

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

public Q_SLOTS:
    void onClickedSearchForCharacter(const QString& SearchString) override;
    void openCyclopediaCharacterInfo(int CharacterID) override;
    void refreshCurrentCharacterInfo() override;
    void onClickedInviteAsFriend() override;
    void onClickedSetOnBlacklist() override;
    void onBackButtonPressed() override;
    /// CIP moc slot (skipped from cip_gen shared_ptr type); qtclient name parity.
    void handleGameAction();
    /// CIP moc slots (friend-system errors while the search tab is open).
    void onErrorLoadingCharacter();
    void onNoPermissionToDisplayCharacterSummary();
    void onNoPermissionToInspectCharacter();

private:
    void setLoading(bool loading);
    void setEmpty(bool empty, bool noResults);

    cip::tibia::gamewindow::TAccountCharacterListData* m_characters = nullptr;
    cip::tibia::gamewindow::TAccountBadgeListData* m_badges = nullptr;
    TCharacterInfoDialogController* m_characterInfo = nullptr;
};

}
}
