#pragma once

#include <deque>
#include <string>

namespace tibia {
namespace shared {

/// CIP `shared::TTibiaFileSystemHelper`: where the client's data files live.
/// Paths are virtual ("/data/assets/…"); they resolve against the user write
/// directory, then the search paths, then the work directory. Desktop paths
/// and the browser's file system both go through Qt.
class TTibiaFileSystemHelper final
{
public:
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
