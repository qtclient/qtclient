#pragma once

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QColor>
#include <QString>

#include <array>
#include <cstdint>
#include <vector>

struct CyclopediaCharacterItemSummary;

namespace tibia {
namespace gamewindow {

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

/// CIP case 6: the five wire lists merged into one row per
/// (type, tier), kept sorted by type then tier .
std::vector<ItemSummaryEntry> mergeItemSummary(const CyclopediaCharacterItemSummary& data);

/// CIP: TItemSummaryModel (vtable , roles , data
/// ).
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

/// CIP: TItemSummaryModelSortFilterProxy (ctor , vtable
/// ). Only the inventory is shown at first.
class TItemSummaryModelSortFilterProxy final : public cip::tibia::gamewindow::TItemSummaryModelSortFilterProxy
{
public:
    TItemSummaryModelSortFilterProxy(std::vector<ItemSummaryEntry> entries, QObject* parent);

    /// CIP ...
    void setShowInventory(bool v) override;
    void setShowStash(bool v) override;
    void setShowDepot(bool v) override;
    void setShowInbox(bool v) override;
    void setShowStoreInbox(bool v) override;
    /// CIP .
    void updateNameFilter(const QString& NameFilter) override;

protected:
    /// CIP .
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP .
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

/// CIP: TTitlesModel (roles).
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
    /// CIP .
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP : case-sensitive.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::vector<TitleEntry> m_entries;
};

/// CIP: TCharacterTitlesInfoQmlData (ctor , current title lookup
/// , hasCurrentTitle).
class TCharacterTitlesInfoQmlData final : public cip::tibia::gamewindow::TCharacterTitlesInfoQmlData
{
public:
    TCharacterTitlesInfoQmlData(uint8_t currentTitleId, std::vector<TitleEntry> entries, QObject* parent);
};

/// CIP: one boost::variant<PlayerOutfit, Mount, SummonSkin> row (0x38 bytes,
/// built by in outfit, mount, summon skin order).
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

/// CIP: TOutfitsMountsSummonSkinsModel (roles , data).
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
/// , metacall). The colours are the outfit's (+0x40)
/// or the mount's (+0x44) by the type filter.
class TOutfitMountSummonSkinsListDataModelSortFilterProxy final
    : public cip::tibia::gamewindow::TOutfitMountSummonSkinsListDataModelSortFilterProxy
{
public:
    /// Head, torso, legs, detail of the outfit, then of the mount.
    using Colors = std::array<uint8_t, 8>;

    TOutfitMountSummonSkinsListDataModelSortFilterProxy(std::vector<AppearanceEntry> entries, const Colors& colors, QObject* parent);

    /// CIP .
    void setTypeFilter(EQmlOutfitMountTypeFilter v) override;
    /// CIP .
    void setSourceFilter(EQmlOutfitMountSourceFilter v) override;
    /// CIP ...
    QColor headColor() const override { return color(0); }
    QColor torsoColor() const override { return color(1); }
    QColor legsColor() const override { return color(2); }
    QColor detailColor() const override { return color(3); }

protected:
    /// CIP .
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

/// CIP: TAccountBadgesModel (roles , data).
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
/// ; static rows from).
struct AchievementEntry
{
    uint16_t id = 0;
    uint32_t timestamp = 0;
    bool secret = false;
    QString name;
    QString description;
    uint8_t grade = 0;
};

/// CIP: TQmlAchievement (ctor).
class TQmlAchievement final : public cip::tibia::gamewindow::TQmlAchievement
{
public:
    TQmlAchievement(const AchievementEntry& entry, QObject* parent);
};

/// CIP: TAchievementInfoController (metacall , vtable
/// ); persists across the dialog's answers.
class TAchievementInfoController final : public cip::tibia::gamewindow::TAchievementInfoController
{
public:
    using cip::tibia::gamewindow::TAchievementInfoController::TAchievementInfoController;

    /// CIP case 5.
    void setData(int points, int totalSecret, std::vector<AchievementEntry> unlocked);

    /// CIP.
    void setSortingCriterion(EAchievmentSortingCriteria v) override;
    void setFilterCriterion(EAchievmentFilterCriteria v) override;

private:
    /// CIP .
    void updateCounts();
    /// CIP .
    void rebuildList();

    std::vector<AchievementEntry> m_unlocked;
};

}
}
