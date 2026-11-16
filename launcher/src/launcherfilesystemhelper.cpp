#include "launcherfilesystemhelper.h"

#include "package.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace launcher {

namespace {

const QString kGroupUrls = QStringLiteral("URLS");
const QString kGroupTiming = QStringLiteral("TIMING");
const QString kGroupUser = QStringLiteral("USER");
const QString kGroupOptions = QStringLiteral("OPTIONS");

/// Host: launcher settings defaults from the tip mods (highest priority last
/// wins), then environment overrides used by local play and the release sidecar.
QJsonObject hostSettingsDefaults()
{
    QJsonObject defaults;
    const auto merge = [&](const QJsonObject& section) {
        for (auto group = section.begin(); group != section.end(); ++group) {
            QJsonObject merged = defaults.value(group.key()).toObject();
            const QJsonObject values = group.value().toObject();
            for (auto value = values.begin(); value != values.end(); ++value)
                merged.insert(value.key(), value.value());
            defaults.insert(group.key(), merged);
        }
    };
    if (!QCoreApplication::arguments().contains(QStringLiteral("--no-mods"))) {
        QString modsRoot = qEnvironmentVariable("QTCLIENT_MODS_DIR");
        if (modsRoot.isEmpty())
            modsRoot = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mods"));
        QList<std::pair<int, QJsonObject>> sections;
        for (const QFileInfo& dir : QDir(modsRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            QFile file(QDir(dir.absoluteFilePath()).filePath(QStringLiteral("mod.json")));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QJsonObject mod = QJsonDocument::fromJson(file.readAll()).object();
            if (mod.contains(QStringLiteral("launchersettings")))
                sections.append({mod.value(QStringLiteral("priority")).toInt(),
                                 mod.value(QStringLiteral("launchersettings")).toObject()});
        }
        std::stable_sort(sections.begin(), sections.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });
        for (const auto& section : sections)
            merge(section.second);
    }
    // Tip url: QTCLIENT_URL, else a tibiametadata.json sidecar next to the launcher.
    QString tip = qEnvironmentVariable("QTCLIENT_URL");
    if (tip.isEmpty()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        for (const QString& path : {appDir + QStringLiteral("/tibiametadata.json"),
                                    appDir + QStringLiteral("/../Resources/tibiametadata.json"),
                                    appDir + QStringLiteral("/../tibiametadata.json")}) {
            if (QFileInfo(path).isFile()) {
                tip = QUrl::fromLocalFile(QFileInfo(path).canonicalFilePath()).toString();
                break;
            }
        }
    }
    if (tip.isEmpty()) {
        const QString cdn = qEnvironmentVariable("QTCLIENT_CDN_URL");
        if (!cdn.isEmpty())
            tip = cdn + QStringLiteral("/launcher/tibiametadata.json");
    }
    if (!tip.isEmpty()) {
        QJsonObject urls = defaults.value(kGroupUrls).toObject();
        urls.insert(QStringLiteral("launcherConfiguration"), tip);
        defaults.insert(kGroupUrls, urls);
    }
    return defaults;
}

} // namespace

// --- TLauncherSettings -------------------------------------------------------------------

TLauncherSettings::TLauncherSettings(const QJsonObject& defaults, const QString& overrideFile, bool epicDistribution)
    : m_defaults(defaults)
    , m_epicDistribution(epicDistribution)
{
    if (QFileInfo::exists(overrideFile))
        m_overrideSettings = std::make_unique<QSettings>(overrideFile, QSettings::IniFormat);
}

QVariant TLauncherSettings::lookup(const QString& group, const QString& key, const QVariant& fallback) const
{
    if (m_overrideSettings && m_overrideSettings->childGroups().contains(group)) {
        m_overrideSettings->beginGroup(group);
        const bool found = m_overrideSettings->contains(key);
        const QVariant value = found ? m_overrideSettings->value(key) : QVariant();
        m_overrideSettings->endGroup();
        if (found)
            return value;
    }
    const QJsonValue builtIn = m_defaults.value(group).toObject().value(key);
    const QVariant defaultValue = builtIn.isUndefined() ? fallback : builtIn.toVariant();
    auto& user = const_cast<QSettings&>(m_userSettings);
    user.beginGroup(group);
    const QVariant value = user.value(key, defaultValue);
    user.endGroup();
    return value;
}

QString TLauncherSettings::stringValue(const QString& group, const QString& key, const QString& fallback) const
{
    return lookup(group, key, fallback).toString();
}

int TLauncherSettings::intValue(const QString& group, const QString& key, int fallback) const
{
    return lookup(group, key, fallback).toInt();
}

bool TLauncherSettings::boolValue(const QString& group, const QString& key, bool fallback) const
{
    return lookup(group, key, fallback).toBool();
}

void TLauncherSettings::setValue(const QString& group, const QString& key, const QVariant& value)
{
    m_userSettings.beginGroup(group);
    m_userSettings.setValue(key, value);
    m_userSettings.endGroup();
}

// --- TLauncherFilesystemHelper -----------------------------------------------------------

TLauncherFilesystemHelper::TLauncherFilesystemHelper()
{
    const QString overrideFile =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("launchersettings.ini"));
    m_settings = std::make_unique<TLauncherSettings>(hostSettingsDefaults(), overrideFile,
                                                     /*epicDistribution=*/false);
}

QUrl TLauncherFilesystemHelper::launcherConfigurationUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("launcherConfiguration")));
}

QUrl TLauncherFilesystemHelper::cdnDiagnosticsTraceUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("cdnDiagnosticsTrace")));
}

int TLauncherFilesystemHelper::downloadErrorTimeout() const
{
    return m_settings->intValue(kGroupTiming, QStringLiteral("downloadErrorTimeout"), 0);
}

int TLauncherFilesystemHelper::clientStartTimeout() const
{
    return m_settings->intValue(kGroupTiming, QStringLiteral("clientStartTimeout"), 0);
}

bool TLauncherFilesystemHelper::eulaAccepted() const
{
    return m_settings->boolValue(kGroupUser, QStringLiteral("eulaAccepted"), false);
}

void TLauncherFilesystemHelper::setEulaAccepted(bool accepted)
{
    m_settings->setValue(kGroupUser, QStringLiteral("eulaAccepted"), accepted);
}

bool TLauncherFilesystemHelper::prerequisitesInstalled() const
{
    return m_settings->boolValue(kGroupUser, QStringLiteral("prerequistesInstalled"), false);
}

void TLauncherFilesystemHelper::setPrerequisitesInstalled(bool installed)
{
    m_settings->setValue(kGroupUser, QStringLiteral("prerequistesInstalled"), installed);
}

bool TLauncherFilesystemHelper::ipV4Mode() const
{
    return m_settings->boolValue(kGroupOptions, QStringLiteral("ipV4Mode"), true);
}

int TLauncherFilesystemHelper::configurationTransportTimeout() const
{
    return m_settings->intValue(kGroupOptions, QStringLiteral("configurationTransportTimeOut"), 750);
}

int TLauncherFilesystemHelper::chunkTransportTimeout() const
{
    return m_settings->intValue(kGroupOptions, QStringLiteral("chunkTransportTimeOut"), 0);
}

int TLauncherFilesystemHelper::retries() const
{
    return m_settings->intValue(kGroupOptions, QStringLiteral("retries"), 5);
}

QDir TLauncherFilesystemHelper::applicationDirectory()
{
    QDir directory(QCoreApplication::applicationDirPath());
    if (directory.absolutePath().endsWith(QLatin1String("Contents/MacOS"))) {
        directory.cdUp();
        directory.cdUp();
    }
    return directory;
}

QDir TLauncherFilesystemHelper::dataDirectory()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return QDir(path);
}

QDir TLauncherFilesystemHelper::logDirectory()
{
    const QString path = dataDirectory().filePath(QStringLiteral("log"));
    QDir().mkpath(path);
    return QDir(path);
}

QDir TLauncherFilesystemHelper::packagesDirectory()
{
    const QString path = dataDirectory().filePath(QStringLiteral("packages"));
    QDir().mkpath(path);
    return QDir(path);
}

QDir TLauncherFilesystemHelper::packageDirectory(const TPackage& package)
{
    if (package.isLauncher())
        return applicationDirectory();
    return QDir(packagesDirectory().filePath(package.directoryName()));
}

QString TLauncherFilesystemHelper::filePath(const QDir& directory, const QString& fileName)
{
    return directory.filePath(fileName);
}

void TLauncherFilesystemHelper::linkLauncherIntoDataDirectory()
{
    QFile executable(QCoreApplication::applicationFilePath());
    const QString link = dataDirectory().absoluteFilePath(QFileInfo(executable).fileName());
    if (QFile::exists(link) && !QFile::symLinkTarget(link).isEmpty())
        return;
    QFile::remove(link);
    executable.link(link);
}

bool TLauncherFilesystemHelper::fileExists(const QString& path)
{
    return QFileInfo(path).isFile();
}

QByteArray TLauncherFilesystemHelper::readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool TLauncherFilesystemHelper::writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(data) != data.size())
        return false;
    return file.commit();
}

bool TLauncherFilesystemHelper::removeFile(const QString& path)
{
    return !QFileInfo::exists(path) || QFile::remove(path);
}

bool TLauncherFilesystemHelper::renameFile(const QString& from, const QString& to)
{
    removeFile(to);
    return QFile::rename(from, to);
}

QByteArray TLauncherFilesystemHelper::localPackageMetaData(const TPackage& package)
{
    return readFile(packageDirectory(package).filePath(packageMetaDataFileName()));
}

QByteArray TLauncherFilesystemHelper::localAssetMetaData(const TPackage& package)
{
    return readFile(packageDirectory(package).filePath(assetMetaDataFileName()));
}

QByteArray TLauncherFilesystemHelper::localPackageVersion(const TPackage& package)
{
    const QDir directory = packageDirectory(package);
    if (fileExists(directory.filePath(packageMarkerFileName())))
        return readFile(directory.filePath(packageMarkerFileName()));
    return readFile(directory.filePath(partialPackageMarkerFileName()));
}

QByteArray TLauncherFilesystemHelper::localAssetVersion(const TPackage& package)
{
    return readFile(packageDirectory(package).filePath(assetVersionFileName()));
}

} // namespace launcher
