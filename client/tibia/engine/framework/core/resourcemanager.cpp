#include "resourcemanager.h"

#include "filestream.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

std::string stripSlash(std::string path)
{
    stdext::replace_all(path, "\\", "/");
    while (!path.empty() && path.front() == '/')
        path.erase(path.begin());
    return path;
}

bool qExists(const QString& path, const bool directory)
{
    const QFileInfo info(path);
    return directory ? info.isDir() : info.isFile();
}

/// QFile sees both a desktop path and a browser MEMFS path. std::filesystem does not.
std::string locate(const ResourceManager& files, const std::string& virtualPath, const bool directory)
{
    const std::string relative = stripSlash(virtualPath);
    if (relative.empty())
        return {};
    const QString given = QString::fromStdString(virtualPath);
    if (given.startsWith(QLatin1Char('/')) && qExists(given, directory))
        return given.toStdString();
    std::vector<std::string> roots;
    if (!files.getWriteDir().empty())
        roots.push_back(files.getWriteDir());
    for (const auto& root : files.getSearchPaths())
        roots.push_back(root);
    if (!files.getWorkDir().empty())
        roots.push_back(files.getWorkDir());
    for (const auto& root : roots) {
        const QString candidate = QDir(QString::fromStdString(root)).filePath(QString::fromStdString(relative));
        if (qExists(candidate, directory))
            return candidate.toStdString();
    }
    return {};
}

std::string readBytes(const std::string& path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly))
        throw Exception("unable to open file '{}'", path);
    const QByteArray bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

}

ResourceManager& tibia::files::data()
{
    static ResourceManager files;
    return files;
}

void ResourceManager::init(const char* argv0)
{
    if (argv0)
        m_binaryPath = argv0;
}

void ResourceManager::terminate()
{
    m_searchPaths.clear();
}

bool ResourceManager::discoverWorkDir(const std::string& existentFile)
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

std::string ResourceManager::resolveUserWriteDir(const std::string& appWriteDirName)
{
    if (!m_userDirOverride.empty())
        return m_userDirOverride;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::string name = stripSlash(appWriteDirName);
    if (!name.empty() && name.back() != '/')
        name.push_back('/');
    return (QDir(base).filePath(QString::fromStdString(name))).toStdString();
}

bool ResourceManager::setupUserWriteDir(const std::string& appWriteDirName)
{
    const std::string dir = resolveUserWriteDir(appWriteDirName);
    return setWriteDir(dir, true);
}

bool ResourceManager::setWriteDir(const std::string& writeDir, const bool create)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (create)
        fs::create_directories(writeDir, ec);
    if (!fs::is_directory(writeDir, ec))
        return false;
    m_writeDir = writeDir;
    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), writeDir) == m_searchPaths.end())
        m_searchPaths.push_front(writeDir);
    return true;
}

bool ResourceManager::addSearchPath(const std::string& path, const bool pushFront)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(path, ec))
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

bool ResourceManager::removeSearchPath(const std::string& path)
{
    const auto it = std::find(m_searchPaths.begin(), m_searchPaths.end(), path);
    if (it == m_searchPaths.end())
        return false;
    m_searchPaths.erase(it);
    return true;
}

void ResourceManager::searchAndAddPackages(const std::string&, const std::string&) {}

bool ResourceManager::fileExists(const std::string& fileName)
{
    return !locate(*this, resolvePath(fileName), false).empty();
}

bool ResourceManager::fileExistsInWorkDir(const std::string& fileName)
{
    const QString path = QDir(QString::fromStdString(m_workDir)).filePath(QString::fromStdString(stripSlash(fileName)));
    return QFileInfo(path).isFile();
}

bool ResourceManager::directoryExists(const std::string& directoryName)
{
    return !locate(*this, resolvePath(directoryName), true).empty();
}

void ResourceManager::readFileStream(const std::string& fileName, std::iostream& out)
{
    const std::string buffer = readFileContents(fileName);
    if (buffer.empty()) {
        out.clear(std::ios::eofbit);
        return;
    }
    out.clear(std::ios::goodbit);
    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    out.seekg(0, std::ios::beg);
}

std::string ResourceManager::readFileContents(const std::string& fileName)
{
    const auto path = locate(*this, resolvePath(fileName), false);
    if (path.empty())
        throw Exception("unable to open file '{}'", fileName);
    return readBytes(path);
}

std::string ResourceManager::readFileContentsFromWorkDir(const std::string& fileName)
{
    return readBytes(QDir(QString::fromStdString(m_workDir)).filePath(QString::fromStdString(stripSlash(fileName))).toStdString());
}

void ResourceManager::prefetchDirectory(const std::string&) {}
void ResourceManager::clearPrefetchedFiles() {}

bool ResourceManager::writeFileBuffer(const std::string& fileName, const uint8_t* data, const uint32_t size, const bool createDirectory)
{
    namespace fs = std::filesystem;
    const fs::path dest = fs::path(m_writeDir) / stripSlash(fileName);
    std::error_code ec;
    if (createDirectory)
        fs::create_directories(dest.parent_path(), ec);
    QFile file(QString::fromStdString(dest.string()));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(reinterpret_cast<const char*>(data), static_cast<qint64>(size)) == static_cast<qint64>(size);
}

bool ResourceManager::writeFileContents(const std::string& fileName, const std::string& data)
{
    return writeFileBuffer(fileName, reinterpret_cast<const uint8_t*>(data.data()), static_cast<uint32_t>(data.size()));
}

bool ResourceManager::writeFileStream(const std::string& fileName, std::iostream& in)
{
    const std::streampos old = in.tellg();
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::string buffer(size, '\0');
    in.read(buffer.data(), static_cast<std::streamsize>(size));
    in.seekg(old);
    return writeFileContents(fileName, buffer);
}

FileStreamPtr ResourceManager::openFile(const std::string& fileName)
{
    const std::string bytes = readFileContents(fileName);
    return std::make_shared<FileStream>(fileName, std::string_view(bytes));
}

FileStreamPtr ResourceManager::appendFile(const std::string& fileName) const
{
    return createFile(fileName);
}

FileStreamPtr ResourceManager::createFile(const std::string& fileName) const
{
    namespace fs = std::filesystem;
    const fs::path dest = fs::path(m_writeDir.empty() ? m_workDir : m_writeDir) / stripSlash(fileName);
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    return std::make_shared<FileStream>(fileName, dest.string(), true);
}

bool ResourceManager::deleteFile(const std::string& fileName)
{
    const auto path = std::filesystem::path(locate(*this, fileName, false));
    if (path.empty())
        return false;
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

bool ResourceManager::makeDir(const std::string& directory)
{
    std::error_code ec;
    const auto root = m_writeDir.empty() ? m_workDir : m_writeDir;
    return std::filesystem::create_directories(std::filesystem::path(root) / stripSlash(directory), ec);
}

std::list<std::string> ResourceManager::listDirectoryFiles(const std::string& directoryPath, const bool fullPath, const bool, const bool recursive)
{
    std::list<std::string> names;
    const auto root = std::filesystem::path(locate(*this, directoryPath, true));
    if (root.empty())
        return names;
    std::error_code ec;
    const auto options = recursive ? std::filesystem::directory_options::skip_permission_denied
                                   : std::filesystem::directory_options::none;
    if (!recursive) {
        for (const auto& entry : std::filesystem::directory_iterator(root, options, ec)) {
            const auto name = entry.path().filename().generic_string();
            names.push_back(fullPath ? (stripSlash(directoryPath) + "/" + name) : name);
        }
        return names;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, options, ec)) {
        const auto name = std::filesystem::relative(entry.path(), root, ec).generic_string();
        names.push_back(fullPath ? (stripSlash(directoryPath) + "/" + name) : name);
    }
    return names;
}

std::vector<std::string> ResourceManager::getDirectoryFiles(const std::string& path, const bool filenameOnly, const bool recursive)
{
    const auto listed = listDirectoryFiles(path, !filenameOnly, false, recursive);
    return { listed.begin(), listed.end() };
}

std::string ResourceManager::resolvePath(const std::string& path)
{
    std::string full = path.starts_with('/') ? path : "/" + path;
    stdext::replace_all(full, "//", "/");
    return full;
}

std::string ResourceManager::getRealDir(const std::string& path)
{
    const bool asDir = directoryExists(path);
    const std::string found = locate(*this, resolvePath(path), asDir);
    if (found.empty())
        return {};
    const std::string relative = stripSlash(resolvePath(path));
    const auto slash = relative.empty() ? std::string::npos : found.rfind(relative);
    if (slash == std::string::npos || slash == 0)
        return std::filesystem::path(found).parent_path().generic_string();
    // Absolute virtual paths (/data/assets/…) locate as found="/"+relative.
    // Returning "/" makes callers that do root+path produce "//data/…", which
    // wasm MEMFS rejects — return empty so concat keeps a single leading slash.
    if (slash == 1 && found.front() == '/')
        return {};
    auto root = found.substr(0, slash);
    while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
        root.pop_back();
    return root;
}

std::string ResourceManager::getRealPath(const std::string& path)
{
    return locate(*this, resolvePath(path), directoryExists(path));
}

std::string ResourceManager::getBaseDir()
{
    return m_binaryPath.parent_path().string();
}

std::string ResourceManager::getUserDir()
{
    return m_writeDir.empty() ? resolveUserWriteDir("tibia/") : m_writeDir;
}

std::string ResourceManager::guessFilePath(const std::string& filename, const std::string& type)
{
    if (isFileType(filename, type))
        return filename;
    return filename + "." + type;
}

bool ResourceManager::isFileType(const std::string& filename, const std::string& type)
{
    return filename.ends_with(std::string(".") + type);
}

std::string ResourceManager::getFileName(const std::string& filePath)
{
    return std::filesystem::path(filePath).filename().string();
}

ticks_t ResourceManager::getFileTime(const std::string&)
{
    return 0;
}

std::string ResourceManager::encrypt(const std::string& data, const std::string&) { return data; }
std::string ResourceManager::decrypt(const std::string& data) { return data; }
uint8_t* ResourceManager::decrypt(uint8_t* data, int32_t) { return data; }
void ResourceManager::runEncryption(const std::string&) {}
void ResourceManager::save_string_into_file(const std::string& contents, const std::string& name)
{
    writeFileContents(name, contents);
}
std::string ResourceManager::fileChecksum(const std::string&) { return {}; }
std::string ResourceManager::fileSha256(const std::string&) { return {}; }
std::string ResourceManager::fileSha256InWorkDir(const std::string&) { return {}; }
bool ResourceManager::writeDownloadedFile(const std::string&, std::string, bool) { return false; }
bool ResourceManager::writeDownloadedFileToWorkDir(const std::string&, std::string, bool) { return false; }
bool ResourceManager::extractDownloadedArchive(const std::string&, std::string, const std::string&, bool) { return false; }
bool ResourceManager::extractDownloadedArchiveToWorkDir(const std::string&, std::string, const std::string&, bool) { return false; }
bool ResourceManager::extractDownloadedZip(const std::string&, std::string, const std::string&, bool) { return false; }
bool ResourceManager::beginExtractDownloadedArchive(const std::string&, std::string, const std::string&, bool) { return false; }
int ResourceManager::stepExtractDownloadedArchive(int) { return 0; }
void ResourceManager::abortExtractDownloadedArchive() {}
int ResourceManager::getExtractDownloadedArchiveProgress() { return 0; }
bool ResourceManager::writeFileContentsToWorkDir(const std::string& fileName, const std::string& data)
{
    return writeFileContents(fileName, data);
}
std::unordered_map<std::string, std::string> ResourceManager::filesChecksums() { return {}; }
std::string ResourceManager::selfChecksum() { return {}; }
void ResourceManager::updateFiles(const std::set<std::string>&) {}
void ResourceManager::updateExecutable(std::string) {}
bool ResourceManager::launchCorrect(const std::vector<std::string>&) { return false; }
std::string ResourceManager::createArchive(const std::unordered_map<std::string, std::string>&) { return {}; }
std::unordered_map<std::string, std::string> ResourceManager::decompressArchive(std::string) { return {}; }

std::vector<std::string> ResourceManager::discoverPath(const std::filesystem::path&, bool, bool)
{
    return {};
}
