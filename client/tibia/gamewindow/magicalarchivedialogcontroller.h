#pragma once

#include <cip_gen.h>

#include <QSortFilterProxyModel>
#include <QVariant>
#include <QJSValue>

#include <set>
#include <vector>

namespace tibia::gamewindow {

/// Full Magical Archive spell rows (CIP: TSpellListDataModel::data 0x100c94290).
class TMagicalArchiveSpellListModel final : public cip::tibia::gamewindow::TSpellListDataModel
{
    Q_OBJECT
public:
    enum Role
    {
        EnumIdRole = 0x101,
        NameRole,
        FormulaRawRole,
        ParameterHintsRole,
        IconUrlRole,
        GroupPrimaryRole,
        GroupPrimaryIconUrlRole,
        GroupSecondaryRole,
        DamagetypeRole,
        MinimumCasterLevelRole,
        DescriptionRole,
        SourceStringRole,
        MinimumMagicLevelRole,
        MeanRole,
        IsRuneRole,
        RuneAmountRole,
        IsPremiumRole,
        PremiumOnlyRole,
        GroupsRole,
        ManaCostRole,
        SoulPointsCostRole,
        CooldownSelfRole,
        CooldownPrimaryGroupRole,
        CooldownSecondaryGroupRole,
        GroupCooldownsRole,
        CastPossibleRole,
        ScalingRole,
        SpellUsageInfoRole,
        RuneUsageInfoRole,
        RelevantUsageInfoRole,
        SourceRole,
        IsAimAtTargetConfigurableRole,
        IsCrossHairSpellRole,
        AllowedVocationsRole,
        IconUrlSmallRole,
        IconIndexRole,
    };

    explicit TMagicalArchiveSpellListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    /// Re-read TSpellInfoStorage (safe if the table loaded after first construction).
    void reload();

private:
    void rebuildRows();

    std::vector<int> m_rows;
};

/// CIP: TSpellListDataModelSortFilterProxy (filter 0x100c97380, show reset
/// 0x100c98d90).
class TSpellListDataModelSortFilterProxy final : public cip::tibia::gamewindow::TSpellListDataModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TSpellListDataModelSortFilterProxy(QObject* parent = nullptr);

    /// CIP 0x100c98d90.
    void resetFilter() override;
    QVariant getModelDataForRow(int RowIndex) override;
    QVariant getDataWithRoleName(int RowIndex, const QByteArray& RoleName) override;
    void onPlayerDataChanged() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void syncPlayer();

    int m_playerLevel = 0;
    int m_playerVocationBase = -1;
    std::set<int> m_knownSpells;
};

/// Magical Archive tab (CIP: TMagicalArchiveDialogController, session
/// 0x100e1fe70, show 0x100e20520).
class TMagicalArchiveDialogController final : public cip::tibia::gamewindow::TMagicalArchiveDialogController
{
    Q_OBJECT
public:
    explicit TMagicalArchiveDialogController(QObject* parent = nullptr);

    QObject* spellListModel() const override { return m_proxy; }
    QJSValue spellsPreviewsJson() const override;
    void setSelectedSpellID(int v) override;

    void showDialogTab() override;
    void hideDialogTab() override;
    void requestAssignSpellToActionButton() override;
    void requestAimAtTargetOptionChange(bool Value) override;

private:
    void refreshSelectionFlags();

    TMagicalArchiveSpellListModel* m_model;
    TSpellListDataModelSortFilterProxy* m_proxy;
    mutable QVariant m_previewsVariant;
    mutable QJSValue m_previewsJs;
};

}
