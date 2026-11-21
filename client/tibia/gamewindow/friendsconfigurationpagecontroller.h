#pragma once

#include "friends/friendlistmodels.h"

#include "friends/friendsciptypes.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

#include <QMetaObject>
#include <QObject>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

/// FriendsConfigurationPage.qml (CIP TFriendsConfigurationPageController):
/// what each friend group may see, from friends::TFriendsStorage.
class TFriendsConfigurationPageController final : public cip::tibia::gamewindow::TFriendsConfigurationPageController
{
    Q_OBJECT
public:
    explicit TFriendsConfigurationPageController(QObject* parent = nullptr);
    /// CIP v12: the page reads the session's friends storages while shown.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QObject* friendSettings() const override;

    void showDialogTab() override;
    void hideDialogTab() override;

    Q_INVOKABLE void setFriendGroupSetting(int FriendGroupID, bool ShowCharacterInfo, bool ShowAccountInfo,
                                          bool AllowInspect) override;

private:
    /// The current session's friends storage, or null.
    friends::TFriendsStorage* friendsStorage() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    /// Copies the stored settings (or a closed setting per group when there
    /// are none) into the model, highest group first.
    void updateSettings();

    TVisibilitySettingsModel* m_model = nullptr;
    QMetaObject::Connection m_settingsConnection;
};

}
}
