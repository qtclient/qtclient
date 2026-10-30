#include "appearanceinstancelistmodel.h"

#include "app/clientoptions.h"
#include "container/containerslotinformation.h"
#include "cyclopedia/iteminfostorage.h"

#include <client/item.h>

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// CIP's count for a cumulative object, 1 for any other.
uint cumulativeCount(const ItemPtr& item)
{
    return item->isStackable() ? std::max<uint>(1, item->getCountOrSubType()) : 1;
}

/// CIP case 4: with showExpireInContainers, the charges, else the time left.
QString alternativeSlotString(const ItemPtr& item, const bool showExpire, const bool whenUnused)
{
    if (!showExpire)
        return {};
    if (item->hasWearOut()) {
        if (QString charges = container::chargesText(item, whenUnused); !charges.isEmpty())
            return charges;
    }
    return container::hasClock(item) ? container::clockText(item, whenUnused) : QString();
}

/// CIP FUN_100927c50 / FUN_100927d10: loot-value frame under (style 1) or over (2).
QString lootValueImageSource(const ItemPtr& item, const int style)
{
    if (!item)
        return {};
    // gameWindowShowLootHighlighting (CIP 0x42): master gate for loot-value tint frames.
    if (!app::ClientOptions::load().options().value(QLatin1String("gameWindowShowLootHighlighting")).toBool(true))
        return {};
    const cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
    if (!prices || prices->colorizeLootStyle() != style)
        return {};
    const int level = prices->lootValueLevel(item->getId());
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

}

TAppearanceInstanceListModel::TAppearanceInstanceListModel(QObject* parent)
    : cip::tibia::gamewindow::TAppearanceInstanceListModel(parent)
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("colorizeLootStyle") || name == QLatin1String("gameWindowShowLootHighlighting"))
            refreshLootValues();
    });
    if (cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance())
        connect(prices, &cyclopedia::TItemInfoStorage::pricesChanged, this, &TAppearanceInstanceListModel::refreshLootValues);
}

int TAppearanceInstanceListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : std::max(m_minimumRows, static_cast<int>(m_objects.size()));
}

QVariant TAppearanceInstanceListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const ItemPtr item = index.row() < static_cast<int>(m_objects.size()) ? m_objects[index.row()] : nullptr;
    // CIP gives empty slots no data; container.qml reads their strings' length.
    switch (role) {
    case TypeIdRole:
        return item ? item->getId() : 0u;
    case CumulativeCountRole:
    case TypeCountRole:
        return item ? cumulativeCount(item) : 0u;
    case CountStringRole:
        return item ? QString::number(cumulativeCount(item)) : QString();
    case AlternativeSlotStringRole:
        return item ? alternativeSlotString(item, m_showExpire, m_showExpireWhenUnused) : QString();
    case LiquidTypeRole: {
        const bool liquid = item && (item->isFluidContainer() || item->isSplash());
        return QVariant::fromValue(static_cast<Info::ELiquidType>(liquid ? item->getCountOrSubType() : 0));
    }
    case HookDirectionRole:
        return QVariant::fromValue(Info::EHookDirection::NONE);
    case LootValueImageUnderItemRole:
        return lootValueImageSource(item, 1);
    case LootValueImageOverItemRole:
        return lootValueImageSource(item, 2);
    case ManagedContainerTooltipRole:
        return container::managedContainerTooltip(item);
    case UpgradeTierRole:
        return item ? static_cast<uint>(item->getTier()) : 0u;
    case DecoItemObjectIdRole:
        return item ? static_cast<uint>(item->getDecoItemId()) : 0u;
    default:
        return {};
    }
}

QHash<int, QByteArray> TAppearanceInstanceListModel::roleNames() const
{
    return {
        { TypeIdRole, "typeID" },
        { CumulativeCountRole, "cumulativeCount" },
        { TypeCountRole, "typeCount" },
        { CountStringRole, "countString" },
        { AlternativeSlotStringRole, "alternativeSlotString" },
        { LiquidTypeRole, "liquideType" },
        { HookDirectionRole, "hookDirection" },
        { LootValueImageUnderItemRole, "lootvalueImageSourceUnderItem" },
        { LootValueImageOverItemRole, "lootvalueImageSourceOverItem" },
        { ManagedContainerTooltipRole, "managedContainerTooltip" },
        { UpgradeTierRole, "upgradeTier" },
        { DecoItemObjectIdRole, "decoItemObjectID" },
    };
}

void TAppearanceInstanceListModel::setMinimumRows(const int rows)
{
    if (rows == m_minimumRows)
        return;
    beginResetModel();
    m_minimumRows = rows;
    endResetModel();
}

void TAppearanceInstanceListModel::setObjects(std::vector<ItemPtr> objects)
{
    if (objects == m_objects)
        return;
    int count = rowCount();
    const int target = std::max(m_minimumRows, static_cast<int>(objects.size()));
    if (target < count) {
        beginRemoveRows({}, target, count - 1);
        m_objects = std::move(objects);
        endRemoveRows();
        count = target;
    } else if (target > count) {
        beginInsertRows({}, count, target - 1);
        m_objects = std::move(objects);
        endInsertRows();
    } else {
        m_objects = std::move(objects);
    }
    if (count > 0)
        Q_EMIT dataChanged(index(0), index(count - 1));
}

void TAppearanceInstanceListModel::setExpireOptions(const bool showExpire, const bool whenUnused)
{
    if (showExpire == m_showExpire && whenUnused == m_showExpireWhenUnused)
        return;
    m_showExpire = showExpire;
    m_showExpireWhenUnused = whenUnused;
    if (!m_objects.empty())
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_objects.size()) - 1), { AlternativeSlotStringRole });
}

void TAppearanceInstanceListModel::refreshLootValues()
{
    if (m_objects.empty())
        return;
    Q_EMIT dataChanged(index(0), index(static_cast<int>(m_objects.size()) - 1),
                       { LootValueImageUnderItemRole, LootValueImageOverItemRole });
}

void TAppearanceInstanceListModel::refreshClocks()
{
    if (!m_showExpire)
        return;
    for (size_t row = 0; row < m_objects.size(); ++row) {
        if (container::hasClock(m_objects[row]))
            Q_EMIT dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)), { AlternativeSlotStringRole });
    }
}

}
