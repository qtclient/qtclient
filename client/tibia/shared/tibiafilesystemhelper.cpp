#include "shared/tibiafilesystemhelper.h"

#if defined(__EMSCRIPTEN__)
#    include "browser/opfs_file_engine.h"
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

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
    // The browser's tip lives on Module.qtclientFs, which only QFile/QDir see.
    return std::filesystem::is_directory(path, ec) || QDir(QString::fromStdString(path)).exists();
}

std::string readBytes(const std::string& path)
{
    const QString qpath = QString::fromStdString(path);
#if defined(__EMSCRIPTEN__)
    // Multi-MB tip assets (appearances.dat, …): one-shot qtclient-fs read. Chunked
    // QFile::readAll across JSPI suspends corrupted payloads on Chrome/Safari.
    if (qpath.startsWith(QLatin1String("/home/web_user"))
        || qpath.startsWith(QLatin1String("/mods"))
        || qpath.startsWith(QLatin1String("/data"))) {
        const QByteArray tipBytes = tibiaQtclientFsReadFile(qpath);
        if (!tipBytes.isEmpty())
            return std::string(tipBytes.constData(), static_cast<size_t>(tipBytes.size()));
    }
#endif
    QFile file(qpath);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("unable to open file '" + path + "'");
    const QByteArray bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

}

namespace shared {

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
    QDir().mkpath(QString::fromStdString(dir));
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
