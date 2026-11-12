#pragma once

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsConfigurationPage.qml (CIP TFriendsConfigurationPageController).
class TFriendsConfigurationPageController final : public cip::tibia::gamewindow::TFriendsConfigurationPageController
{
    Q_OBJECT
public:
    explicit TFriendsConfigurationPageController(QObject* parent = nullptr);

    QObject* friendSettings() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

    Q_INVOKABLE void setFriendGroupSetting(int FriendGroupID, bool ShowCharacterInfo, bool ShowAccountInfo,
                                          bool AllowInspect) override;

private:
    cip::TFriendListModel* m_model = nullptr;
};

}
}
