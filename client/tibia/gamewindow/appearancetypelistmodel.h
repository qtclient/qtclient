#pragma once

#include "shared/sortedvectorlistmodel.h"

#include <cip_gen.h>

#include <QCollator>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace tibia {
namespace gamewindow {

/// One object type of the list (CIP: the 0x148-byte entries of
/// TAppearanceTypeListModel, one per type and upgrade tier).
struct TAppearanceTypeEntry
{
    uint16_t typeId = 0;
    uint8_t tier = 0;
    bool hasTier = false;
    QString name;
    int marketCategory = 0;
    int classification = 0;
    int requiredLevel = 0;
    /// Bit (profession - 1) per profession that may use it; 0 for all.
    uint16_t professions = 0;
    bool hasClothSlot = false;
    int clothSlot = 0;
    /// The names the type's imbuement filter matches (CIP: the entries of
    /// the type's list with a non-zero count).
    QStringList imbuements;
};

/// Object types with their counts (CIP: TAppearanceTypeListModel, data
/// with roles 0x101..0x116, roleNames).
class TAppearanceTypeListModel final : public shared::TSortedVectorListModel<cip::tibia::gamewindow::TAppearanceTypeListModel>
{
    Q_OBJECT
public:
    enum Role {
        TypeID = 0x101,
        DisplayTypeID,
        CumulativeCount,
        TypeCount,
        CountString,
        CountAndNameTooltip,
        TypeName,
        LiquideType,
        HookDirection,
        LootvalueImageSourceUnderItem,
        LootvalueImageSourceOverItem,
        HighlightText,
        Classification,
        UpgradeTier,
        MarketCategory,
        DecoItemObjectID,
        TradeableInMarket,
        MarketAverageValue,
        NpcSellValue,
        ProficiencyXP,
        ProficiencyLevel,
        HasProficiencyMastery,
    };

    using CountProvider = std::function<int(uint16_t typeId, uint8_t tier)>;

    explicit TAppearanceTypeListModel(CountProvider counts, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Takes the new types with the fewest row signals: both lists in
    /// (type, tier) order are merged; rows missing from the new list are
    /// removed, new ones inserted, kept ones left alone (an entry whose tier
    /// presence or tier changed is reported changed).
    void setItems(std::vector<TAppearanceTypeEntry> entries);
    void reset(std::vector<TAppearanceTypeEntry> entries) { setItems(std::move(entries)); }
    /// (type, tier) order; an entry without a tier sorts as tier 0.
    static bool lessThan(const TAppearanceTypeEntry& left, const TAppearanceTypeEntry& right);
    static bool sameKey(const TAppearanceTypeEntry& left, const TAppearanceTypeEntry& right);
    /// The entry of `row`, or an empty one for a row out of range.
    const TAppearanceTypeEntry& entry(int row) const;
    int size() const { return static_cast<int>(m_entries.size()); }

public Q_SLOTS:
    void onItemPriceVisualizationChanged() override;
    void onItemTrackingChanged() override;
    void onObjectCountsChanged() override;
    void onObjectProficiencyXPChanged() override;
    /// A type's proficiency changed: its roles are announced once the event
    /// loop runs (several changes in a row are announced together).
    void onProficiencyDataChanged(uint16_t AppearanceTypeID);

private:
    /// dataChanged over every row for `roles`.
    void notifyRolesChanged(const QList<int>& roles);
    int count(const TAppearanceTypeEntry& entry) const;

    /// Announces the proficiency roles of every row.
    void onProficiencyTimerTimeout();

    CountProvider m_counts;
    QTimer m_proficiencyTimer;
    std::vector<TAppearanceTypeEntry> m_entries;
};

/// The filter buttons of the item lists (CIP: TAppearanceTypeFilter, the
/// 0x68-byte value at TAppearanceTypeListModelSortFilterProxy+0x140). Each
/// selection is a mode (0 off, 1 none only, 2 any, 3 `value` only) with the
/// label the drop-down shows.
struct TAppearanceTypeFilter
{
    bool filterByLevel = false;
    bool filterByVocation = false;
    bool onlyOneHandWeapons = false;
    bool onlyTwoHandWeapons = false;
    int classificationMode = 0;
    int classification = 0;
    QString classificationName;
    int tierMode = 0;
    int tier = 0;
    QString tierName;
    int imbuementMode = 0;
    int imbuementValue = 0;
    QString imbuement;

    /// CIP: member-wise, the labels included.
    bool operator==(const TAppearanceTypeFilter& other) const;
    bool operator!=(const TAppearanceTypeFilter& other) const { return !(*this == other); }
    /// Whether `entry` passes the buttons for a player of `level` and
    /// `profession`; `tier` is the row's upgrade tier, if it has one.
    bool accepts(const TAppearanceTypeEntry& entry, std::optional<uint8_t> tier, int level, int profession) const;
};

/// Category, name and filter buttons over the list (CIP:
/// TAppearanceTypeListModelSortFilterProxy). Filter changes are applied on
/// the next event loop turn (a zero-interval single-shot timer started by
/// filterChanged), or at once by applyFilterNow.
class TAppearanceTypeListModelSortFilterProxy final : public cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy
{
    Q_OBJECT
public:
    /// Category identifiers of TQmlObjectCategory.
    static constexpr int AllCategories = 0xff;
    static constexpr int AllWeaponsCategory = 0x1d;
    /// CIP .
    static bool isHandWeaponCategory(int category);

    /// The orders of the list (CIP +0x1ac); anything else sorts like
    /// NameAscending.
    enum ESortMode {
        NameAscending = 1,
        NameDescending = 2,
        AverageValueDescending = 5,
        AverageValueAscending = 6,
        TotalAverageValueDescending = 7,
        TotalAverageValueAscending = 8,
        NpcValueDescending = 9,
        NpcValueAscending = 10,
        TotalNpcValueDescending = 11,
        TotalNpcValueAscending = 12,
        CountDescending = 13,
        CountAscending = 14,
        ProficiencyDescending = 15,
    };

    explicit TAppearanceTypeListModelSortFilterProxy(TAppearanceTypeListModel* model, QObject* parent = nullptr);

    bool filterByLevel() const override { return m_filter.filterByLevel; }
    bool filterByVocation() const override { return m_filter.filterByVocation; }
    bool onlyOneHandWeapons() const override { return m_filter.onlyOneHandWeapons; }
    bool onlyTwoHandWeapons() const override { return m_filter.onlyTwoHandWeapons; }

    /// Clears category, type set, name, buttons and type.
    void resetFilters();
    /// The category (or -1) with the buttons; the other filters are reset
    /// when anything changes.
    void setCategoryFilter(int category, const TAppearanceTypeFilter& filter, bool showLockerOnly);
    void setCategoryFilter(int category, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand,
                           TAppearanceTypeFilter filter, bool showLockerOnly);
    /// The name with the buttons; the other filters are reset when the name
    /// or the buttons change.
    void setNameFilter(const QString& name, const TAppearanceTypeFilter& filter, bool showLockerOnly);
    void setNameFilter(const QString& name, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand,
                       TAppearanceTypeFilter filter, bool showLockerOnly);
    /// Only the type shown as `typeId`; the other filters are reset.
    void setTypeFilter(uint16_t typeId);
    void setCategory(int category);
    void setFilter(const TAppearanceTypeFilter& filter);
    void setName(const QString& name);
    void setTypeId(uint16_t typeId);
    void setTypeIds(const std::set<int>& typeIds);
    void setShowLockerOnly(bool showLockerOnly);
    /// Rows pass without any filter set (otherwise the list is empty then).
    void setAcceptWithoutFilter(bool accept);
    void setSortMode(int sortMode);
    /// Replaces the model's entries inside one filter change.
    void resetEntries(std::vector<TAppearanceTypeEntry> entries);
    /// Applies pending filter changes now.
    void applyFilterNow();

    int category() const { return m_category; }
    QString name() const { return m_name; }
    uint16_t typeId() const { return m_typeId; }
    const TAppearanceTypeFilter& filter() const { return m_filter; }
    bool showLockerOnly() const { return m_showLockerOnly; }

    /// The row of the type and tier, or -1.
    int rowOf(uint16_t typeId, uint8_t tier) const;
    uint16_t typeIdAt(int row) const;
    uint8_t tierAt(int row) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TAppearanceTypeListModel* m_model;
    QCollator m_collator;
    QTimer m_filterTimer;
    int m_category = -1;
    std::set<int> m_typeIds;
    QString m_name;
    uint16_t m_typeId = 0;
    TAppearanceTypeFilter m_filter;
    bool m_acceptWithoutFilter = false;
    bool m_showLockerOnly = false;
    int m_sortMode = NameAscending;
};

}
}
