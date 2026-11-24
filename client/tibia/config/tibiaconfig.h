#pragma once

#include "shared/tibiafilesystemhelper.h"

#include <QSettings>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia::config {

/// CIP config::TTibiaConfig: the client settings, `[GROUP] key` values of
/// config.ini. The file is the user's conf/config.ini when there is one, else
/// the one bundled with the client (:/conf/config.ini), whose values are the
/// defaults CipSoft ships.
class TTibiaConfig
{
public:
    /// CIP: the settings the file system helper opens for config.ini.
    explicit TTibiaConfig(const shared::TTibiaFileSystemHelper& FileSystemHelper)
        : m_pSettings(FileSystemHelper.openConfigSettings())
    {
    }
    /// Host: settings already open (tests, the free functions' fallback).
    explicit TTibiaConfig(std::shared_ptr<QSettings> Settings)
        : m_pSettings(std::move(Settings))
    {
    }

    /// CIP: an integer value of `[Group] Key`, `DefaultValue` when unset.
    qint64 clientSetting(const QString& Group, const QString& Key, qint64 DefaultValue) const;
    /// A floating-point value of `[Group] Key`, `DefaultValue` when unset.
    double clientSettingReal(const QString& Group, const QString& Key, double DefaultValue) const;
    /// A text value of `[Group] Key`, `DefaultValue` when unset.
    QString clientSettingString(const QString& Group, const QString& Key, const QString& DefaultValue) const;
    /// As clientSetting, held at `Maximum`: a larger value is written back as `Maximum`.
    qint64 clientSettingAtMost(const QString& Group, const QString& Key, qint64 DefaultValue, qint64 Maximum) const;
    /// As clientSetting, held at `Minimum`: a smaller value is written back as `Minimum`.
    qint64 clientSettingAtLeast(const QString& Group, const QString& Key, qint64 DefaultValue, qint64 Minimum) const;

    /// CIP: a URL of the [URLS] group, `DefaultValue` when unset.
    QUrl readUrl(const QString& Key, const QUrl& DefaultValue) const;
    /// CIP: [URLS] loginWebService (empty when unset).
    QUrl loginWebServiceUrl() const;

    std::shared_ptr<QSettings> settings() const { return m_pSettings; }

private:
    std::shared_ptr<QSettings> m_pSettings;
};

}
