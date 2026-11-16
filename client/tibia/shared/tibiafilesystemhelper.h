#pragma once

#include <QDir>
#include <QJsonDocument>
#include <QString>

#include <deque>
#include <memory>
#include <string>

class QImage;
class QSettings;

namespace tibia {
namespace shared {

class TCoordinate;
class TFileSystemHelper;

/// CIP `shared::TTibiaFileSystemHelper`: where the client's data files live.
/// Paths are virtual ("/data/assets/…"); they resolve against the user write
/// directory, then the search paths, then the work directory. Desktop paths
/// and the browser's file system both go through Qt.
class TTibiaFileSystemHelper final
{
public:
    /// CIP directory kinds (directory name table in the binary).
    enum EDirectory {
        DIRECTORY_ASSETS = 0,
        DIRECTORY_MINIMAP = 1,
        DIRECTORY_TEMPMINIMAP = 2,
        DIRECTORY_CACHE = 3,
        DIRECTORY_CONF = 4,
        DIRECTORY_LOG = 5,
        DIRECTORY_CRASHDUMP = 6,
        DIRECTORY_STOREIMAGES = 7,
        DIRECTORY_3RDPARTYLICENCES = 8,
        DIRECTORY_BATTLEYE = 9,
        DIRECTORY_ANGLE_V1 = 10,
        DIRECTORY_ANGLE_CURRENT = 11,
        DIRECTORY_CHARACTERDATA = 12,
        DIRECTORY_SCREENSHOTS = 13,
        DIRECTORY_TUTORIAL = 14,
        DIRECTORY_SOUNDS = 15,
    };

    TTibiaFileSystemHelper();
    ~TTibiaFileSystemHelper();

    // CIP path service (vtable order).
    QString getDirectoryName(EDirectory directory) const;
    QString getBaseDirectoryPath(EDirectory directory) const;
    QDir getDirectory(EDirectory directory) const;
    QString getDirectoryPath(EDirectory directory) const;
    QString getFilePath(EDirectory directory, const QString& fileName) const;
    /// Like getFilePath, creating the directory first.
    QString getFilePathInDirectory(EDirectory directory, const QString& fileName) const;
    QDir getAssetsDirectory() const;
    QDir getSoundsDirectory() const;
    QDir getLogDirectory() const;
    QDir getMinimapDirectory() const;
    QDir getCacheDirectory() const;
    QDir getScreenshotsDirectory() const;
    QDir getCrashDumpDirectory() const;
    QDir getThirdPartyLicencesDirectory() const;
    QDir getStoreImagesDirectory() const;
    QDir getCharacterDataDirectory(const QString& characterName) const;
    void removeCrashDumps() const;
    QString getLatestCrashDumpFilePath() const;
    static QString getCrashDumpMetaDataFileName(const QString& crashDumpFilePath);
    static QString getCrashDumpSessionFileName(const QString& crashDumpFilePath);
    QByteArray readFileIfExists(const QString& filePath) const;
    QByteArray readCrashDumpMetaData(const QString& crashDumpFilePath) const;
    QString getFilePathInDirectory(const QDir& directory, const QString& fileName) const;
    QString getConfigFilePath(const QString& fileName) const;
    QString getLogFilePath(const QString& fileName) const;
    std::shared_ptr<QSettings> openConfigSettings() const;
    /// Throws std::runtime_error when the file is corrupted.
    QJsonDocument readJsonFile(const QString& filePath) const;
    QJsonDocument readClientOptions() const;
    QJsonDocument readDefaultClientOptions() const;
    void writeJsonFile(const QString& filePath, const QJsonDocument& document) const;
    void writeClientOptions(const QJsonDocument& document) const;
    std::shared_ptr<QImage> readMinimapColorImage(const TCoordinate& coordinate) const;
    std::shared_ptr<QImage> readMinimapWaypointCostImage(const TCoordinate& coordinate) const;
    void writeMinimapColorImage(const TCoordinate& coordinate, const QImage& image) const;
    void writeMinimapWaypointCostImage(const TCoordinate& coordinate, const QImage& image) const;
    QByteArray readMinimapMarkers() const;
    void writeMinimapMarkers(const QByteArray& data) const;
    void appendToLogFile(const QString& logName, const QString& text) const;
    QJsonDocument readCompendium() const;
    void writeCompendium(const QJsonDocument& document) const;
    QJsonDocument readEventSchedule() const;
    void writeEventSchedule(const QJsonDocument& document) const;
    QJsonDocument readOnlineNumbers() const;
    void writeOnlineNumbers(const QJsonDocument& document) const;
    QJsonDocument readBoostedCreature() const;
    void writeBoostedCreature(const QJsonDocument& document) const;
    void writeTextFile(const QString& text, const QString& filePath) const;
    QJsonDocument readTutorialCatalog() const;
    QJsonDocument readSoundCatalog() const;
    void writeSystemSurvey(const QString& text) const;
    QString getAllLicenseTexts() const;
    static QString getApplicationDirectoryPath();
    static QString getTranslationFileName();
    std::shared_ptr<TFileSystemHelper> getFileSystemHelper() const;
    void writeScreenshot(const QString& fileName, const QImage& image) const;
    static void openDirectoryInFileBrowser(const QDir& directory);
    void clearTemporaryMinimap() const;
    void setBaseDirectory(const QDir& directory);
    QString readAssetsJsonSha256() const;
    /// Throws std::invalid_argument when the base directory is missing.
    void validateBaseDirectory() const;
    QString getMinimapColorFilePath(const TCoordinate& coordinate) const;
    QString getMinimapWaypointCostFilePath(const TCoordinate& coordinate) const;
    QString getMinimapMarkersFilePath() const;
    QString getLogFilePathForName(const QString& logName) const;
    QString getCompendiumFilePath() const;
    QString getScreenshotFilePath(const QString& fileName) const;
    QString getEventScheduleFilePath() const;
    QString getOnlineNumbersFilePath() const;
    QString getBoostedCreatureFilePath() const;
    /// The document, or `{}` when the file is corrupted.
    QJsonDocument readJsonFileOrEmptyObject(const QString& filePath) const;

    // qtclient resource lookup (virtual data paths).
    void init(const char* argv0);
    /// Finds the directory (cwd or a parent) that holds `existentFile` and makes it the work dir.
    bool discoverWorkDir(const std::string& existentFile);
    /// Uses (and creates) the per-user app data directory `appWriteDirName` as the write dir.
    bool setupUserWriteDir(const std::string& appWriteDirName);
    bool addSearchPath(const std::string& path, bool pushFront = false);

    bool fileExists(const std::string& fileName) const;
    bool directoryExists(const std::string& directoryName) const;
    /// The whole file; throws std::runtime_error when it does not exist.
    std::string readFileContents(const std::string& fileName) const;

    /// "a/b" → "/a/b" (one leading slash, no doubled slashes).
    static std::string resolvePath(const std::string& path);
    /// The real path a virtual path resolves to, empty when not found.
    std::string getRealPath(const std::string& path) const;
    /// `filename` with ".type" appended unless it already ends with it.
    static std::string guessFilePath(const std::string& filename, const std::string& type);

    std::string getWorkDir() const { return m_workDir; }
    std::string getWriteDir() const { return m_writeDir; }
    const std::deque<std::string>& getSearchPaths() const { return m_searchPaths; }

private:
    std::string locate(const std::string& virtualPath, bool directory) const;
    bool setWriteDir(const std::string& writeDir);

    // CIP layout: data directory (+0x8), base directory (+0x10), file system helper (+0x18).
    QDir m_dataDirectory;
    QDir m_baseDirectory;
    std::shared_ptr<TFileSystemHelper> m_fileSystemHelper;

    std::string m_binaryPath;
    std::string m_workDir;
    std::string m_writeDir;
    std::deque<std::string> m_searchPaths;
};

}

namespace files {
/// The client's data file system.
shared::TTibiaFileSystemHelper& data();
}
}
