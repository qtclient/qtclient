#include "package.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>
#include <QUrlQuery>

#include <algorithm>

namespace launcher {

EOperatingSystem currentOperatingSystem()
{
    const QString product = QSysInfo::productType();
    if (product.compare(QLatin1String("windows"), Qt::CaseInsensitive) == 0)
        return EOperatingSystem::Windows;
    if (product.compare(QLatin1String("macos"), Qt::CaseInsensitive) == 0)
        return EOperatingSystem::MacOS;
    if (QSysInfo::kernelType().compare(QLatin1String("linux"), Qt::CaseInsensitive) == 0)
        return EOperatingSystem::Linux;
    return EOperatingSystem::Unknown;
}

quint8 buildCpuArchitecture()
{
    const QString arch = QSysInfo::buildCpuArchitecture();
    if (arch.compare(QLatin1String("i386"), Qt::CaseInsensitive) == 0)
        return ArchitectureI386;
    if (arch.compare(QLatin1String("x86_64"), Qt::CaseInsensitive) == 0)
        return ArchitectureX86_64;
    return arch.compare(QLatin1String("arm64"), Qt::CaseInsensitive) == 0 ? ArchitectureArm64 : 0;
}

quint8 currentCpuArchitecture()
{
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch.compare(QLatin1String("i386"), Qt::CaseInsensitive) == 0)
        return ArchitectureI386;
    return arch.compare(QLatin1String("x86_64"), Qt::CaseInsensitive) == 0 ? ArchitectureX86_64 : 0;
}

QUrl resolveConfigurationUrl(const QUrl& base, const QUrl& url)
{
    if (!url.isRelative())
        return url;
    const QString basePath = base.path();
    const char* separator = "/";
    if (basePath.endsWith(QLatin1Char('/')) || url.path().startsWith(QLatin1Char('/')))
        separator = "";
    QUrl resolved(base);
    resolved.setPath(base.path() + QString::fromUtf8(separator) + url.path(), QUrl::DecodedMode);
    resolved.setQuery(QUrlQuery());
    return resolved;
}

// --- TPackageConfiguration ------------------------------------------------------------

TPackageConfiguration::TPackageConfiguration(const QByteArray& packageVersion, const QByteArray& assetsVersion,
                                             const QUrl& packageUrl, const QUrl& assetsUrl)
    : m_packageUrl(packageUrl)
    , m_assetsUrl(assetsUrl)
    , m_packageVersion(QString::fromLocal8Bit(packageVersion))
    , m_assetsVersion(QString::fromLocal8Bit(assetsVersion))
{
}

QString TPackageConfiguration::packageVersion() const
{
    return m_packageVersion.isEmpty() ? QStringLiteral("?") : m_packageVersion;
}

QString TPackageConfiguration::assetsVersion() const
{
    return m_assetsVersion.isEmpty() ? QStringLiteral("?") : m_assetsVersion;
}

QString TPackageConfiguration::combinedVersion() const
{
    return QStringLiteral("P%1 A%2").arg(packageVersion(), assetsVersion());
}

namespace {

TFileInPackage parseFileInPackage(const QJsonObject& file, const QUrl& baseUrl)
{
    TFileInPackage parsed;
    parsed.url = resolveConfigurationUrl(baseUrl, QUrl(file.value(QStringLiteral("url")).toString()));
    parsed.localFile = file.value(QStringLiteral("localfile")).toString();
    parsed.packedSize = file.value(QStringLiteral("packedsize")).toInt();
    parsed.packedHash = file.value(QStringLiteral("packedhash")).toString();
    parsed.unpackedSize = file.value(QStringLiteral("unpackedsize")).toInt();
    parsed.unpackedHash = file.value(QStringLiteral("unpackedhash")).toString();
    parsed.unpack = file.value(QStringLiteral("unpack")).toBool();
    parsed.executable = file.value(QStringLiteral("executable")).toBool();
    return parsed;
}

} // namespace

bool TPackageConfiguration::load(const QByteArray& packageMetaData, const QByteArray& assetMetaData)
{
    QJsonParseError error{};
    const QJsonDocument package = QJsonDocument::fromJson(packageMetaData, &error);
    if (error.error != QJsonParseError::NoError) {
        qCritical() << "JSON package configuration file could not be parsed, using default package.";
        return false;
    }
    QJsonDocument assets;
    if (!assetMetaData.isEmpty()) {
        assets = QJsonDocument::fromJson(assetMetaData, &error);
        if (error.error != QJsonParseError::NoError) {
            qCritical() << "JSON asset configuration file could not be parsed, using default package.";
            return false;
        }
    }
    const QJsonObject object = package.object();
    m_executable = object.value(QStringLiteral("executable")).toString();
    const QString generation = object.value(QStringLiteral("generation")).toString();
    if (generation.compare(QLatin1String("first"), Qt::CaseInsensitive) == 0)
        m_generation = 1;
    else
        m_generation = generation.compare(QLatin1String("second"), Qt::CaseInsensitive) == 0 ? 2 : 0;
    const QJsonValue files = object.value(QStringLiteral("files"));
    if (files.isArray()) {
        for (const QJsonValue& file : files.toArray())
            m_files.push_back(parseFileInPackage(file.toObject(), m_packageUrl.adjusted(QUrl::RemoveFilename)));
    }
    const QJsonValue parameters = object.value(QStringLiteral("parameters"));
    if (parameters.isArray()) {
        for (const QJsonValue& parameter : parameters.toArray())
            m_parameters.append(parameter.toString());
    }
    if (assets.isObject()) {
        const QJsonValue assetFiles = assets.object().value(QStringLiteral("files"));
        if (assetFiles.isArray()) {
            for (const QJsonValue& file : assetFiles.toArray())
                m_files.push_back(parseFileInPackage(file.toObject(), m_assetsUrl.adjusted(QUrl::RemoveFilename)));
        }
    }
    std::sort(m_files.begin(), m_files.end(),
              [](const TFileInPackage& a, const TFileInPackage& b) { return a.localFile < b.localFile; });
    return true;
}

// --- TPackageBuild -----------------------------------------------------------------------

TPackageBuild::TPackageBuild(TPackage* owner, const QJsonObject& build, const QUrl& baseUrl)
    : package(owner)
{
    const QString osName = build.value(QStringLiteral("os")).toString();
    if (osName.compare(QLatin1String("windows")) == 0)
        os = EOperatingSystem::Windows;
    else if (osName.compare(QLatin1String("macos")) == 0)
        os = EOperatingSystem::MacOS;
    else
        os = osName.compare(QLatin1String("linux")) == 0 ? EOperatingSystem::Linux : EOperatingSystem::Unknown;

    const QString architecture = build.value(QStringLiteral("architecture")).toString();
    quint8 mask = architecture.indexOf(QLatin1String("i386")) != -1 ? ArchitectureI386 : 0;
    if (architecture.indexOf(QLatin1String("x86_64")) != -1)
        mask += ArchitectureX86_64;
    if (architecture.indexOf(QLatin1String("arm64")) != -1)
        mask += ArchitectureArm64;
    architectures = mask;

    url = resolveConfigurationUrl(baseUrl, QUrl(build.value(QStringLiteral("url")).toString()));
    const auto optionalUrl = [&](const char* key) {
        const QString name = QString::fromLatin1(key);
        return build.contains(name) ? resolveConfigurationUrl(baseUrl, QUrl(build.value(name).toString())) : QUrl();
    };
    versionUrl = optionalUrl("versionurl");
    assetsUrl = optionalUrl("assetsurl");
    assetsVersionUrl = optionalUrl("assetsversionurl");
    // Host channels of the release tip (see THostChannel).
    for (const char* channel : {"mods", "minimap"}) {
        const QString name = QString::fromLatin1(channel);
        const QString key = name + QStringLiteral("url");
        if (!build.contains(key))
            continue;
        THostChannel host;
        host.name = name;
        host.url = resolveConfigurationUrl(baseUrl, QUrl(build.value(key).toString()));
        const QString versionKey = name + QStringLiteral("versionurl");
        if (build.contains(versionKey))
            host.versionUrl = resolveConfigurationUrl(baseUrl, QUrl(build.value(versionKey).toString()));
        hostChannels.push_back(host);
    }
}

QByteArray hostCompositeAssetsVersion(const QByteArray& assetsVersion,
                                      const std::vector<std::pair<QString, QByteArray>>& channelVersions)
{
    if (channelVersions.empty())
        return assetsVersion;
    QByteArray composite = assetsVersion.trimmed();
    for (const auto& [name, version] : channelVersions)
        composite += ' ' + name.toUtf8() + '=' + version.trimmed();
    return composite;
}

QByteArray hostMergeChannelManifests(const QByteArray& assetMetaData,
                                     const std::vector<std::pair<QUrl, QByteArray>>& channelManifests)
{
    if (channelManifests.empty())
        return assetMetaData;
    QJsonObject assets = QJsonDocument::fromJson(assetMetaData).object();
    QJsonArray files = assets.value(QStringLiteral("files")).toArray();
    for (const auto& [manifestUrl, manifest] : channelManifests) {
        const QJsonArray channelFiles =
            QJsonDocument::fromJson(manifest).object().value(QStringLiteral("files")).toArray();
        for (const QJsonValue& value : channelFiles) {
            QJsonObject file = value.toObject();
            // Channel manifests use urls relative to themselves (e.g. ../assets-current/…).
            file.insert(QStringLiteral("url"),
                        manifestUrl.resolved(QUrl(file.value(QStringLiteral("url")).toString())).toString());
            files.append(file);
        }
    }
    assets.insert(QStringLiteral("files"), files);
    return QJsonDocument(assets).toJson(QJsonDocument::Compact);
}

// --- TPackage ----------------------------------------------------------------------------

TPackage::TPackage(int type, const QJsonObject& package, const QUrl& baseUrl)
    : m_type(type)
{
    m_name = package.value(QStringLiteral("name")).toString();
    const QString variant = package.value(QStringLiteral("variant")).toString();
    if (variant.compare(QLatin1String("regular"), Qt::CaseInsensitive) == 0)
        m_variant = VariantRegular;
    else if (variant.compare(QLatin1String("preview"), Qt::CaseInsensitive) == 0)
        m_variant = VariantPreview;
    else if (variant.compare(QLatin1String("test"), Qt::CaseInsensitive) == 0)
        m_variant = VariantTest;
    else
        m_variant = variant.compare(QLatin1String("beta"), Qt::CaseInsensitive) == 0 ? VariantBeta : VariantUnknown;
    m_directoryName = package.value(QStringLiteral("directoryname")).toString();
    if (package.contains(QStringLiteral("logolinktarget")))
        m_logoLinkTarget = QUrl(package.value(QStringLiteral("logolinktarget")).toString());
    m_installAutomatically = package.value(QStringLiteral("installautomatically")).toBool();
    if (package.value(QStringLiteral("builds")).isArray()) {
        for (const QJsonValue& build : package.value(QStringLiteral("builds")).toArray())
            m_builds.push_back(std::make_unique<TPackageBuild>(this, build.toObject(), baseUrl));
    }
#ifdef Q_OS_MACOS
    // Client packages are application bundles on macOS. Product tips already
    // name the bundle ("IdleOT.app"); only bare names get the suffix.
    if (!m_directoryName.endsWith(QLatin1String(".app")))
        m_directoryName.append(QLatin1String(".app"));
#endif
}

TPackage::~TPackage() = default;

QString TPackage::statusName(EStatus status)
{
    switch (status) {
    case Unknown:
        return QStringLiteral("Unknown");
    case NotInstalled:
        return QStringLiteral("NotInstalled");
    case PartiallyInstalled:
        return QStringLiteral("PartiallyInstalled");
    case InstalledAndUsable:
        return QStringLiteral("InstalledAndUsable");
    }
    return QStringLiteral("Invalid PackageStatus value.");
}

void TPackage::setLocalStatus(EStatus status)
{
    qDebug() << "Changing local package status from" << statusName(m_localStatus) << "to" << statusName(status);
    if (m_localStatus == status)
        return;
    m_localStatus = status;
    Q_EMIT localPackageStatusChanged();
}

TPackageBuild* TPackage::buildForCurrentPlatform() const
{
    if (buildCpuArchitecture() == ArchitectureI386 && currentCpuArchitecture() == ArchitectureX86_64) {
        // A 32-bit launcher on a 64-bit machine prefers the native build.
        for (const auto& build : m_builds) {
            if (currentOperatingSystem() == build->os && build->architectures == currentCpuArchitecture())
                return build.get();
        }
    }
    for (const auto& build : m_builds) {
        if ((buildCpuArchitecture() & ~build->architectures) == 0 && currentOperatingSystem() == build->os)
            return build.get();
    }
    return nullptr;
}

TPackageConfiguration* TPackage::configurationForCurrentPlatform() const
{
    if (TPackageBuild* build = buildForCurrentPlatform())
        return build->configuration.get();
    for (const auto& build : m_builds) {
        if (currentOperatingSystem() == build->os)
            return build->configuration.get();
    }
    return nullptr;
}

QString describeBuild(const TPackageBuild& build)
{
    QString os;
    switch (build.os) {
    case EOperatingSystem::Linux:
        os = QStringLiteral("Linux");
        break;
    case EOperatingSystem::MacOS:
        os = QStringLiteral("macOS");
        break;
    case EOperatingSystem::Windows:
        os = QStringLiteral("Windows");
        break;
    default:
        os = QStringLiteral("unknown");
    }
    QStringList architectures;
    if (build.architectures & ArchitectureI386)
        architectures << QStringLiteral("x86");
    if (build.architectures & ArchitectureX86_64)
        architectures << QStringLiteral("x86_64");
    if (build.architectures & ArchitectureArm64)
        architectures << QStringLiteral("arm64");
    const QString architecture = architectures.isEmpty() ? QStringLiteral("unknown") : architectures.join(QLatin1Char(';'));
    return QStringLiteral("\"%1\" (%2/%3)").arg(build.package->name(), os, architecture);
}

} // namespace launcher
