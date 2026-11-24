#pragma once

#include <QByteArray>
#include <QDir>
#include <QJsonObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QVariant>

#include <memory>
#include <vector>

namespace launcher {

class TPackage;
struct TFileInPackage;
struct TPackageBuild;
struct THostChannel;

/// Launcher settings: an optional `launchersettings.ini` next to the launcher
/// overrides the user's settings, which fall back to built-in defaults.
/// CipSoft ships its defaults (CipSoft server urls) as a resource; qtclient
/// takes them from the tip mods (`mods/<id>/mod.json` → "launchersettings")
/// so no CipSoft url is contacted by default.
class TLauncherSettings
{
public:
    TLauncherSettings(const QJsonObject& defaults, const QString& overrideFile, bool epicDistribution);

    QString stringValue(const QString& group, const QString& key, const QString& fallback = {}) const;
    int intValue(const QString& group, const QString& key, int fallback) const;
    bool boolValue(const QString& group, const QString& key, bool fallback) const;
    void setValue(const QString& group, const QString& key, const QVariant& value);
    bool isEpicDistribution() const { return m_epicDistribution; }

private:
    QVariant lookup(const QString& group, const QString& key, const QVariant& fallback) const;

    QSettings m_userSettings;
    std::unique_ptr<QSettings> m_overrideSettings;
    QJsonObject m_defaults;
    bool m_epicDistribution = false;
};

/// tibia::launcher::TLauncherFilesystemHelper: settings plus the on-disk layout
/// of installed packages (AppData/packages/<directoryname>, install markers).
class TLauncherFilesystemHelper
{
public:
    TLauncherFilesystemHelper();

    TLauncherSettings& settings() { return *m_settings; }
    const TLauncherSettings& settings() const { return *m_settings; }

    // URLS
    QUrl launcherConfigurationUrl() const;
    QUrl cdnDiagnosticsTraceUrl() const;
    bool appendNocacheToPackageUrls() const;
    /// Host: where the system survey is posted. CipSoft takes the tip's
    /// "hardwaresurvey" url; qtclient takes it from mod configuration so no
    /// CipSoft url is contacted (empty: no survey).
    QUrl systemSurveyUrl() const;
    /// Host: origin of the tip's CDN bundle (QTCLIENT_CDN_URL, else the pack brand's); empty for stock.
    static QString cdnBase();
    /// Host: [URLS] clientRevision — a JSON document whose "revision" is the
    /// client revision handed to the client; empty: the package version.
    QUrl clientRevisionUrl() const;
    /// Host: [URLS] contentStore — content-addressed store base
    /// (`<store>/<aa>/<packedhash>`) for file downloads; empty: CipSoft's file urls.
    QUrl contentStoreUrl() const;
    /// Host: where `file` is downloaded from (its url, or the content store blob).
    QUrl fileDownloadUrl(const TFileInPackage& file) const;
    /// Host: channels of `build` enabled by the tip's launcher mod capabilities.
    static std::vector<THostChannel> enabledHostChannels(const TPackageBuild& build);
    /// Host (macOS): restores the framework symlinks a package file list cannot carry.
    static void repairMacFrameworkVersions(const QDir& bundle);
    // TIMING
    int downloadErrorTimeout() const;
    int clientStartTimeout() const;
    // USER
    bool eulaAccepted() const;
    void setEulaAccepted(bool accepted);
    bool prerequisitesInstalled() const;
    void setPrerequisitesInstalled(bool installed);
    // OPTIONS
    bool ipV4Mode() const;
    int configurationTransportTimeout() const;
    int chunkTransportTimeout() const;
    int retries() const;

    /// Directory of the running application (the bundle root on macOS).
    static QDir applicationDirectory();
    static QDir dataDirectory();
    static QDir logDirectory();
    static QDir packagesDirectory();
    /// Install directory of `package` (the launcher installs into its own directory).
    static QDir packageDirectory(const TPackage& package);
    static QString filePath(const QDir& directory, const QString& fileName);
    /// Keeps a link to the running launcher executable in the data directory.
    static void linkLauncherIntoDataDirectory();

    static QString packageMarkerFileName() { return QStringLiteral("package.json.version"); }
    static QString partialPackageMarkerFileName() { return QStringLiteral("partial.package.json.version"); }
    static QString packageMetaDataFileName() { return QStringLiteral("package.json"); }
    static QString assetMetaDataFileName() { return QStringLiteral("assets.json"); }
    static QString assetVersionFileName() { return QStringLiteral("assets.json.sha256"); }
    /// Marker file in a package directory that makes it the default client.
    static QString defaultPackageMarkerFileName() { return QStringLiteral("default"); }
    /// Marker file in a package directory that forces an update of that package on the next start.
    static QString forcePatchMarkerFileName() { return QStringLiteral("FORCE_PATCH"); }
    /// Suffix of the copies a self-update keeps of the files it replaces.
    static QString backupFileSuffix() { return QStringLiteral(".old"); }
    /// True when `distribution.channel` next to the application reads "Epic".
    static bool isEpicDistribution();

    static bool fileExists(const QString& path);
    static QByteArray readFile(const QString& path);
    static bool writeFile(const QString& path, const QByteArray& data);
    static bool removeFile(const QString& path);
    static bool renameFile(const QString& from, const QString& to);

    /// Local package.json / assets.json of an installed package.
    static QByteArray localPackageMetaData(const TPackage& package);
    static QByteArray localAssetMetaData(const TPackage& package);
    static QByteArray localPackageVersion(const TPackage& package);
    static QByteArray localAssetVersion(const TPackage& package);

private:
    std::unique_ptr<TLauncherSettings> m_settings;
};

} // namespace launcher
