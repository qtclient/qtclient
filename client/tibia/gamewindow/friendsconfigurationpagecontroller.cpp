#include "friendsconfigurationpagecontroller.h"

#include "friends/friendsciptypes.h"
#include "protocol/clientmessages.h"

#include <algorithm>

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kLoadSettings = 15;

}

TFriendsConfigurationPageController::TFriendsConfigurationPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsConfigurationPageController(parent)
    , m_model(new TVisibilitySettingsModel(this))
{
    m_friendSettings = m_model;
}

void TFriendsConfigurationPageController::updateSettings()
{
    std::vector<friends::TVisibilitySetting> settings = friends::TFriendsStorage::instance()->settings();
    if (settings.empty()) {
        for (int group = 0; group <= 3; ++group)
            settings.push_back({ group, false, false, false });
    }
    std::sort(settings.begin(), settings.end(),
              [](const friends::TVisibilitySetting& left, const friends::TVisibilitySetting& right) {
                  return left.friendGroup > right.friendGroup;
              });
    m_model->setItems(settings);
}

QObject* TFriendsConfigurationPageController::friendSettings() const
{
    return m_friendSettings;
}

void TFriendsConfigurationPageController::showDialogTab()
{
    // Follows the stored settings while the tab is open and asks for fresh ones.
    disconnect(m_settingsConnection);
    m_settingsConnection = connect(friends::TFriendsStorage::instance(), &friends::TFriendsStorage::visibilitySettingsChanged, this,
                                   &TFriendsConfigurationPageController::updateSettings);
    updateSettings();
    protocol::gameclient::sendFriendSystemAction(kLoadSettings);
}

void TFriendsConfigurationPageController::hideDialogTab()
{
    disconnect(m_settingsConnection);
}

void TFriendsConfigurationPageController::setFriendGroupSetting(const int FriendGroupID, const bool ShowCharacterInfo,
                                                               const bool ShowAccountInfo, const bool AllowInspect)
{
    protocol::gameclient::sendFriendSystemSetting(static_cast<uint8_t>(friends::friendGroupToProtocol(FriendGroupID)),
                                                  ShowCharacterInfo, ShowAccountInfo, AllowInspect);
}

}
