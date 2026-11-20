#include "launcherfilesystemhelper.h"

#include "brand.h"
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

/// Host: origin of the product CDN bundle (QTCLIENT_CDN_URL, else the tip
/// pack's brand `defaultCdn`); empty for stock CipSoft.
QString hostCdnBase()
{
    QString cdn = qEnvironmentVariable("QTCLIENT_CDN_URL");
    if (cdn.isEmpty() && !LauncherBrand::instance().noMods)
        cdn = LauncherBrand::instance().defaultCdn;
    while (cdn.endsWith(QLatin1Char('/')))
        cdn.chop(1);
    return cdn;
}

/// Host: content-addressed store (`<cdn>/content/<aa>/<packedhash>`) for file
/// downloads. QTCLIENT_CDN_CONTENT_STORE=1/0 forces it; otherwise only the
/// local test origins use it, and tips keep CipSoft's per-file urls.
bool hostUseContentStore(const QString& cdn)
{
    const QString force = qEnvironmentVariable("QTCLIENT_CDN_CONTENT_STORE");
    if (force == QLatin1String("1"))
        return !cdn.isEmpty();
    if (force == QLatin1String("0") || cdn.isEmpty())
        return false;
    const QString host = QUrl(cdn).host();
    return host.contains(QLatin1String("example.invalid")) || host.contains(QLatin1String("qtclient.local"));
}

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
    // The in-tree default mod (compiled in) gives the lowest-priority defaults.
    {
        QFile file(QStringLiteral(":/mods/default/mod.json"));
        if (file.open(QIODevice::ReadOnly))
            merge(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("launchersettings")).toObject());
    }
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
    const QString cdn = hostCdnBase();
    QJsonObject urls = defaults.value(kGroupUrls).toObject();
    // An explicit QTCLIENT_CDN_URL wins over the mods; the brand's CDN only fills a gap.
    if (tip.isEmpty() && !cdn.isEmpty()
        && (!qEnvironmentVariableIsEmpty("QTCLIENT_CDN_URL") || !urls.contains(QStringLiteral("launcherConfiguration"))))
        tip = cdn + QStringLiteral("/launcher/tibiametadata.json");
    if (!tip.isEmpty())
        urls.insert(QStringLiteral("launcherConfiguration"), tip);
    // The release gate: the portal accepts latest/release.json "revision" as the
    // client revision at login (errorCode 7 otherwise).
    if (!cdn.isEmpty() && !urls.contains(QStringLiteral("releaseManifest")))
        urls.insert(QStringLiteral("releaseManifest"), cdn + QStringLiteral("/latest/release.json"));
    if (hostUseContentStore(cdn) && !urls.contains(QStringLiteral("contentStore")))
        urls.insert(QStringLiteral("contentStore"), cdn + QStringLiteral("/content"));
    // The browser host pins the hardware survey to the page origin (a stamped
    // site on another host is cross-origin there), as it does for login.
    const QString survey = qEnvironmentVariable("QTCLIENT_SYSTEM_SURVEY_URL");
    if (!survey.isEmpty())
        urls.insert(QStringLiteral("systemSurvey"), survey);
    defaults.insert(kGroupUrls, urls);
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
    m_settings = std::make_unique<TLauncherSettings>(hostSettingsDefaults(), overrideFile, isEpicDistribution());
}

bool TLauncherFilesystemHelper::isEpicDistribution()
{
    const QByteArray channel = readFile(applicationDirectory().filePath(QStringLiteral("distribution.channel")));
    return QString::fromUtf8(channel).trimmed().compare(QLatin1String("Epic"), Qt::CaseSensitive) == 0;
}

QUrl TLauncherFilesystemHelper::launcherConfigurationUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("launcherConfiguration")));
}

QUrl TLauncherFilesystemHelper::cdnDiagnosticsTraceUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("cdnDiagnosticsTrace")));
}

QUrl TLauncherFilesystemHelper::systemSurveyUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("systemSurvey")));
}

bool TLauncherFilesystemHelper::appendNocacheToPackageUrls() const
{
    return m_settings->boolValue(kGroupUrls, QStringLiteral("appendNocacheToPackageUrls"), false);
}

QString TLauncherFilesystemHelper::cdnBase()
{
    return hostCdnBase();
}

QUrl TLauncherFilesystemHelper::releaseManifestUrl() const
{
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("releaseManifest")));
}

QUrl TLauncherFilesystemHelper::contentStoreUrl() const
{
    if (qEnvironmentVariable("QTCLIENT_CDN_CONTENT_STORE") == QLatin1String("0"))
        return {};
    return QUrl(m_settings->stringValue(kGroupUrls, QStringLiteral("contentStore")));
}

QUrl TLauncherFilesystemHelper::fileDownloadUrl(const TFileInPackage& file) const
{
    const QUrl store = contentStoreUrl();
    const QString hash = file.packedHash.trimmed().toLower();
    if (!store.isValid() || store.isEmpty() || hash.size() != 64)
        return file.url;
    for (const QChar c : hash) {
        if (!c.isDigit() && (c < QLatin1Char('a') || c > QLatin1Char('f')))
            return file.url;
    }
    QString base = store.toString();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return QUrl(base + QLatin1Char('/') + hash.left(2) + QLatin1Char('/') + hash);
}

std::vector<THostChannel> TLauncherFilesystemHelper::enabledHostChannels(const TPackageBuild& build)
{
    std::vector<THostChannel> channels;
    const LauncherMods& mods = LauncherMods::instance();
    for (const THostChannel& channel : build.hostChannels) {
        const bool enabled = (channel.name == QLatin1String("mods") && mods.clientModsChannel)
                             || (channel.name == QLatin1String("minimap") && mods.minimapChannel);
        if (enabled && channel.url.isValid() && !channel.url.isEmpty())
            channels.push_back(channel);
    }
    return channels;
}

void TLauncherFilesystemHelper::repairMacFrameworkVersions(const QDir& bundle)
{
#ifdef Q_OS_MACOS
    // Packages ship file lists without symlinks: Qt frameworks need
    // Versions/Current plus the top-level Resources / Headers / binary links.
    // The package directory is the bundle (CipSoft: packages/Tibia.app).
    for (const QFileInfo& nested : bundle.entryInfoList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot))
        repairMacFrameworkVersions(QDir(nested.absoluteFilePath()));
    const QDir frameworks(bundle.filePath(QStringLiteral("Contents/Frameworks")));
    if (!frameworks.exists())
        return;
    for (const QFileInfo& framework :
         frameworks.entryInfoList({QStringLiteral("*.framework")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir versions(QDir(framework.absoluteFilePath()).filePath(QStringLiteral("Versions")));
        if (!versions.exists())
            continue;
        const QString current = versions.filePath(QStringLiteral("Current"));
        if (QFileInfo::exists(current))
            continue;
        QString target;
        for (const QFileInfo& entry : versions.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.fileName() == QLatin1String("A")) {
                target = entry.fileName();
                break;
            }
            if (target.isEmpty() && entry.fileName() != QLatin1String("Current"))
                target = entry.fileName();
        }
        if (target.isEmpty())
            continue;
        if (!QFile::link(target, current)) {
            continue;
        }
        const QDir root(framework.absoluteFilePath());
        const QString binary = framework.completeBaseName();
        for (const QString& leaf : {QStringLiteral("Resources"), QStringLiteral("Headers"), QStringLiteral("Modules"),
                                    binary}) {
            const QString link = root.filePath(leaf);
            const QString destination = QStringLiteral("Versions/Current/") + leaf;
            if (QFileInfo::exists(link) || !QFileInfo::exists(root.filePath(destination)))
                continue;
            QFile::link(destination, link);
        }
    }
#else
    Q_UNUSED(bundle);
#endif
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
