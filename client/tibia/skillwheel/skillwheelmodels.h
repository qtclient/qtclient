#pragma once

#include "skillwheel/skillwheelstorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

namespace tibia {
namespace skillwheel {

/// CIP's TPaginationProxyModel: the rows of one page (1-based) of its source.
class TPaginationProxyModel final : public cip::shared::TPaginationProxyModel
{
    Q_OBJECT
public:
    explicit TPaginationProxyModel(int entriesPerPage, QObject* parent = nullptr);

    void setSourceModel(QAbstractItemModel* source) override;
    void setEntriesPerPage(int v) override;
    void setCurrentPage(int v) override;
    QVariantMap rowData(const QModelIndex& Index) override;

    /// The page holding a source row, then its index in this model.
    QModelIndex showSourceRow(int sourceRow);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void updatePages();
};

/// The revealed gems of the atelier (CIP roles 0x101..0x11a,
/// data).
class TInventoryGemListModel final : public cip::tibia::skillwheel::TInventoryGemListModel
{
    Q_OBJECT
public:
    enum ERole {
        GemImageSource = 0x101, DomainAffinitySource, GemName,
        Mod1Id, Mod1ImageSource, Mod1EffectString, Mod1Active, Mod1Grade, Mod1PotentialGrade,
        Mod2Id, Mod2ImageSource, Mod2EffectString, Mod2Active, Mod2Grade, Mod2PotentialGrade,
        Mod3Id, Mod3ImageSource, Mod3EffectString, Mod3Active, Mod3Grade, Mod3PotentialGrade,
        IsSocketed, GemId, CanBeDismantled, IsLastOfDomainAffinity, IsLocked,
    };

    explicit TInventoryGemListModel(TSkillWheelStorage* storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset();
    void refresh();
    int rowOfGem(int gemId) const;

private:
    TSkillWheelStorage* m_storage;
};

/// CIP : the mod text, domain, quality and locked filters.
class TInventoryGemSortFilterProxyModel final : public cip::tibia::skillwheel::TInventoryGemSortFilterProxyModel
{
    Q_OBJECT

public:
    TInventoryGemSortFilterProxyModel(TSkillWheelStorage* storage, QObject* parent = nullptr);

    void setFilterModName(const QString& v) override;
    void setFilterDomainAffinity(int v);
    void setFilterGemQuality(int v) override;
    void setShowLockedOnly(bool v) override;
    bool accepts(const TGem& gem) const;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void refilter();

    TSkillWheelStorage* m_storage;
    int m_filterDomainAffinity = -1;
};

/// One mod of the fragment workshop.
struct TModEntry
{
    bool isBasic = true;
    bool isSocketed = false;
    int modId = 0;
    int amount = 0;
    int grade = 0;
};

/// The fragment workshop mods: the vocation's supreme mods, then the basic ones.
/// CIP: tibia::skillwheel::TModListModel (rowData invokable).
class TModListModel final : public cip::tibia::skillwheel::TModListModel
{
    Q_OBJECT
public:
    enum ERole {
        ModImageSource = 0x101, ModId, IsBasicMod, IsSocketed, Grade, AmountOnGemsInAtelier, ModName, AugmentName,
        Grade1Bonus, Grade2Bonus, Grade3Bonus, Grade4Bonus,
    };

    explicit TModListModel(TSkillWheelStorage* storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;

    void reset();
    const TModEntry* entry(int row) const;
    int rowOfMod(bool isBasic, int modId) const;
    QString modName(const TModEntry& entry) const;
    int vocationIndex() const { return m_storage->vocationIndex(); }

private:
    TSkillWheelStorage* m_storage;
    QList<TModEntry> m_entries;
};

/// CIP: tibia::skillwheel::TModListSortFilterProxyModel (filterChanged / filterModName).
/// Tip also shadows FragmentWorkshop `filterMods` (0 all, 1 basic, 2 supreme,
/// 3 in a vessel, 4..7 grade I..IV).
class TModListSortFilterProxyModel final : public cip::tibia::skillwheel::TModListSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int filterMods READ filterMods WRITE setFilterMods NOTIFY filterChanged FINAL)
public:
    explicit TModListSortFilterProxyModel(TModListModel* source, QObject* parent = nullptr);

    void setFilterModName(const QString& v) override;
    int filterMods() const { return m_filterMods; }
    void setFilterMods(int v);
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TModListModel* m_source;
    int m_filterMods = 0;
};

/// One summary line (roles label, effekt, additionalInfo, imageSource, isSeparator).
struct TSummaryEntry
{
    QString label;
    QString effect;
    QString additionalInfo;
    QUrl image;
    bool separator = false;
};

class TSummaryModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ERole { Label = Qt::UserRole + 1, Effect, AdditionalInfo, ImageSource, IsSeparator };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QList<TSummaryEntry> entries);
    const QList<TSummaryEntry>& entries() const { return m_entries; }

private:
    QList<TSummaryEntry> m_entries;
};

/// The presets table: "name" and "availablePoints".
class TPresetInfoModel final : public cip::TPresetInfoModel
{
    Q_OBJECT
public:
    enum ERole { Name = Qt::UserRole + 1, AvailablePoints };

    explicit TPresetInfoModel(TSkillWheelStorage* storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP moc property getter.
    int nameEnumValue() const override;

    void reset();

private:
    TSkillWheelStorage* m_storage;
};

}
}
