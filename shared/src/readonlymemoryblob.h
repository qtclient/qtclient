#pragma once

#include <QByteArray>
#include <QString>

namespace tibia::shared {

/// CIP `shared::TReadOnlyMemoryBlob`: read-only bytes backed either by a file
/// (read on every getData()) or by an in-memory buffer.
class TReadOnlyMemoryBlob
{
public:
    /// Throws std::runtime_error when the file does not exist.
    explicit TReadOnlyMemoryBlob(const QString& filePath);
    explicit TReadOnlyMemoryBlob(const QByteArray& data);
    virtual ~TReadOnlyMemoryBlob() = default;

    /// The buffer, or the file's contents when the blob has no buffer.
    /// Throws std::runtime_error on an I/O error.
    virtual QByteArray getData() const;
    virtual QString getFilePath() const;

private:
    QString m_filePath;
    QByteArray m_data;
};

}
