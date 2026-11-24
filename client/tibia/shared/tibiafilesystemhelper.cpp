#include "shared/tibiafilesystemhelper.h"

#include "filesystemhelper.h"
#include "platform/platform.h"
#include "shared/sharedciptypes.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace tibia {

namespace {

std::string stripSlash(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (!path.empty() && path.front() == '/')
        path.erase(path.begin());
    return path;
}

bool qExists(const QString& path, const bool directory)
{
    const QFileInfo info(path);
    return directory ? info.isDir() : info.isFile();
}

bool isDirectory(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::string readBytes(const std::string& path)
{
    const QString qpath = QString::fromStdString(path);
    QFile file(qpath);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("unable to open file '" + path + "'");
    const QByteArray bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

}

namespace shared {

namespace {

QString directoryNameOf(const TTibiaFileSystemHelper::EDirectory directory)
{
    switch (directory) {
    case TTibiaFileSystemHelper::DIRECTORY_ASSETS: return QStringLiteral("assets");
    case TTibiaFileSystemHelper::DIRECTORY_MINIMAP: return QStringLiteral("minimap");
    case TTibiaFileSystemHelper::DIRECTORY_TEMPMINIMAP: return QStringLiteral("tempminimap");
    case TTibiaFileSystemHelper::DIRECTORY_CACHE: return QStringLiteral("cache");
    case TTibiaFileSystemHelper::DIRECTORY_CONF: return QStringLiteral("conf");
    case TTibiaFileSystemHelper::DIRECTORY_LOG: return QStringLiteral("log");
    case TTibiaFileSystemHelper::DIRECTORY_CRASHDUMP: return QStringLiteral("crashdump");
    case TTibiaFileSystemHelper::DIRECTORY_STOREIMAGES: return QStringLiteral("storeimages");
    case TTibiaFileSystemHelper::DIRECTORY_3RDPARTYLICENCES: return QStringLiteral("3rdpartylicences");
    case TTibiaFileSystemHelper::DIRECTORY_BATTLEYE: return QStringLiteral("BattlEye");
    case TTibiaFileSystemHelper::DIRECTORY_ANGLE_V1: return QStringLiteral("angle_v1");
    case TTibiaFileSystemHelper::DIRECTORY_ANGLE_CURRENT: return QStringLiteral("angle_current");
    case TTibiaFileSystemHelper::DIRECTORY_CHARACTERDATA: return QStringLiteral("characterdata");
    case TTibiaFileSystemHelper::DIRECTORY_SCREENSHOTS: return QStringLiteral("screenshots");
    case TTibiaFileSystemHelper::DIRECTORY_TUTORIAL: return QString();
    case TTibiaFileSystemHelper::DIRECTORY_SOUNDS: return QStringLiteral("sounds");
    }
    return QString();
}

const QString kConfigIni = QStringLiteral("config.ini");
const QString kClientOptionsJson = QStringLiteral("clientoptions.json");
const QString kMinimapMarkersBin = QStringLiteral("minimapmarkers.bin");
const QString kCompendiumJson = QStringLiteral("compendium.json");
const QString kOnlineNumbersJson = QStringLiteral("onlinenumbers.json");
const QString kBoostedCreatureJson = QStringLiteral("boostedcreature.json");
const QString kSystemSurveyTxt = QStringLiteral("systemsurvey.txt");
const QString kEventScheduleJson = QStringLiteral("eventschedule.json");
const QString kAssetsJsonSha256 = QStringLiteral("assets.json.sha256");
const QString kLauncherExecutableName = QStringLiteral("Tibia");
const QString kClientExecutableName = QStringLiteral("client");
const QString kForcePatchMarkerName = QStringLiteral("FORCE_PATCH");
constexpr const char* kImageFormat = "PNG";

/// <app>/../Resources (macOS bundle resources).
QDir resourcesDirectory()
{
    QDir directory(QCoreApplication::applicationDirPath());
    directory.cdUp();
    directory.cd(QStringLiteral("./Resources"));
    return directory;
}

void removeMatchingFiles(QDir directory, const QStringList& nameFilters)
{
    directory.setNameFilters(nameFilters);
    directory.setFilter(QDir::Files);
    const QStringList entries = directory.entryList();
    for (const QString& entry : entries)
        directory.remove(entry);
}

}

TTibiaFileSystemHelper::TTibiaFileSystemHelper()
    : m_dataDirectory(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_baseDirectory(m_dataDirectory)
    , m_fileSystemHelper(std::make_shared<TFileSystemHelper>())
{
}

TTibiaFileSystemHelper::~TTibiaFileSystemHelper() = default;

QString TTibiaFileSystemHelper::getDirectoryName(const EDirectory directory) const
{
    return directoryNameOf(directory);
}

QString TTibiaFileSystemHelper::getBaseDirectoryPath(const EDirectory directory) const
{
    switch (directory) {
    case DIRECTORY_CONF:
    case DIRECTORY_CHARACTERDATA:
        return m_baseDirectory.path();
    case DIRECTORY_BATTLEYE:
    case DIRECTORY_ANGLE_V1:
    case DIRECTORY_ANGLE_CURRENT:
        return QCoreApplication::applicationDirPath();
    case DIRECTORY_TUTORIAL:
        return QStringLiteral(":/tutorial");
    case DIRECTORY_ASSETS:
    case DIRECTORY_MINIMAP:
    case DIRECTORY_TEMPMINIMAP:
    case DIRECTORY_CACHE:
    case DIRECTORY_LOG:
    case DIRECTORY_CRASHDUMP:
    case DIRECTORY_STOREIMAGES:
    case DIRECTORY_3RDPARTYLICENCES:
    case DIRECTORY_SCREENSHOTS:
    case DIRECTORY_SOUNDS:
        return m_dataDirectory.path();
    }
    return QString();
}

QDir TTibiaFileSystemHelper::getDirectory(const EDirectory directory) const
{
    return m_fileSystemHelper->createDirectory(getDirectoryPath(directory));
}

QString TTibiaFileSystemHelper::getDirectoryPath(const EDirectory directory) const
{
    const QString basePath = getBaseDirectoryPath(directory);
    const QString name = getDirectoryName(directory);
    if (name.isEmpty())
        return basePath;
    return m_fileSystemHelper->joinPaths({ basePath, name });
}

QString TTibiaFileSystemHelper::getFilePath(const EDirectory directory, const QString& fileName) const
{
    return m_fileSystemHelper->joinPaths({ getDirectoryPath(directory), fileName });
}

QString TTibiaFileSystemHelper::getFilePathInDirectory(const EDirectory directory, const QString& fileName) const
{
    const QString path = getDirectory(directory).path();
    return m_fileSystemHelper->joinPaths({ path, fileName });
}

QDir TTibiaFileSystemHelper::getAssetsDirectory() const { return getDirectory(DIRECTORY_ASSETS); }
QDir TTibiaFileSystemHelper::getSoundsDirectory() const { return getDirectory(DIRECTORY_SOUNDS); }
QDir TTibiaFileSystemHelper::getLogDirectory() const { return getDirectory(DIRECTORY_LOG); }
QDir TTibiaFileSystemHelper::getMinimapDirectory() const { return getDirectory(DIRECTORY_MINIMAP); }
QDir TTibiaFileSystemHelper::getCacheDirectory() const { return getDirectory(DIRECTORY_CACHE); }
QDir TTibiaFileSystemHelper::getScreenshotsDirectory() const { return getDirectory(DIRECTORY_SCREENSHOTS); }
QDir TTibiaFileSystemHelper::getCrashDumpDirectory() const { return getDirectory(DIRECTORY_CRASHDUMP); }
QDir TTibiaFileSystemHelper::getThirdPartyLicencesDirectory() const { return getDirectory(DIRECTORY_3RDPARTYLICENCES); }
QDir TTibiaFileSystemHelper::getStoreImagesDirectory() const { return getDirectory(DIRECTORY_STOREIMAGES); }

QDir TTibiaFileSystemHelper::getCharacterDataDirectory(const QString& characterName) const
{
    return m_fileSystemHelper->createDirectory(getFilePathInDirectory(DIRECTORY_CHARACTERDATA, characterName));
}

void TTibiaFileSystemHelper::removeCrashDumps() const
{
    removeMatchingFiles(getCrashDumpDirectory(),
                        { QStringLiteral("*.dmp"), QStringLiteral("*.dmp.json"), QStringLiteral("*.dmp.session") });
}

QString TTibiaFileSystemHelper::getLatestCrashDumpFilePath() const
{
    QDir directory = getDirectory(DIRECTORY_CRASHDUMP);
    directory.setNameFilters({ QStringLiteral("*.dmp") });
    directory.setFilter(QDir::Files);
    directory.setSorting(QDir::Time | QDir::DirsFirst);
    if (directory.entryList().isEmpty())
        return QString();
    return m_fileSystemHelper->joinPaths({ directory.absolutePath(), directory.entryList().first() });
}

QString TTibiaFileSystemHelper::getCrashDumpMetaDataFileName(const QString& crashDumpFilePath)
{
    return QFileInfo(crashDumpFilePath).fileName().append(QStringLiteral(".json"));
}

QString TTibiaFileSystemHelper::getCrashDumpSessionFileName(const QString& crashDumpFilePath)
{
    return QFileInfo(crashDumpFilePath).fileName().append(QStringLiteral(".session"));
}

QByteArray TTibiaFileSystemHelper::readFileIfExists(const QString& filePath) const
{
    return m_fileSystemHelper->readFileIfExists(filePath);
}

QByteArray TTibiaFileSystemHelper::readCrashDumpMetaData(const QString& crashDumpFilePath) const
{
    const QString path = getFilePathInDirectory(DIRECTORY_CRASHDUMP, getCrashDumpMetaDataFileName(crashDumpFilePath));
    return m_fileSystemHelper->readFileIfExists(path);
}

QString TTibiaFileSystemHelper::getFilePathInDirectory(const QDir& directory, const QString& fileName) const
{
    m_fileSystemHelper->createDirectory(directory.path());
    return m_fileSystemHelper->joinPaths({ directory.path(), fileName });
}

QString TTibiaFileSystemHelper::getConfigFilePath(const QString& fileName) const
{
    const QDir directory(getDirectoryPath(DIRECTORY_CONF));
    return getFilePathInDirectory(directory, fileName);
}

QString TTibiaFileSystemHelper::getLogFilePath(const QString& fileName) const
{
    return getFilePathInDirectory(DIRECTORY_LOG, fileName);
}

std::shared_ptr<QSettings> TTibiaFileSystemHelper::openConfigSettings() const
{
    const QString path = getConfigFilePath(kConfigIni);
    if (m_fileSystemHelper->fileExists(path))
        return m_fileSystemHelper->openSettings(path);
    return m_fileSystemHelper->openSettings(QStringLiteral(":/conf/") + kConfigIni);
}

QJsonDocument TTibiaFileSystemHelper::readJsonFile(const QString& filePath) const
{
    bool error = false;
    QJsonDocument document = m_fileSystemHelper->readJsonFileAsQJsonDocument(filePath, error);
    if (!error)
        return document;
    throw std::runtime_error(
        QStringLiteral("Unable to load '%1', file might be corrupted").arg(filePath).toStdString());
}

QJsonDocument TTibiaFileSystemHelper::readClientOptions() const
{
    return readJsonFile(getConfigFilePath(kClientOptionsJson));
}

QJsonDocument TTibiaFileSystemHelper::readDefaultClientOptions() const
{
    return readJsonFile(getFilePath(DIRECTORY_TUTORIAL, kClientOptionsJson));
}

void TTibiaFileSystemHelper::writeJsonFile(const QString& filePath, const QJsonDocument& document) const
{
    m_fileSystemHelper->writeFile(filePath, document.toJson(QJsonDocument::Indented));
}

void TTibiaFileSystemHelper::writeClientOptions(const QJsonDocument& document) const
{
    writeJsonFile(getConfigFilePath(kClientOptionsJson), document);
}

std::shared_ptr<QImage> TTibiaFileSystemHelper::readMinimapColorImage(const TCoordinate& coordinate) const
{
    return m_fileSystemHelper->readImage(getMinimapColorFilePath(coordinate), kImageFormat);
}

std::shared_ptr<QImage> TTibiaFileSystemHelper::readMinimapWaypointCostImage(const TCoordinate& coordinate) const
{
    return m_fileSystemHelper->readImage(getMinimapWaypointCostFilePath(coordinate), kImageFormat);
}

void TTibiaFileSystemHelper::writeMinimapColorImage(const TCoordinate& coordinate, const QImage& image) const
{
    m_fileSystemHelper->writeImage(image, getMinimapColorFilePath(coordinate), kImageFormat);
}

void TTibiaFileSystemHelper::writeMinimapWaypointCostImage(const TCoordinate& coordinate, const QImage& image) const
{
    m_fileSystemHelper->writeImage(image, getMinimapWaypointCostFilePath(coordinate), kImageFormat);
}

QByteArray TTibiaFileSystemHelper::readMinimapMarkers() const
{
    return m_fileSystemHelper->readFileIfExists(getMinimapMarkersFilePath());
}

void TTibiaFileSystemHelper::writeMinimapMarkers(const QByteArray& data) const
{
    m_fileSystemHelper->writeFile(getMinimapMarkersFilePath(), data);
}

void TTibiaFileSystemHelper::appendToLogFile(const QString& logName, const QString& text) const
{
    m_fileSystemHelper->appendTextFile(getLogFilePathForName(logName), text);
}

QJsonDocument TTibiaFileSystemHelper::readCompendium() const { return readJsonFileOrEmptyObject(getCompendiumFilePath()); }
void TTibiaFileSystemHelper::writeCompendium(const QJsonDocument& document) const { writeJsonFile(getCompendiumFilePath(), document); }
QJsonDocument TTibiaFileSystemHelper::readEventSchedule() const { return readJsonFileOrEmptyObject(getEventScheduleFilePath()); }
void TTibiaFileSystemHelper::writeEventSchedule(const QJsonDocument& document) const { writeJsonFile(getEventScheduleFilePath(), document); }
QJsonDocument TTibiaFileSystemHelper::readOnlineNumbers() const { return readJsonFileOrEmptyObject(getOnlineNumbersFilePath()); }
void TTibiaFileSystemHelper::writeOnlineNumbers(const QJsonDocument& document) const { writeJsonFile(getOnlineNumbersFilePath(), document); }
QJsonDocument TTibiaFileSystemHelper::readBoostedCreature() const { return readJsonFileOrEmptyObject(getBoostedCreatureFilePath()); }
void TTibiaFileSystemHelper::writeBoostedCreature(const QJsonDocument& document) const { writeJsonFile(getBoostedCreatureFilePath(), document); }

void TTibiaFileSystemHelper::writeTextFile(const QString& text, const QString& filePath) const
{
    m_fileSystemHelper->writeTextFile(filePath, text);
}

QJsonDocument TTibiaFileSystemHelper::readTutorialCatalog() const
{
    return readJsonFileOrEmptyObject(QStringLiteral(":/tutorial/tutorial-catalog.json"));
}

QJsonDocument TTibiaFileSystemHelper::readSoundCatalog() const
{
    return readJsonFileOrEmptyObject(getFilePathInDirectory(DIRECTORY_ASSETS, QStringLiteral("catalog-sound.json")));
}

void TTibiaFileSystemHelper::writeSystemSurvey(const QString& text) const
{
    const QString path = m_fileSystemHelper->joinPaths({ getLogDirectory().absolutePath(), kSystemSurveyTxt });
    m_fileSystemHelper->writeTextFile(path, text);
}

QString TTibiaFileSystemHelper::getAllLicenseTexts() const
{
    QStringList texts;
    m_fileSystemHelper->forEachFileInDirectory(getThirdPartyLicencesDirectory().absolutePath(),
                                               [this, &texts](const TLocalFileInfo& file) {
                                                   const QByteArray data = m_fileSystemHelper->readFileIfExists(file.getFilePath());
                                                   texts.append(QString::fromUtf8(data));
                                               });
    return texts.join(QStringLiteral("\n\n"));
}

QString TTibiaFileSystemHelper::getApplicationDirectoryPath()
{
    return QCoreApplication::applicationDirPath();
}

QString TTibiaFileSystemHelper::getTranslationFileName()
{
    return QStringLiteral("client.en");
}

std::shared_ptr<TFileSystemHelper> TTibiaFileSystemHelper::getFileSystemHelper() const
{
    return m_fileSystemHelper;
}

void TTibiaFileSystemHelper::writeScreenshot(const QString& fileName, const QImage& image) const
{
    m_fileSystemHelper->writeImage(image, getScreenshotFilePath(fileName), kImageFormat);
}

void TTibiaFileSystemHelper::openDirectoryInFileBrowser(const QDir& directory)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory.path()));
}

void TTibiaFileSystemHelper::clearTemporaryMinimap() const
{
    removeMatchingFiles(getDirectory(DIRECTORY_TEMPMINIMAP), { QStringLiteral("*.png"), QStringLiteral("*.bin") });
}

void TTibiaFileSystemHelper::setBaseDirectory(const QDir& directory)
{
    m_baseDirectory = directory == QDir(QString()) ? m_dataDirectory : directory;
}

QString TTibiaFileSystemHelper::readAssetsJsonSha256() const
{
    QDir directory = resourcesDirectory();
    directory.cdUp();
    directory.cdUp();
    const QFile file(directory.absoluteFilePath(kAssetsJsonSha256));
    const QByteArray data = m_fileSystemHelper->readFile(file.fileName());
    return QString::fromUtf8(data).trimmed();
}

void TTibiaFileSystemHelper::validateBaseDirectory() const
{
    const QString path = m_baseDirectory.path();
    if (m_fileSystemHelper && !m_fileSystemHelper->directoryExists(path))
        throw std::invalid_argument(
            QStringLiteral("base directory '%1' does not exist").arg(path).toLocal8Bit().toStdString());
}

QString TTibiaFileSystemHelper::getMinimapColorFilePath(const TCoordinate& coordinate) const
{
    const QString name = QStringLiteral("Minimap_Color_%1_%2_%3.png").arg(coordinate.x).arg(coordinate.y).arg(coordinate.z);
    return getFilePathInDirectory(DIRECTORY_MINIMAP, name);
}

QString TTibiaFileSystemHelper::getMinimapWaypointCostFilePath(const TCoordinate& coordinate) const
{
    const QString name =
        QStringLiteral("Minimap_WaypointCost_%1_%2_%3.png").arg(coordinate.x).arg(coordinate.y).arg(coordinate.z);
    return getFilePathInDirectory(DIRECTORY_MINIMAP, name);
}

QString TTibiaFileSystemHelper::getMinimapMarkersFilePath() const
{
    return getFilePathInDirectory(DIRECTORY_MINIMAP, kMinimapMarkersBin);
}

QString TTibiaFileSystemHelper::getLogFilePathForName(const QString& logName) const
{
    return getFilePathInDirectory(DIRECTORY_LOG, QStringLiteral("%1.txt").arg(logName));
}

QString TTibiaFileSystemHelper::getCompendiumFilePath() const { return getFilePathInDirectory(DIRECTORY_CACHE, kCompendiumJson); }

QString TTibiaFileSystemHelper::getScreenshotFilePath(const QString& fileName) const
{
    return getFilePathInDirectory(DIRECTORY_SCREENSHOTS, fileName);
}

QString TTibiaFileSystemHelper::getEventScheduleFilePath() const { return getFilePathInDirectory(DIRECTORY_CACHE, kEventScheduleJson); }
QString TTibiaFileSystemHelper::getOnlineNumbersFilePath() const { return getFilePathInDirectory(DIRECTORY_CACHE, kOnlineNumbersJson); }
QString TTibiaFileSystemHelper::getBoostedCreatureFilePath() const { return getFilePathInDirectory(DIRECTORY_CACHE, kBoostedCreatureJson); }

QJsonDocument TTibiaFileSystemHelper::readJsonFileOrEmptyObject(const QString& filePath) const
{
    bool error = false;
    QJsonDocument document = m_fileSystemHelper->readJsonFileAsQJsonDocument(filePath, error);
    if (!error)
        return document;
    return QJsonDocument::fromJson(QByteArrayLiteral("{}"));
}

QDir TTibiaFileSystemHelper::getPackageRootDirectory()
{
    // Walk up from the application directory until the launcher executable is
    // a file there; a default QDir when the file system root is reached.
    QDir directory(QCoreApplication::applicationDirPath());
    while (!QFileInfo(directory.absoluteFilePath(kLauncherExecutableName)).isFile()) {
        if (!directory.cdUp())
            return QDir(QString());
    }
    return directory;
}

QDir TTibiaFileSystemHelper::getLauncherDirectory()
{
    const QDir packageRoot = getPackageRootDirectory();
    return QDir(QFileInfo(packageRoot.absoluteFilePath(kLauncherExecutableName)).canonicalPath());
}

bool TTibiaFileSystemHelper::startLauncherDetached(const QStringList& arguments)
{
    const QDir launcherDirectory = getLauncherDirectory();
    const QString program = launcherDirectory.absoluteFilePath(kLauncherExecutableName);
    QDir pluginDirectory(launcherDirectory);
    pluginDirectory.cd(QStringLiteral("../PlugIns"));
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", pluginDirectory.canonicalPath().toLocal8Bit());
    return tibia::platform::startDetached(program, arguments, {}, launcherDirectory.absolutePath());
}

bool TTibiaFileSystemHelper::startLauncher(const ELauncherStartReason reason, const QString& patchPath)
{
    QStringList arguments;
    switch (reason) {
    case LAUNCHER_START_FIRST_LAUNCH:
        arguments.append(QStringLiteral("--first-launch"));
        break;
    case LAUNCHER_START_SKIP_SELFUPDATE:
        arguments.append(QStringLiteral("--skip-selfupdate"));
        break;
    case LAUNCHER_START_SHOW_UI:
        arguments.append(QStringLiteral("--show-ui"));
        break;
    case LAUNCHER_START_PATCH:
        arguments.append(QStringLiteral("--patch"));
        arguments.append(patchPath);
        break;
    default:
        break;
    }
    return startLauncherDetached(arguments);
}

bool TTibiaFileSystemHelper::startLauncherToPatchClient()
{
    const QString clientPath = QCoreApplication::applicationDirPath() + QLatin1Char('/') + kClientExecutableName;
    return startLauncher(LAUNCHER_START_PATCH, QDir::toNativeSeparators(clientPath));
}

void TTibiaFileSystemHelper::createForcePatchMarker()
{
    QDir directory = resourcesDirectory();
    directory.cdUp();
    directory.cdUp();
    QFile file(directory.absoluteFilePath(kForcePatchMarkerName));
    (void)file.open(QIODevice::WriteOnly);
    file.close();
}

bool TTibiaFileSystemHelper::hasForcePatchMarker()
{
    QDir directory = resourcesDirectory();
    directory.cdUp();
    directory.cdUp();
    const QFile file(directory.absoluteFilePath(kForcePatchMarkerName));
    return file.exists();
}

void TTibiaFileSystemHelper::init(const char* argv0)
{
    if (argv0)
        m_binaryPath = argv0;
}

bool TTibiaFileSystemHelper::discoverWorkDir(const std::string& existentFile)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        std::error_code ec;
        if (fs::is_regular_file(dir / existentFile, ec)) {
            m_workDir = dir.string();
            if (std::find(m_searchPaths.begin(), m_searchPaths.end(), m_workDir) == m_searchPaths.end())
                m_searchPaths.push_back(m_workDir);
            return true;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir)
            break;
        dir = dir.parent_path();
    }
    return false;
}

bool TTibiaFileSystemHelper::setupUserWriteDir(const std::string& appWriteDirName)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::string name = stripSlash(appWriteDirName);
    if (!name.empty() && name.back() != '/')
        name.push_back('/');
    const std::string dir = QDir(base).filePath(QString::fromStdString(name)).toStdString();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return setWriteDir(dir);
}

bool TTibiaFileSystemHelper::setWriteDir(const std::string& writeDir)
{
    if (!isDirectory(writeDir))
        return false;
    m_writeDir = writeDir;
    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), writeDir) == m_searchPaths.end())
        m_searchPaths.push_front(writeDir);
    return true;
}

bool TTibiaFileSystemHelper::addSearchPath(const std::string& path, const bool pushFront)
{
    if (!isDirectory(path))
        return false;
    if (m_workDir.empty())
        m_workDir = path;
    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), path) != m_searchPaths.end())
        return true;
    if (pushFront)
        m_searchPaths.push_front(path);
    else
        m_searchPaths.push_back(path);
    return true;
}

std::string TTibiaFileSystemHelper::locate(const std::string& virtualPath, const bool directory) const
{
    const std::string relative = stripSlash(virtualPath);
    if (relative.empty())
        return {};
    const QString given = QString::fromStdString(virtualPath);
    if (given.startsWith(QLatin1Char('/')) && qExists(given, directory))
        return given.toStdString();
    std::vector<std::string> roots;
    if (!m_writeDir.empty())
        roots.push_back(m_writeDir);
    for (const auto& root : m_searchPaths)
        roots.push_back(root);
    if (!m_workDir.empty())
        roots.push_back(m_workDir);
    for (const auto& root : roots) {
        const QString candidate = QDir(QString::fromStdString(root)).filePath(QString::fromStdString(relative));
        if (qExists(candidate, directory))
            return candidate.toStdString();
    }
    return {};
}

bool TTibiaFileSystemHelper::fileExists(const std::string& fileName) const
{
    return !locate(resolvePath(fileName), false).empty();
}

bool TTibiaFileSystemHelper::directoryExists(const std::string& directoryName) const
{
    return !locate(resolvePath(directoryName), true).empty();
}

std::string TTibiaFileSystemHelper::readFileContents(const std::string& fileName) const
{
    const auto path = locate(resolvePath(fileName), false);
    if (path.empty())
        throw std::runtime_error("unable to open file '" + fileName + "'");
    return readBytes(path);
}

std::string TTibiaFileSystemHelper::resolvePath(const std::string& path)
{
    std::string full = path.starts_with('/') ? path : "/" + path;
    for (size_t at = full.find("//"); at != std::string::npos; at = full.find("//", at))
        full.erase(at, 1);
    return full;
}

std::string TTibiaFileSystemHelper::getRealPath(const std::string& path) const
{
    return locate(resolvePath(path), directoryExists(path));
}

std::string TTibiaFileSystemHelper::guessFilePath(const std::string& filename, const std::string& type)
{
    if (filename.ends_with(std::string(".") + type))
        return filename;
    return filename + "." + type;
}

}

namespace files {

shared::TTibiaFileSystemHelper& data()
{
    static shared::TTibiaFileSystemHelper files;
    return files;
}

}
}
