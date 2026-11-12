#include "qmlitemslotdata.h"

#include <QLocale>
#include <QStringList>

#include <algorithm>

namespace tibia::container {

QString countVariants(const uint count)
{
    QStringList variants;
    variants.append(QLocale(QLocale::English).toString(count));
    const QString plain = QString::number(count);
    if (plain != variants.first())
        variants.append(plain);
    if (count > 999) {
        variants.append(QString::number(count / 1000) + QLatin1Char('k'));
        if (count > 999999)
            variants.append(QString::number(count / 1000000) + QLatin1String("kk"));
    }
    return variants.join(QChar(0x9c));
}

TQMLItemSlotData::TQMLItemSlotData(const int typeId, const uint count, const QString& name, QObject* parent)
    : cip::tibia::container::TQMLItemSlotData(parent)
    , m_count(count)
    , m_name(name)
{
    m_typeID = typeId;
    m_displayTypeID = typeId;
}

// b1/d6db2dda6351a121
void TQMLItemSlotData::setCount(const uint count)
{
    if (count == m_count)
        return;
    m_count = count;
    Q_EMIT countChanged();
}

// b1/2e06661686807eaa
int TQMLItemSlotData::cumulativeCount() const
{
    return static_cast<int>(std::min<uint>(m_count, 255));
}

// b1/97c7118feb2daa0e
QString TQMLItemSlotData::countString() const
{
    return countVariants(m_count);
}

// b1/de57f04c733cda73
QString TQMLItemSlotData::tooltipString() const
{
    if (m_name.isEmpty())
        return {};
    return QStringLiteral("%1x %2").arg(QLocale(QLocale::English).toString(m_count), m_name);
}

}
