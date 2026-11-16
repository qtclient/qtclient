#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>
#include <vector>

namespace launcher {

/// A delta patch of a file record: turns version `fromVersion` into `toVersion`.
struct TFilePatch {
    QUrl url;
    QString fromVersion;
    QString toVersion;
    qint64 size = 0;
};

/// One file record of package.json / assets.json.
struct TFileInPackage {
    QUrl url;
    QString localFile;
    qint64 packedSize = 0;
    QString packedHash;
    qint64 unpackedSize = 0;
    QString unpackedHash;
    bool unpack = false;
    bool executable = false;
    std::vector<TFilePatch> patches;
};

/// The package.json + assets.json pair of one build ("package configuration").
/// Versions are the contents of the build's versionurl / assetsversionurl.
class TPackageConfiguration
{
public:
    TPackageConfiguration(const QByteArray& packageVersion, const QByteArray& assetsVersion, const QUrl& packageUrl,
                          const QUrl& assetsUrl);

    /// Parses package.json (and assets.json when given). False when either is not JSON.
    bool load(const QByteArray& packageMetaData, const QByteArray& assetMetaData);

    /// "P<package version> A<assets version>"; "?" for an unknown part.
    QString combinedVersion() const;
    QString packageVersion() const;
    QString assetsVersion() const;
    int generation() const { return m_generation; }
    const QString& executable() const { return m_executable; }
    const QStringList& parameters() const { return m_parameters; }
    const std::vector<TFileInPackage>& files() const { return m_files; }

private:
    QUrl m_packageUrl;
    QUrl m_assetsUrl;
    QString m_packageVersion;
    QString m_assetsVersion;
    int m_generation = 0;
    QString m_executable;
    QStringList m_parameters;
    std::vector<TFileInPackage> m_files;
};

class TPackage;

enum class EOperatingSystem { Unknown = 0, Windows = 1, MacOS = 2, Linux = 3 };
/// Bit mask of supported CPU architectures.
enum EArchitecture : quint8 { ArchitectureI386 = 1, ArchitectureX86_64 = 2, ArchitectureArm64 = 4 };

EOperatingSystem currentOperatingSystem();
/// Architecture the launcher binary was built for.
quint8 buildCpuArchitecture();
/// Architecture of the machine (i386 / x86_64 only).
quint8 currentCpuArchitecture();

/// Host (qtclient): a non-CipSoft download channel of a build, published by
/// the release tip as `<name>url` / `<name>versionurl` (mods, minimap). Its
/// manifest has the assets.json shape; the files install into the package
/// directory together with the assets. Gated by launcher mod capabilities.
struct THostChannel {
    QString name;
    QUrl url;
    QUrl versionUrl;
};

/// One `builds[]` entry of a package.
struct TPackageBuild {
    TPackageBuild(TPackage* package, const QJsonObject& build, const QUrl& baseUrl);

    TPackage* package = nullptr;
    EOperatingSystem os = EOperatingSystem::Unknown;
    quint8 architectures = 0;
    QUrl url;
    QUrl versionUrl;
    QUrl assetsUrl;
    QUrl assetsVersionUrl;
    std::unique_ptr<TPackageConfiguration> configuration;
    /// Host channels the tip publishes for this build (not filtered by capability).
    std::vector<THostChannel> hostChannels;
    /// Host channels whose version is part of the configuration's assets version.
    QStringList activeHostChannels;
};

/// Host: assets version of a build with host channels — the published assets
/// version followed by ` <channel>=<version>` per channel, so a changed channel
/// makes the installed assets outdated the CipSoft way. Unchanged without channels.
QByteArray hostCompositeAssetsVersion(const QByteArray& assetsVersion,
                                      const std::vector<std::pair<QString, QByteArray>>& channelVersions);
/// Host: appends the files of host channel manifests to assets.json (urls made
/// absolute against each manifest's url). Returns `assetMetaData` unchanged when
/// there is nothing to merge.
QByteArray hostMergeChannelManifests(const QByteArray& assetMetaData,
                                     const std::vector<std::pair<QUrl, QByteArray>>& channelManifests);

/// Resolves `url` against `base` the way CipSoft's metadata does: absolute urls
/// stay, relative ones are appended to the base path with exactly one '/'.
QUrl resolveConfigurationUrl(const QUrl& base, const QUrl& url);

/// A package of tibiametadata.json (the launcher itself or a client).
class TPackage : public QObject
{
    Q_OBJECT
public:
    enum EVariant { VariantUnknown = 0, VariantRegular = 1, VariantPreview = 2, VariantBeta = 3, VariantTest = 4 };
    enum EStatus { Unknown = 0, NotInstalled = 1, PartiallyInstalled = 2, InstalledAndUsable = 3 };
    /// Type of the launcher's own package; clients are numbered by position.
    static constexpr int LauncherPackageType = 0x7fffffff;

    TPackage(int type, const QJsonObject& package, const QUrl& baseUrl);
    ~TPackage() override;

    int type() const { return m_type; }
    bool isLauncher() const { return m_type == LauncherPackageType; }
    const QString& name() const { return m_name; }
    EVariant variant() const { return m_variant; }
    const QString& directoryName() const { return m_directoryName; }
    const QUrl& logoLinkTarget() const { return m_logoLinkTarget; }
    bool installAutomatically() const { return m_installAutomatically; }
    bool isDefault() const { return m_isDefault; }
    void setDefault(bool isDefault) { m_isDefault = isDefault; }
    /// Set once an installation or update of this package finished in this session.
    bool isUpToDate() const { return m_upToDate; }
    void setUpToDate(bool upToDate) { m_upToDate = upToDate; }
    EStatus localStatus() const { return m_localStatus; }
    void setLocalStatus(EStatus status);
    const std::vector<std::unique_ptr<TPackageBuild>>& builds() const { return m_builds; }

    /// Build for this machine: exact os/arch match for a 32-bit launcher on a
    /// 64-bit machine, else the first build whose architectures cover the launcher's.
    TPackageBuild* buildForCurrentPlatform() const;
    /// Configuration of the current build, or of the first build for this OS.
    TPackageConfiguration* configurationForCurrentPlatform() const;

    static QString statusName(EStatus status);

Q_SIGNALS:
    void localPackageStatusChanged();

private:
    int m_type = 0;
    QString m_name;
    EVariant m_variant = VariantUnknown;
    QString m_directoryName;
    QUrl m_logoLinkTarget;
    bool m_installAutomatically = false;
    bool m_upToDate = false;
    bool m_isDefault = false;
    std::vector<std::unique_ptr<TPackageBuild>> m_builds;
    EStatus m_localStatus = Unknown;
};

/// Readable build label: `"<name>" (<os>/<arch;arch>)`.
QString describeBuild(const TPackageBuild& build);

} // namespace launcher
