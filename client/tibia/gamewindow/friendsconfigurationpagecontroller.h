#pragma once

#include "friends/friendlistmodels.h"

#include <cip_gen.h>

#include <QMetaObject>
#include <QObject>

namespace tibia {
namespace gamewindow {

/// FriendsConfigurationPage.qml (CIP TFriendsConfigurationPageController):
/// what each friend group may see, from friends::TFriendsStorage.
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
    /// Copies the stored settings (or a closed setting per group when there
    /// are none) into the model, highest group first.
    void updateSettings();

    TVisibilitySettingsModel* m_model = nullptr;
    QMetaObject::Connection m_settingsConnection;
};

}
}
