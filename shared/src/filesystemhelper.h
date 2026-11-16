#pragma once

#include "localfileinfo.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

namespace tibia::shared {

class TReadOnlyMemoryBlob;

/// Throws std::runtime_error "I/O error on file '<name>': <error>".
[[noreturn]] void throwFileError(const QFile& file, const QString& error);

/// CIP `shared::TFileSystemHelper`: file and directory I/O used by the
/// client's path services. Failures throw std::runtime_error.
class TFileSystemHelper
{
public:
    virtual ~TFileSystemHelper() = default;

    /// The file as a blob: `filePath` itself when it ends in ".lzma" or no
    /// "<filePath>.lzma" exists, else that LZMA file. LZMA blobs unpack on read.
    virtual std::shared_ptr<TReadOnlyMemoryBlob> openReadOnlyMemoryBlob(const QString& filePath) const;

    virtual QByteArray readFile(const QString& filePath) const;
    /// Empty when the file cannot be opened.
    virtual QByteArray readFileIfExists(const QString& filePath) const;
    virtual void writeFile(const QString& filePath, const QByteArray& data) const;
    virtual void writeTextFile(const QString& filePath, const QString& text) const;
    virtual void appendTextFile(const QString& filePath, const QString& text) const;
    virtual void removeFile(const QString& filePath) const;
    virtual QDir createDirectory(const QString& directoryPath) const;
    /// `path` inside `baseDirectory`; a path escaping it is reduced to its last component.
    virtual QString getPathInsideDirectory(const QDir& baseDirectory, const QString& path) const;
    virtual void forEachFileInDirectory(const QString& directoryPath,
                                        const std::function<void(const TLocalFileInfo&)>& callback) const;
    virtual std::vector<TLocalFileInfo> getFilesInDirectory(const QString& directoryPath) const;
    virtual bool fileExists(const QString& filePath) const;
    virtual bool directoryExists(const QString& directoryPath) const;
    virtual QString joinPaths(const QStringList& parts) const;
    virtual QDir getOrCreateSubdirectory(const QString& directoryPath, const QString& name) const;
    virtual std::shared_ptr<QSettings> openSettings(const QString& filePath) const;
    virtual QJsonDocument readJsonFileAsQJsonDocument(const QString& filePath, bool& error) const;
    virtual std::shared_ptr<QImage> readImage(const QString& filePath, const char* format) const;
    virtual void writeImage(const QImage& image, const QString& filePath, const char* format) const;
};

}
