#include "friendsconfigurationpagecontroller.h"

namespace tibia::gamewindow {

TFriendsConfigurationPageController::TFriendsConfigurationPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsConfigurationPageController(parent)
    , m_model(new cip::TFriendListModel(this))
{
    m_friendSettings = m_model;
}

QObject* TFriendsConfigurationPageController::friendSettings() const
{
    return m_friendSettings;
}

void TFriendsConfigurationPageController::showDialogTab()
{
}

void TFriendsConfigurationPageController::hideDialogTab()
{
}

void TFriendsConfigurationPageController::setFriendGroupSetting(const int FriendGroupID, const bool ShowCharacterInfo,
                                                               const bool ShowAccountInfo, const bool AllowInspect)
{
    Q_UNUSED(FriendGroupID);
    Q_UNUSED(ShowCharacterInfo);
    Q_UNUSED(ShowAccountInfo);
    Q_UNUSED(AllowInspect);
}

}
