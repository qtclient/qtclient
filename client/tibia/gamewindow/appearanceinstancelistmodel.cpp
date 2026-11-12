#include "appearanceinstancelistmodel.h"

#include "app/clientoptions.h"
#include "container/containerslotinformation.h"
#include "cyclopedia/iteminfostorage.h"

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
using Slot = container::TInventorySlot;

/// CIP's count for a cumulative object, 1 for any other.
uint cumulativeCount(const Slot& slot)
{
    return slot.stackable ? std::max<uint>(1, slot.count) : 1;
}

/// CIP case 4: with showExpireInContainers, the charges, else the time left.
QString alternativeSlotString(const Slot& slot, const bool showExpire, const bool whenUnused)
{
    if (!showExpire)
        return {};
    if (slot.hasWearOut) {
        if (QString charges = container::chargesText(slot, whenUnused); !charges.isEmpty())
            return charges;
    }
    return container::hasClock(slot) ? container::clockText(slot, whenUnused) : QString();
}

/// CIP / : loot-value frame under (style 1) or over (2).
QString lootValueImageSource(const Slot& slot, const int style)
{
    if (slot.empty())
        return {};
    // gameWindowShowLootHighlighting (CIP 0x42): master gate for loot-value tint frames.
    if (!app::ClientOptions::load().options().value(QLatin1String("gameWindowShowLootHighlighting")).toBool(true))
        return {};
    const cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
    if (!prices || prices->colorizeLootStyle() != style)
        return {};
    const int level = prices->lootValueLevel(slot.typeId);
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
    const Slot slot = index.row() < static_cast<int>(m_objects.size()) ? m_objects[index.row()] : Slot{};
    // CIP gives empty slots no data; container.qml reads their strings' length.
    switch (role) {
    case TypeIdRole:
        return slot.typeId;
    case CumulativeCountRole:
    case TypeCountRole:
        return slot.empty() ? 0u : cumulativeCount(slot);
    case CountStringRole:
        return slot.empty() ? QString() : QString::number(cumulativeCount(slot));
    case AlternativeSlotStringRole:
        return slot.empty() ? QString() : alternativeSlotString(slot, m_showExpire, m_showExpireWhenUnused);
    case LiquidTypeRole:
        return QVariant::fromValue(static_cast<Info::ELiquidType>(slot.fluidOrSplash ? slot.fluid : 0));
    case HookDirectionRole:
        return QVariant::fromValue(Info::EHookDirection::NONE);
    case LootValueImageUnderItemRole:
        return lootValueImageSource(slot, 1);
    case LootValueImageOverItemRole:
        return lootValueImageSource(slot, 2);
    case ManagedContainerTooltipRole:
        return container::managedContainerTooltip(slot);
    case UpgradeTierRole:
        return static_cast<uint>(slot.tier);
    case DecoItemObjectIdRole:
        return static_cast<uint>(slot.decoItemId);
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

void TAppearanceInstanceListModel::setObjects(std::vector<container::TInventorySlot> objects)
{
    if (objects.size() == m_objects.size()) {
        bool same = true;
        for (size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].typeId != m_objects[i].typeId || objects[i].count != m_objects[i].count
                || objects[i].fluid != m_objects[i].fluid || objects[i].tier != m_objects[i].tier
                || objects[i].decoItemId != m_objects[i].decoItemId
                || objects[i].charges != m_objects[i].charges
                || objects[i].durationEndMs != m_objects[i].durationEndMs
                || objects[i].containerAmmoTotal != m_objects[i].containerAmmoTotal) {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }
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
