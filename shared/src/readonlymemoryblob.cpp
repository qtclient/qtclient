#include "readonlymemoryblob.h"

#include "filesystemhelper.h"

#include <QFile>

namespace tibia::shared {

namespace {

void throwOnFileError(const QFile& file)
{
    if (file.error() != QFileDevice::NoError)
        throwFileError(file, file.errorString());
}

}

TReadOnlyMemoryBlob::TReadOnlyMemoryBlob(const QString& filePath)
    : m_filePath(filePath)
{
    QFile file(filePath);
    if (!file.exists())
        throwFileError(file, QStringLiteral("File does not exist"));
}

TReadOnlyMemoryBlob::TReadOnlyMemoryBlob(const QByteArray& data)
    : m_data(data)
{
}

QByteArray TReadOnlyMemoryBlob::getData() const
{
    if (!m_data.isEmpty())
        return m_data;
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        throwFileError(file, QStringLiteral("Could not open file."));
    throwOnFileError(file);
    QByteArray data = file.readAll();
    throwOnFileError(file);
    return data;
}

QString TReadOnlyMemoryBlob::getFilePath() const
{
    return m_filePath;
}

}
