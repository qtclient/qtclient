#pragma once

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QColor>
#include <QString>

#include <array>
#include <cstdint>
#include <vector>

struct CyclopediaCharacterItemSummary;

namespace tibia::gamewindow {

/// CIP: one 0x38-byte TCharacterInfoStorage item summary row (+0x98).
struct ItemSummaryEntry
{
    uint16_t typeId = 0;
    uint8_t upgradeTier = 0;
    QString name;
    /// +0x20: the item price storage's loot value level (slot 22).
    int lootValueLevel = 0;
    uint32_t inventoryAmount = 0;
    uint32_t depotAmount = 0;
    uint32_t stashAmount = 0;
    uint32_t inboxAmount = 0;
    uint32_t storeInboxAmount = 0;
};

/// CIP 0x100a52510 case 6: the five wire lists merged into one row per
/// (type, tier), kept sorted by type then tier (0x100a546f0).
std::vector<ItemSummaryEntry> mergeItemSummary(const CyclopediaCharacterItemSummary& data);

/// CIP: TItemSummaryModel (vtable 0x10265eb98, roles 0x100d162a0, data
/// 0x100d166e0).
class TItemSummaryModel final : public QAbstractListModel
{
public:
    enum Role
    {
        AppearanceTypeID = 0x101,
        UpgradeTier,
        Name,
        InventoryAmount,
        StashAmount,
        DepotAmount,
        InboxAmount,
        StoreInboxAmount,
        TotalAmount,
        LootValueSourceUnder,
        LootValueSourceOver,
    };

    TItemSummaryModel(std::vector<ItemSummaryEntry> entries, QObject* parent);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<ItemSummaryEntry> m_entries;
};

/// CIP: TItemSummaryModelSortFilterProxy (ctor 0x100d16890, vtable
/// 0x10262e7a0). Only the inventory is shown at first.
class TItemSummaryModelSortFilterProxy final : public cip::tibia::gamewindow::TItemSummaryModelSortFilterProxy
{
public:
    TItemSummaryModelSortFilterProxy(std::vector<ItemSummaryEntry> entries, QObject* parent);

    /// CIP 0x100d171d0..0x100d172d0.
    void setShowInventory(bool v) override;
    void setShowStash(bool v) override;
    void setShowDepot(bool v) override;
    void setShowInbox(bool v) override;
    void setShowStoreInbox(bool v) override;
    /// CIP 0x100d17310.
    void updateNameFilter(const QString& NameFilter) override;

protected:
    /// CIP 0x100d16a50.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP 0x100d16f50.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void setFlag(bool& flag, bool value);

    TItemSummaryModel* m_model;
    QString m_nameFilter;
};

/// CIP: one 0x40-byte character title row (storage +0xb8).
struct TitleEntry
{
    uint8_t id = 0;
    QString name;
    QString description;
    bool permanent = false;
    bool unlocked = false;
};

/// CIP: TTitlesModel (roles 0x100d18c70).
class TTitlesModel final : public QAbstractListModel
{
public:
    enum Role
    {
        TitleId = 0x102,
        Name,
        Description,
        IsPermanent,
        IsUnlocked,
        CanBeChosen,
    };

    TTitlesModel(std::vector<TitleEntry> entries, QObject* parent);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<TitleEntry> m_entries;
};

/// CIP: TTitlesSortProxyModel, embedded in the titles data (sort(0) in its
/// ctor).
class TTitlesSortProxyModel final : public cip::tibia::gamewindow::TTitlesSortProxyModel
{
public:
    TTitlesSortProxyModel(std::vector<TitleEntry> entries, QObject* parent);

    void setNameFilter(const QString& v) override;
    void setTitleFilter(EQmlTitleFilter v) override;

protected:
    /// CIP 0x100d19a50.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP 0x100d19760: case-sensitive.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::vector<TitleEntry> m_entries;
};

/// CIP: TCharacterTitlesInfoQmlData (ctor 0x100d18f90, current title lookup
/// 0x100d192f0, hasCurrentTitle 0x100d195b0).
class TCharacterTitlesInfoQmlData final : public cip::tibia::gamewindow::TCharacterTitlesInfoQmlData
{
public:
    TCharacterTitlesInfoQmlData(uint8_t currentTitleId, std::vector<TitleEntry> entries, QObject* parent);
};

/// CIP: one boost::variant<PlayerOutfit, Mount, SummonSkin> row (0x38 bytes,
/// built by 0x100a90860 in outfit, mount, summon skin order).
struct AppearanceEntry
{
    enum Kind
    {
        Outfit,
        Mount,
        SummonSkin
    };
    Kind kind = Outfit;
    uint16_t id = 0;
    QString name;
    /// TOutfitsMountsSummonSkinsModel::EOutfitMountSummonSkinSource.
    int source = 0;
    int offerId = 0;
    uint8_t addons = 0;
};

/// CIP: TOutfitsMountsSummonSkinsModel (roles 0x100d17420, data 0x100d177a0).
class TOutfitsMountsSummonSkinsModel final : public cip::tibia::gamewindow::TOutfitsMountsSummonSkinsModel
{
public:
    enum Role
    {
        OutfitID = 0x101,
        Name,
        Source,
        OfferID,
        HasFirstAddon,
        HasSecondAddon,
        IsOutfit,
        IsMount,
        IsSummonSkin,
    };

    TOutfitsMountsSummonSkinsModel(std::vector<AppearanceEntry> entries, QObject* parent);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<AppearanceEntry> m_entries;
};

/// CIP: TOutfitMountSummonSkinsListDataModelSortFilterProxy (builder
/// 0x100d136f0, metacall 0x10080b7c0). The colours are the outfit's (+0x40)
/// or the mount's (+0x44) by the type filter.
class TOutfitMountSummonSkinsListDataModelSortFilterProxy final
    : public cip::tibia::gamewindow::TOutfitMountSummonSkinsListDataModelSortFilterProxy
{
public:
    /// Head, torso, legs, detail of the outfit, then of the mount.
    using Colors = std::array<uint8_t, 8>;

    TOutfitMountSummonSkinsListDataModelSortFilterProxy(std::vector<AppearanceEntry> entries, const Colors& colors, QObject* parent);

    /// CIP 0x100d18150.
    void setTypeFilter(EQmlOutfitMountTypeFilter v) override;
    /// CIP 0x100d18110.
    void setSourceFilter(EQmlOutfitMountSourceFilter v) override;
    /// CIP 0x100d17cc0..0x100d17db0.
    QColor headColor() const override { return color(0); }
    QColor torsoColor() const override { return color(1); }
    QColor legsColor() const override { return color(2); }
    QColor detailColor() const override { return color(3); }

protected:
    /// CIP 0x100d17e80.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QColor color(int part) const;

    TOutfitsMountsSummonSkinsModel* m_model;
    Colors m_colors;
};

/// CIP: one account badge from the badges answer (id + name).
struct AccountBadgeEntry
{
    uint32_t id = 0;
    QString name;
};

/// CIP: TAccountBadgesModel (roles 0x100d80cb0, data 0x100d80e00).
class TAccountBadgesModel final : public QAbstractListModel
{
public:
    enum Role
    {
        BadgeID = 0x101,
        BadgeName,
        BadgeIcon,
    };

    TAccountBadgesModel(std::vector<AccountBadgeEntry> entries, QObject* parent);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<AccountBadgeEntry> m_entries;
};

/// CIP: one 0x48-byte achievement row (achievement storage +0x10, added by
/// 0x100a8da90; static rows from 0x100a8dc00).
struct AchievementEntry
{
    uint16_t id = 0;
    uint32_t timestamp = 0;
    bool secret = false;
    QString name;
    QString description;
    uint8_t grade = 0;
};

/// CIP: TQmlAchievement (ctor 0x100d15d40).
class TQmlAchievement final : public cip::tibia::gamewindow::TQmlAchievement
{
public:
    TQmlAchievement(const AchievementEntry& entry, QObject* parent);
};

/// CIP: TAchievementInfoController (metacall 0x10080b090, vtable
/// 0x10262e068); persists across the dialog's answers.
class TAchievementInfoController final : public cip::tibia::gamewindow::TAchievementInfoController
{
public:
    using cip::tibia::gamewindow::TAchievementInfoController::TAchievementInfoController;

    /// CIP 0x100a52510 case 5.
    void setData(int points, int totalSecret, std::vector<AchievementEntry> unlocked);

    /// CIP 0x100d15c70 / 0x100d15ca0.
    void setSortingCriterion(EAchievmentSortingCriteria v) override;
    void setFilterCriterion(EAchievmentFilterCriteria v) override;

private:
    /// CIP 0x100d15390.
    void updateCounts();
    /// CIP 0x100d147d0.
    void rebuildList();

    std::vector<AchievementEntry> m_unlocked;
};

}
