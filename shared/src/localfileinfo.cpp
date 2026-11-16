#include "localfileinfo.h"

#include <QFileInfo>

namespace tibia::shared {

TLocalFileInfo::TLocalFileInfo(QString filePath, const qint64 size)
    : m_filePath(std::move(filePath))
    , m_size(size)
{
}

QString TLocalFileInfo::getFilePath() const
{
    return m_filePath;
}

QString TLocalFileInfo::getFileName() const
{
    return QFileInfo(m_filePath).fileName();
}

qint64 TLocalFileInfo::getSize() const
{
    return m_size;
}

}
