#include "friendsconfigurationpagecontroller.h"

#include "friends/friendsciptypes.h"
#include "protocol/clientmessages.h"

namespace tibia::gamewindow {

namespace {

constexpr uint8_t kLoadSettings = 15;

}

TFriendsConfigurationPageController::TFriendsConfigurationPageController(QObject* parent)
    : cip::tibia::gamewindow::TFriendsConfigurationPageController(parent)
    , m_model(new TFriendsRowsModel({ "settingType", "showCharacterInfo", "showAccountInfo", "allowInspect",
                                      "enableShowCharacterInfo", "enableShowAccountInfo", "enableAllowInspect" },
                                    this))
{
    m_friendSettings = m_model;
    connect(friends::TFriendsStorage::instance(), &friends::TFriendsStorage::visibilitySettingsChanged, this,
            &TFriendsConfigurationPageController::rebuildRows);
    rebuildRows();
}

void TFriendsConfigurationPageController::rebuildRows()
{
    QVector<QVariantMap> rows;
    for (const auto& setting : friends::TFriendsStorage::instance()->settings()) {
        rows.push_back({
            { QStringLiteral("settingType"), static_cast<int>(setting.setting_type()) },
            { QStringLiteral("showCharacterInfo"), setting.show_character_info() },
            { QStringLiteral("showAccountInfo"), setting.show_account_info() },
            { QStringLiteral("allowInspect"), setting.allow_inspect() },
            { QStringLiteral("enableShowCharacterInfo"), true },
            { QStringLiteral("enableShowAccountInfo"), true },
            { QStringLiteral("enableAllowInspect"), true },
        });
    }
    m_model->setRows(std::move(rows));
}

QObject* TFriendsConfigurationPageController::friendSettings() const
{
    return m_friendSettings;
}

void TFriendsConfigurationPageController::showDialogTab()
{
    protocol::gameclient::sendFriendSystemAction(kLoadSettings);
}

void TFriendsConfigurationPageController::hideDialogTab()
{
}

void TFriendsConfigurationPageController::setFriendGroupSetting(const int FriendGroupID, const bool ShowCharacterInfo,
                                                               const bool ShowAccountInfo, const bool AllowInspect)
{
    protocol::gameclient::sendFriendSystemSetting(static_cast<uint8_t>(FriendGroupID), ShowCharacterInfo, ShowAccountInfo,
                                                  AllowInspect);
}

}
