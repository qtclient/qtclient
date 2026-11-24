#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "proficiency/proficiencystorage.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QAbstractListModel>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace input {
class IGameAction;
namespace gameactions {
class TGameActionOpenWeaponProficiencyShapeDialog;
class TGameActionOpenWeaponProficiencyShapeOptionsDialog;
}
}
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
    /// The dialog is made with `engine` in `dialogParentItem`.
    TWeaponProficiencyShapeDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
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
    /// OPEN_WEAPON_PROFICIENCY_SHAPE_DIALOG.
    void handleGameAction(const input::gameactions::TGameActionOpenWeaponProficiencyShapeDialog& action);
    /// Closes the dialog and follows the session's proficiency data and
    /// balances; the shown weapon is refreshed.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    enum class EConfirm
    {
        Refine,
        Maximise,
        Reshape,
        Clear,
    };

    /// CIP .
    void onProficiencyDataChanged(uint16_t typeId);
    /// CIP /.
    void updateRefine(const std::optional<TShapedSlot>& slot);
    void updateMaximise(const std::optional<TShapedSlot>& slot);
    void updateReshape(const std::optional<TShapedSlot>& slot);
    /// CIP : the shown slot as the server has it.
    std::optional<TShapedSlot> shapedSlot() const;
    void resetShapeData();
    void confirm(EConfirm action, const QString& caption, const QString& text);
    void sendSlotAction(uint8_t type) const;

    TProficiencyStorage* m_storage;
    gamewindow::TMessageDialogController* m_message;
    EConfirm m_pending = EConfirm::Refine;
    uint16_t m_typeId = 0;
    uint8_t m_level = 0;
    uint8_t m_perk = 0;
    bool m_fromPerkTree = false;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

/// WeaponProficiencyReshapeDialog.qml (CIP: TWeaponProficiencyReshapeDialogController,
/// ctor): the server's three offers for the shaped slot.
class TWeaponProficiencyReshapeDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TWeaponProficiencyReshapeDialogController>
{
    Q_OBJECT
public:
    /// The dialog is made with `engine` in `dialogParentItem`; `shape` is the
    /// shape dialog whose perk is being reshaped.
    TWeaponProficiencyReshapeDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                              TWeaponProficiencyShapeDialogController* shape, QObject* parent = nullptr);
    /// Closes the confirmation and the dialog.
    ~TWeaponProficiencyReshapeDialogController() override;

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
    /// Closes the dialog and follows the session's reshape offers.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    /// The server's reshape offers: the offer list is rebuilt, the
    /// confirmation hidden and the dialog opened unless it is there.
    void onShapedPerkReshapeOffersChanged();
    /// CIP .
    void rebuildOffers();
    void pickOffer(int index) const;

    TProficiencyStorage* m_storage;
    TWeaponProficiencyShapeDialogController* m_shape;
    gamewindow::TMessageDialogController* m_message;
    int m_selectedOffer = -1;
    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
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

    /// New perks; the filter string is cleared.
    void setPerks(std::vector<TProficiencyPerk> perks);
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
    /// The dialog is made with `engine` in `dialogParentItem`.
    TWeaponProficiencyShapeOptionsDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QObject* parent = nullptr);
    ~TWeaponProficiencyShapeOptionsDialogController() override;

    void requestClose() override;
    void setFilterString(const QString& Filter) override;
    void showDialog() override;
    void closeDialog() override;
    /// OPEN_WEAPON_PROFICIENCY_SHAPE_OPTIONS_DIALOG.
    void handleGameAction(const input::gameactions::TGameActionOpenWeaponProficiencyShapeOptionsDialog& action);

private:
    TShapedPerkOptionsSortFilterProxyModel m_proxy;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
