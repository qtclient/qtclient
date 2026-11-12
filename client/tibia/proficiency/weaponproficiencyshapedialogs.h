#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "proficiency/proficiencystorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>

#include <optional>
#include <vector>

namespace tibia {
namespace gamewindow {
class TMessageDialogController;
}
}

namespace tibia {
namespace proficiency {

/// WeaponProficiencyShapeDialog.qml (CIP: TWeaponProficiencyShapeDialogController,
/// ctor , game action OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG).
/// The action carries the weapon in `instance`, the level in `parameter`, the
/// perk in `category` and whether it comes from the perk tree in `enable`.
class TWeaponProficiencyShapeDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TWeaponProficiencyShapeDialogController>
{
    Q_OBJECT
public:
    explicit TWeaponProficiencyShapeDialogController(TProficiencyStorage* storage, QObject* parent = nullptr);
    ~TWeaponProficiencyShapeDialogController() override;

    void requestClose() override;
    void onRefineClicked() override;
    void onMaximiseClicked() override;
    void onReshapeClicked() override;
    void onClearClicked() override;
    void onShowShapeOptionsClicked() override;
    /// CIP .
    void showDialog() override;
    void closeDialog() override;
    /// CIP .
    void onMessageDialogAccept() override;
    void onResourceBalanceChanged() override;

private:
    enum class EConfirm
    {
        Refine,
        Maximise,
        Reshape,
        Clear,
    };

    /// CIP .
    void update(uint16_t typeId);
    /// CIP /.
    void updateRefine(const std::optional<TShapedSlot>& slot);
    void updateMaximise(const std::optional<TShapedSlot>& slot);
    void updateReshape(const std::optional<TShapedSlot>& slot);
    /// CIP : the shown slot as the server has it.
    std::optional<TShapedSlot> shapedSlot() const;
    void confirm(EConfirm action, const QString& caption, const QString& text);
    void sendSlotAction(uint8_t type) const;

    TProficiencyStorage* m_storage;
    gamewindow::TMessageDialogController* m_message;
    EConfirm m_pending = EConfirm::Refine;
    uint16_t m_typeId = 0;
    uint8_t m_level = 0;
    uint8_t m_perk = 0;
    bool m_fromPerkTree = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// WeaponProficiencyReshapeDialog.qml (CIP: TWeaponProficiencyReshapeDialogController,
/// ctor): the server's three offers for the shaped slot.
class TWeaponProficiencyReshapeDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TWeaponProficiencyReshapeDialogController>
{
    Q_OBJECT
public:
    TWeaponProficiencyReshapeDialogController(TProficiencyStorage* storage, TWeaponProficiencyShapeDialogController* shape,
                                              QObject* parent = nullptr);

    /// CIP : the shape dialog's perk.
    QObject* currentPerk() const override;

    /// CIP : keeps the current perk.
    void requestCancel() override;
    /// CIP .
    void replacePerkSelected(int PerkIndex) override;
    void showDialog() override;
    void closeDialog() override;
    /// CIP .
    void onMessageDialogAccept() override;

private:
    /// CIP .
    void rebuildOffers();
    void pickOffer(int index) const;

    TProficiencyStorage* m_storage;
    TWeaponProficiencyShapeDialogController* m_shape;
    gamewindow::TMessageDialogController* m_message;
    int m_selectedOffer = -1;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// The shaping catalogue's perks (CIP: the list model inside
/// TShapedPerkOptionsSortFilterProxyModel, vtable).
class TShapedPerkOptionsModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        PerkName = 0x101,
        ImageSource,
        HasAugmentEffect,
        AugmentEffectImageSource,
        AugmentSpellName,
        AugmentType,
        PerkDescriptionRank0,
        PerkDescriptionRank10,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    /// CIP .
    QVariant data(const QModelIndex& index, int role) const override;
    /// CIP .
    QHash<int, QByteArray> roleNames() const override;

    /// CIP .
    void setPerks(std::vector<TProficiencyPerk> perks);

private:
    std::vector<TProficiencyPerk> m_perks;
};

/// CIP: TShapedPerkOptionsSortFilterProxyModel (ctor).
class TShapedPerkOptionsSortFilterProxyModel final : public cip::TShapedPerkOptionsSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TShapedPerkOptionsSortFilterProxyModel(QObject* parent = nullptr);

    void setPerks(std::vector<TProficiencyPerk> perks) { m_model.setPerks(std::move(perks)); }
    void setFilterString(const QString& filter);

Q_SIGNALS:
    void filterChanged();

protected:
    /// CIP .
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP .
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TShapedPerkOptionsModel m_model;
    QString m_filter;
};

/// WeaponProficiencyShapeOptionsDialog.qml (CIP: TWeaponProficiencyShapeOptionsDialogController,
/// ctor , game action OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG with
/// the profession in `parameter`).
class TWeaponProficiencyShapeOptionsDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TWeaponProficiencyShapeOptionsDialogController>
{
    Q_OBJECT
public:
    explicit TWeaponProficiencyShapeOptionsDialogController(QObject* parent = nullptr);
    ~TWeaponProficiencyShapeOptionsDialogController() override;

    void requestClose() override;
    void setFilterString(const QString& Filter) override;
    void showDialog() override;
    void closeDialog() override;

private:
    TShapedPerkOptionsSortFilterProxyModel m_proxy;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
