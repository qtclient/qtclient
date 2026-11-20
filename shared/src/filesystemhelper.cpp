#include "filesystemhelper.h"

#include "lzmacompressedreadonlymemoryblob.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonParseError>
#include <QTextStream>

#include <stdexcept>

namespace tibia::shared {

void throwFileError(const QFile& file, const QString& error)
{
    throw std::runtime_error(
        QStringLiteral("I/O error on file '%1': %2").arg(file.fileName(), error).toStdString());
}

std::shared_ptr<TReadOnlyMemoryBlob> TFileSystemHelper::openReadOnlyMemoryBlob(const QString& filePath) const
{
    static const QString LzmaSuffix = QStringLiteral(".lzma");
    if (fileExists(filePath)) {
        if (filePath.endsWith(LzmaSuffix))
            return std::make_shared<TLZMACompressedReadOnlyMemoryBlob>(filePath);
        return std::make_shared<TReadOnlyMemoryBlob>(filePath);
    }
    if (!fileExists(filePath + LzmaSuffix))
        return std::make_shared<TReadOnlyMemoryBlob>(filePath);
    return std::make_shared<TLZMACompressedReadOnlyMemoryBlob>(filePath + LzmaSuffix);
}

QByteArray TFileSystemHelper::readFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly) && file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    file.close();
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    return data;
}

QByteArray TFileSystemHelper::readFileIfExists(const QString& filePath) const
{
    QFile file(filePath);
    QByteArray data;
    if (!file.open(QIODevice::ReadOnly)) {
        if (file.error() == QFileDevice::OpenError)
            return data;
        throwFileError(file, file.errorString());
    }
    data = file.readAll();
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    file.close();
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    return data;
}

void TFileSystemHelper::writeFile(const QString& filePath, const QByteArray& data) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throwFileError(file, QStringLiteral("Could not open file."));
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    const qint64 written = file.write(data);
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
    if (written != data.size())
        throwFileError(file, QStringLiteral("Could not write all data to disk."));
    file.close();
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
}

void TFileSystemHelper::writeTextFile(const QString& filePath, const QString& text) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        throwFileError(file, QStringLiteral("Could not open file."));
    QTextStream stream(&file);
    stream << text;
    file.close();
}

void TFileSystemHelper::appendTextFile(const QString& filePath, const QString& text) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        throwFileError(file, QStringLiteral("Could not open file."));
    QTextStream stream(&file);
    stream << text;
    file.close();
}

void TFileSystemHelper::removeFile(const QString& filePath) const
{
    if (QFile::remove(filePath))
        return;
    throw std::runtime_error((QStringLiteral("Failed to delete file ") + filePath).toStdString());
}

QDir TFileSystemHelper::createDirectory(const QString& directoryPath) const
{
    QDir directory(directoryPath);
    if (!directory.exists()) {
        if (!directory.mkpath(QStringLiteral(".")))
            throw std::runtime_error(QStringLiteral("Failed to create folder: '%1'.")
                                         .arg(QDir::toNativeSeparators(directory.path()))
                                         .toStdString());
        directory.refresh();
    }
    return directory;
}

QString TFileSystemHelper::getPathInsideDirectory(const QDir& baseDirectory, const QString& path) const
{
    const QString base = QDir::cleanPath(baseDirectory.absolutePath());
    QString joined = QDir::cleanPath(joinPaths({ base, path }));
    if (joined.startsWith(base, Qt::CaseSensitive))
        return joined;
    const QString fileName = QFileInfo(path).fileName();
    const QString name = fileName.isEmpty() ? QDir(path).dirName() : QFileInfo(path).fileName();
    return QDir::cleanPath(joinPaths({ base, name }));
}

void TFileSystemHelper::forEachFileInDirectory(const QString& directoryPath,
                                               const std::function<void(const TLocalFileInfo&)>& callback) const
{
    const std::vector<TLocalFileInfo> files = getFilesInDirectory(directoryPath);
    for (const TLocalFileInfo& file : files)
        callback(file);
}

std::vector<TLocalFileInfo> TFileSystemHelper::getFilesInDirectory(const QString& directoryPath) const
{
    const QDir directory(directoryPath);
    const QFileInfoList entries =
        directory.entryInfoList(QDir::Files | QDir::NoDot | QDir::NoDotDot, QDir::NoSort);
    std::vector<TLocalFileInfo> files;
    for (const QFileInfo& entry : entries)
        files.emplace_back(entry.absoluteFilePath(), entry.size());
    return files;
}

bool TFileSystemHelper::fileExists(const QString& filePath) const
{
    return QFileInfo::exists(filePath);
}

bool TFileSystemHelper::directoryExists(const QString& directoryPath) const
{
    return QDir(directoryPath).exists();
}

QString TFileSystemHelper::joinPaths(const QStringList& parts) const
{
    QString path;
    for (const QString& part : parts)
        path.append(QLatin1Char('/')).append(part);
    if (!path.isEmpty())
        path = path.right(path.size() - 1);
    return QDir::toNativeSeparators(path);
}

QDir TFileSystemHelper::getOrCreateSubdirectory(const QString& directoryPath, const QString& name) const
{
    if (!directoryExists(directoryPath))
        throw std::runtime_error("Directory doesn't exist");
    QDir directory(directoryPath);
    if (!directory.cd(name)) {
        if (!directory.mkdir(name))
            throw std::runtime_error("Failed to create subdirectory.");
        directory.cd(name);
    }
    return directory;
}

std::shared_ptr<QSettings> TFileSystemHelper::openSettings(const QString& filePath) const
{
    return std::make_shared<QSettings>(filePath, QSettings::IniFormat);
}

QJsonDocument TFileSystemHelper::readJsonFileAsQJsonDocument(const QString& filePath, bool& error) const
{
    const QByteArray data = readFileIfExists(filePath);
    QJsonParseError parseError;
    parseError.offset = -1;
    parseError.error = QJsonParseError::NoError;
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    error = false;
    if (!data.isEmpty() && parseError.error != QJsonParseError::NoError) {
        qWarning() << "Json parse error in file" << filePath << ": " << parseError.errorString() << "at offset"
                   << parseError.offset;
        document = QJsonDocument();
        error = true;
    }
    return document;
}

std::shared_ptr<QImage> TFileSystemHelper::readImage(const QString& filePath, const char* format) const
{
    const QByteArray data = readFileIfExists(filePath);
    if (data.isEmpty())
        return {};
    QImage image = QImage::fromData(data, format);
    if (image.isNull())
        return {};
    return std::make_shared<QImage>(std::move(image));
}

void TFileSystemHelper::writeImage(const QImage& image, const QString& filePath, const char* format) const
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);
    writeFile(filePath, data);
}

}
