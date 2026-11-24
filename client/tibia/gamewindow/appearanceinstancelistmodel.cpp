#include "appearanceinstancelistmodel.h"
#include "appearanceinstanceroledata.h"

#include "container/containerslotinformation.h"
#include "analysers/itempricestorage.h"
#include "config/clientoptions.h"

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

}

/// CIP / : loot-value frame under (style 1) or over (2).
QString TAppearanceInstanceListModel::lootValueImageSource(const Slot& slot, const int style) const
{
    if (slot.empty() || !m_clientOptions)
        return {};
    // gameWindowShowLootHighlighting (CIP 0x42): master gate for loot-value tint frames.
    if (!m_clientOptions->boolOption(config::EClientOption::gameWindowShowLootHighlighting))
        return {};
    if (m_clientOptions->colorizeLootStyle() != style)
        return {};
    if (!m_prices)
        return {};
    const int level = m_prices->lootValueLevel(slot.typeId);
    if (level == 0)
        return {};
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

TAppearanceInstanceListModel::TAppearanceInstanceListModel(config::TClientOptions* ClientOptions, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceInstanceListModel(parent)
    , m_clientOptions(ClientOptions)
{
    if (m_clientOptions)
        connect(m_clientOptions.data(), &config::TClientOptions::appearanceTypeListModelOptionsChanged, this,
                &TAppearanceInstanceListModel::refreshLootValues);
}

void TAppearanceInstanceListModel::setItemPriceStorage(analysers::TItemPriceStorage* prices)
{
    if (m_prices == prices)
        return;
    QObject::disconnect(m_pricesConfigurationChanged);
    QObject::disconnect(m_pricesMarketAvailable);
    m_prices = prices;
    if (m_prices) {
        m_pricesConfigurationChanged = connect(m_prices.data(), &analysers::TItemPriceStorage::configurationChangedFromUserInput, this,
                                               &TAppearanceInstanceListModel::refreshLootValues);
        m_pricesMarketAvailable = connect(m_prices.data(), &analysers::TItemPriceStorage::marketPricesAvailable, this,
                                          &TAppearanceInstanceListModel::refreshLootValues);
    }
    refreshLootValues();
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
    if (!slot.empty()) {
        if (role < TypeIdRole || role > DecoItemObjectIdRole)
            return {};
        // An object's roles come from the shared appearance-instance helper; the type count
        // is the player's count of the type and tier when a count storage is set.
        TAppearanceInstanceRoleSources sources;
        sources.withOptions = true;
        sources.clientOptions = m_clientOptions.data();
        sources.prices = m_prices.data();
        sources.showExpire = m_showExpire;
        sources.showExpireWhenUnused = m_showExpireWhenUnused;
        if (const container::TObjectCountStorage* counts = m_objectCounts.data())
            sources.typeCount = [counts](const uint16_t typeId, const uint8_t tier) { return uint(counts->count(typeId, tier)); };
        return appearanceInstanceRoleData(slot, static_cast<EAppearanceInstanceRole>(role - TypeIdRole), sources, false);
    }
    // CIP gives empty slots no data; container.qml reads their strings' length.
    switch (role) {
    case TypeIdRole:
        return slot.typeId;
    case CumulativeCountRole:
        return slot.empty() ? 1u : cumulativeCount(slot);
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
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert({
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
    });
    return roles;
}

void TAppearanceInstanceListModel::setMinimumRows(const int rows)
{
    if (rows == m_minimumRows)
        return;
    if (const int count = rowCount(); count > 0) {
        beginRemoveRows({}, 0, count - 1);
        endRemoveRows();
    }
    m_minimumRows = rows;
    if (const int count = rowCount(); count > 0) {
        beginInsertRows({}, 0, count - 1);
        endInsertRows();
    }
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
    if (const int count = rowCount(); count > 0)
        Q_EMIT dataChanged(index(0), index(count - 1), { LootValueImageUnderItemRole, LootValueImageOverItemRole, AlternativeSlotStringRole });
}

void TAppearanceInstanceListModel::setObjectCountStorage(container::TObjectCountStorage* counts)
{
    QObject::disconnect(m_objectCountsChanged);
    m_objectCounts = counts;
    if (!counts)
        return;
    m_objectCountsChanged = connect(counts, &container::TObjectCountStorage::objectCountsChanged, this,
                                    &TAppearanceInstanceListModel::onObjectCountsChanged);
    refreshLootValues();
    onObjectCountsChanged();
}

void TAppearanceInstanceListModel::onObjectCountsChanged()
{
    if (const int count = rowCount(); count > 0)
        Q_EMIT dataChanged(index(0), index(count - 1), { TypeCountRole, CountStringRole });
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
