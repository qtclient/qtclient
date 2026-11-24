#pragma once

#include "skillwheel/skillwheelstorage.h"

#include <cip_gen.h>
#include "paginationproxymodel.h"
#include "shared/simplevectorabstractlistmodel.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

namespace tibia {
namespace skillwheel {

/// The skill wheel's pages: CIP's shared::TPaginationProxyModel with a
/// helper that turns to a source row's page.
class TPaginationProxyModel final : public shared::TPaginationProxyModel
{
    Q_OBJECT
public:
    TPaginationProxyModel(int entriesPerPage, QObject* parent);

    /// The page holding a source row, then its index in this model.
    QModelIndex showSourceRow(int sourceRow);
};

/// The revealed gems of the atelier (CIP: tibia::skillwheel::TInventoryGemListModel):
/// the inventory, and the configurations and mod grades the texts are computed with.
class TInventoryGemListModel final : public cip::tibia::skillwheel::TInventoryGemListModel
{
    Q_OBJECT
public:
    enum ERole {
        GemImageSource = 0x101, GemName, Quality,
        Mod1Id, Mod1ImageSource, Mod1EffectString, Mod1Grade,
        Mod2Id, Mod2ImageSource, Mod2EffectString, Mod2Grade,
        Mod3Id, Mod3ImageSource, Mod3EffectString, Mod3Grade,
        IsSocketed, GemId, CanBeDismantled, IsLocked,
    };

    explicit TInventoryGemListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setGems(const std::vector<TGem>& gems);
    /// `configuration` is the shown wheel (texts, socketed gems); a gem in `serverConfiguration`
    /// cannot be dismantled.
    void setConfiguration(const std::optional<TWheel>& configuration, const std::optional<TWheel>& serverConfiguration,
                          const TGrades& basicGrades, const TGrades& supremeGrades);
    const TGem* gem(int row) const;
    int rowOfGem(int gemId) const;
    /// The gem's role values by role name; empty when no gem has that id.
    QVariantMap dataForId(int gemId) const;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;
    bool hasConfiguration() const { return m_configuration.has_value(); }
    /// The configuration's vocation index, 0 without one.
    int vocationIndex() const;
    int basicGrade(int mod) const;
    int supremeGrade(int mod) const;

private:
    std::vector<TGem> m_gems;
    std::optional<TWheel> m_configuration;
    std::optional<TWheel> m_serverConfiguration;
    TGrades m_basicGrades;
    TGrades m_supremeGrades;
};

/// CIP : the mod text, quality and locked filters.
class TInventoryGemSortFilterProxyModel final : public cip::tibia::skillwheel::TInventoryGemSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TInventoryGemSortFilterProxyModel(TInventoryGemListModel* source, QObject* parent = nullptr);

    void setFilterModName(const QString& v) override;
    void setFilterGemQuality(int v) override;
    void setShowLockedOnly(bool v) override;
    bool accepts(const TGem& gem) const;
    /// The row showing the gem, invalid when the filters hide it.
    QModelIndex indexOfGem(int gemId) const;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;
    /// Runs the filters again over every row (no filterChanged).
    void invalidateRows();
    /// No mod text, every quality (-1), locked and unlocked gems; then filterChanged.
    void resetFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void refilter();

    TInventoryGemListModel* m_source;
};

/// The fragment workshop mods (CIP: tibia::skillwheel::TModListModel): the storage's
/// mod list, and the configuration and mod grades the texts are computed with.
class TModListModel final : public cip::tibia::skillwheel::TModListModel
{
    Q_OBJECT
public:
    enum ERole {
        ModImageSource = 0x101, ModId, IsBasicMod, IsSocketed, Grade, AmountOnGemsInAtelier, ModName, AugmentName,
        Grade1Bonus, Grade2Bonus, Grade3Bonus, Grade4Bonus,
    };

    explicit TModListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;

    void setMods(const std::vector<TModListEntry>& mods);
    void setConfiguration(const TWheel& configuration, const TGrades& basicGrades, const TGrades& supremeGrades);
    const TModListEntry* entry(int row) const;
    /// The configuration's vocation index, 0 without one.
    int vocationIndex() const;
    int basicGrade(int mod) const;
    int supremeGrade(int mod) const;

private:
    std::vector<TModListEntry> m_mods;
    std::optional<TWheel> m_configuration;
    TGrades m_basicGrades;
    TGrades m_supremeGrades;
};

/// CIP: tibia::skillwheel::TModListSortFilterProxyModel: the mod name filter and the
/// workshop's mod filter (0 all, 1 basic, 2 supreme, 3 in a vessel, 4..7 grade I..IV).
class TModListSortFilterProxyModel final : public cip::tibia::skillwheel::TModListSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int filterMods READ filterMods WRITE setFilterMods NOTIFY filterChanged FINAL)
public:
    explicit TModListSortFilterProxyModel(TModListModel* source, QObject* parent = nullptr);

    void setFilterModName(const QString& v) override;
    int filterMods() const { return m_filterMods; }
    void setFilterMods(int v);
    /// No mod text, every mod (0); then filterChanged.
    void resetFilters();
    Q_INVOKABLE QVariantMap rowData(const QModelIndex& Index) override;
    /// CIP: the first shown row of the supreme / basic mod, an invalid index without one.
    QModelIndex indexOfSupremeMod(int modId) const;
    QModelIndex indexOfBasicMod(int modId) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TModListModel* m_source;
    int m_filterMods = 0;
};

/// One summary line (roles label, effekt, additionalInfo, imageSource, isSeparator).
/// CIP TPerkInfo: a summary row (a separator row has no texts).
struct TPerkInfo
{
    QString label;
    QString effect;
    QString additionalInfo;
    QUrl imageSource;
    bool isSeparator = false;
};

/// CIP: the summary lists ("label", "effekt", "additionalInfo", "isSeparator", "imageSource").
class TPerkInfoModel final : public shared::TSimpleVectorAbstractListModel<TPerkInfo, cip::TPerkInfoModel>
{
    Q_OBJECT
public:
    enum ERole { Label = Qt::UserRole + 1, Effect, AdditionalInfo, IsSeparator, ImageSource };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant itemData(const TPerkInfo& item, int role) const override;
};

/// CIP TPresetInfo: a preset row.
struct TPresetInfo
{
    QString name;
    TWheel wheel;
};

/// CIP: the presets table ("name", "availablePoints") over the page's sorted presets; the available
/// points are the character's total minus the preset's spent points.
class TPresetInfoModel final : public shared::TSimpleVectorAbstractListModel<TPresetInfo, cip::TPresetInfoModel>
{
    Q_OBJECT
public:
    enum ERole { Name = Qt::UserRole + 1, AvailablePoints };

    explicit TPresetInfoModel(QObject* parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;

    /// CIP moc property getter.
    int nameEnumValue() const override;

    int totalPoints() const { return m_totalPoints; }
    /// A changed total reports every row changed.
    void setTotalPoints(int points);

protected:
    QVariant itemData(const TPresetInfo& item, int role) const override;

private:
    int m_totalPoints = 0;
};

}
}
