#include "assignablebossmodels.h"

namespace tibia::gamewindow {

TAssignableBossModel::TAssignableBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::TAssignableBossModel(parent)
    , m_storage(storage)
{}

int TAssignableBossModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_bosses.size());
}

QVariant TAssignableBossModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const cyclopedia::TAssignableBoss& boss = m_bosses[static_cast<size_t>(index.row())];
    switch (role) {
        case BossRaceId: return boss.raceId;
        case Name: return boss.name;
        case BossRaritySource: return cyclopedia::bossRaritySource(boss.category);
        case BossRarityTooltip: return m_storage.rarityTooltip(boss.category);
        default: return {};
    }
}

QHash<int, QByteArray> TAssignableBossModel::roleNames() const
{
    return {
        { BossRaceId, "bossRaceId" },
        { Name, "name" },
        { BossRaritySource, "bossRaritySource" },
        { BossRarityTooltip, "bossRarityTooltip" },
    };
}

int TAssignableBossModel::count() const
{
    return static_cast<int>(m_bosses.size());
}

void TAssignableBossModel::setBosses(const std::vector<cyclopedia::TAssignableBoss>& bosses)
{
    beginResetModel();
    m_bosses = bosses;
    m_count = static_cast<int>(m_bosses.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

void TAssignableBossModel::refreshAll()
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1));
}

TAssignableBossSortProxyModel::TAssignableBossSortProxyModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::TAssignableBossSortProxyModel(parent)
    , m_source(new TAssignableBossModel(storage, this))
{
    m_raceIDRoleEnumValue = TAssignableBossModel::BossRaceId;
    setSourceModel(m_source);
    sort(0, Qt::AscendingOrder);
}

QString TAssignableBossSortProxyModel::nameFilter() const
{
    return m_nameFilter;
}

void TAssignableBossSortProxyModel::setNameFilter(const QString& v)
{
    if (v == m_nameFilter)
        return;
    beginFilterChange();
    m_nameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

int TAssignableBossSortProxyModel::raceIDRoleEnumValue() const
{
    return m_raceIDRoleEnumValue;
}

void TAssignableBossSortProxyModel::resetFilter()
{
    setNameFilter({});
}

bool TAssignableBossSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (m_nameFilter.isEmpty())
        return true;
    const QString name = m_source->data(m_source->index(sourceRow), TAssignableBossModel::Name).toString();
    return name.contains(m_nameFilter, Qt::CaseInsensitive);
}

bool TAssignableBossSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString a = m_source->data(left, TAssignableBossModel::Name).toString();
    const QString b = m_source->data(right, TAssignableBossModel::Name).toString();
    return QString::compare(a, b, Qt::CaseSensitive) < 0;
}

}
