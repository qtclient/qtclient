#pragma once

#include <cip_gen.h>

#include <QCollator>
#include <QString>

#include <cstdint>
#include <functional>
#include <vector>

namespace tibia::gamewindow {

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
};

/// Object types with their counts (CIP: TAppearanceTypeListModel, data
/// 0x100d6acd0 with roles 0x101..0x115, roleNames 0x100d6a4e0).
class TAppearanceTypeListModel final : public cip::tibia::gamewindow::TAppearanceTypeListModel
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

    void reset(std::vector<TAppearanceTypeEntry> entries);
    const TAppearanceTypeEntry& entry(int row) const { return m_entries[row]; }
    int size() const { return static_cast<int>(m_entries.size()); }

public Q_SLOTS:
    void onObjectCountsChanged() override;

private:
    int count(const TAppearanceTypeEntry& entry) const;

    CountProvider m_counts;
    std::vector<TAppearanceTypeEntry> m_entries;
};

/// The filters of TAppearanceTypeListModelSortFilterProxy (CIP 0x100cac240).
struct TAppearanceTypeFilter
{
    /// 0 off, 1 unclassified only, 2 any, 3 `value` only.
    int classificationMode = 0;
    int classification = 0;
    /// 0 off, 3 `value` only.
    int tierMode = 0;
    int tier = 0;
};

/// Category, name and filter buttons over the list, sorted by name (CIP:
/// TAppearanceTypeListModelSortFilterProxy, filterAcceptsRow 0x100d6bc60).
class TAppearanceTypeListModelSortFilterProxy final : public cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy
{
    Q_OBJECT
public:
    /// Category identifiers of TQmlObjectCategory.
    static constexpr int AllCategories = 0xff;
    static constexpr int AllWeaponsCategory = 0x1d;
    /// CIP 0x100d58830.
    static bool isHandWeaponCategory(int category);

    explicit TAppearanceTypeListModelSortFilterProxy(TAppearanceTypeListModel* model, QObject* parent = nullptr);

    /// CIP 0x100d6d140: the category (or -1) and the buttons' filters.
    void setCategoryFilter(int category, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand,
                           const TAppearanceTypeFilter& filter, bool showLockerOnly);
    /// CIP 0x100d6d330: the name (no category) and the buttons' filters.
    void setNameFilter(const QString& name, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand,
                       const TAppearanceTypeFilter& filter, bool showLockerOnly);
    /// CIP 0x100d6d4e0: only `typeId` (the market opened on a type).
    void setTypeFilter(uint16_t typeId);
    /// CIP 0x100d6daf0: the row of the type and tier, or -1.
    int rowOf(uint16_t typeId, uint8_t tier) const;
    uint16_t typeIdAt(int row) const;
    uint8_t tierAt(int row) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void setButtons(bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand);

    TAppearanceTypeListModel* m_model;
    int m_category = -1;
    QString m_name;
    uint16_t m_typeId = 0;
    TAppearanceTypeFilter m_filter;
    bool m_showLockerOnly = false;
    QCollator m_collator;
};

}
