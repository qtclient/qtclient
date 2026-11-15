#include "qmlitemslotdata.h"

#include <QLocale>
#include <QStringList>

#include <algorithm>

namespace tibia::container {

QString countVariants(const uint count)
{
    QStringList variants;
    const QString grouped = QLocale(QLocale::English).toString(count);
    const QString plain = QString::number(count);
    variants.append(grouped);
    if (grouped != plain)
        variants.append(plain);
    if (count > 999) {
        variants.append(QString::number(count / 1000) + QLatin1Char('k'));
        if (count > 999999)
            variants.append(QString::number(count / 1000000) + QLatin1String("kk"));
    }
    if (variants.size() == 1)
        return variants.first();
    return variants.join(QChar(0x9c));
}

TQMLItemSlotData::TQMLItemSlotData(const uint16_t typeId, const uint32_t count, const QString& name, QObject* parent)
    : cip::tibia::container::TQMLItemSlotData(parent)
    , m_typeId(typeId)
    , m_displayTypeId(typeId)
    , m_count(count)
    , m_name(name)
{
}

int TQMLItemSlotData::cumulativeCount() const
{
    return static_cast<int>(std::min<uint32_t>(m_count, 255));
}

QString TQMLItemSlotData::countString() const
{
    return countVariants(m_count);
}

QString TQMLItemSlotData::tooltipString() const
{
    if (m_name.isEmpty())
        return QString();
    return QStringLiteral("%1x %2").arg(QLocale(QLocale::English).toString(m_count), m_name);
}

void TQMLItemSlotData::setCount(const uint32_t count)
{
    if (m_count == count)
        return;
    m_count = count;
    Q_EMIT countChanged();
}

void TQMLItemSlotData::setDisplayTypeID(const uint16_t typeId)
{
    if (m_displayTypeId == typeId)
        return;
    m_displayTypeId = typeId;
    Q_EMIT typeIDchanged();
}

void TQMLItemSlotData::setLootvalueImageSources(const QString& underItem, const QString& overItem)
{
    if (m_lootvalueUnder == underItem && m_lootvalueOver == overItem)
        return;
    m_lootvalueUnder = underItem;
    m_lootvalueOver = overItem;
    Q_EMIT lootvalueCategoryChanged();
}

}
