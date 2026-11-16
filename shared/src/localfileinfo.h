#pragma once

#include <QString>
#include <QtGlobal>

namespace tibia::shared {

/// CIP `shared::TLocalFileInfo`: a file found on disk, its absolute path and size.
class TLocalFileInfo
{
public:
    TLocalFileInfo(QString filePath, qint64 size);
    virtual ~TLocalFileInfo() = default;

    QString getFilePath() const;
    QString getFileName() const;
    qint64 getSize() const;

private:
    QString m_filePath;
    qint64 m_size = 0;
};

}
