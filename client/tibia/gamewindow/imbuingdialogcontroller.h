#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "imbuing/imbuingstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

}

namespace tibia::imbuing {

/// The imbuements to choose from (CIP: imbuing::TAvailableImbuementsListModel,
/// data 0x10084f140's roles imbuementId, imageSource, category).
class TAvailableImbuementsListModel final : public cip::tibia::imbuing::TAvailableImbuementsListModel
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TAvailableImbuementsListModel::TAvailableImbuementsListModel;

    void setImbuements(std::vector<Imbuement> imbuements);
    const std::vector<Imbuement>& imbuements() const { return m_imbuements; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<Imbuement> m_imbuements;
};

/// The item's slots (CIP: imbuing::TImbuementSlotsListModel, roles
/// slotIndex, imbuementId, imageSource, category).
class TImbuementSlotsListModel final : public cip::tibia::imbuing::TImbuementSlotsListModel
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TImbuementSlotsListModel::TImbuementSlotsListModel;

    void setSlots(std::vector<TImbuementSlot> slots);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<TImbuementSlot> m_slots;
};

/// One resource an imbuement needs (CIP: imbuing::TQmlAstralSource).
class TQmlAstralSource final : public cip::tibia::imbuing::TQmlAstralSource
{
    Q_OBJECT
public:
    TQmlAstralSource(const QString& name, uint typeId, uint needed, uint owned, QObject* parent);
};

/// The selected imbuement (CIP: imbuing::TQmlImbuementData).
class TQmlImbuementData final : public cip::tibia::imbuing::TQmlImbuementData
{
    Q_OBJECT
public:
    TQmlImbuementData(const Imbuement& data, const QList<QObject*>& sources, bool enough, QObject* parent);
};

/// The selected slot's imbuement (CIP: imbuing::TQmlExistingImbuement).
class TQmlExistingImbuement final : public cip::tibia::imbuing::TQmlExistingImbuement
{
    Q_OBJECT
public:
    TQmlExistingImbuement(const TImbuementSlot& slot, QObject* parent);
};

}

namespace tibia::gamewindow {

/// ImbuingDialog.qml (CIP: gamewindow::TImbuingDialogController, ctor
/// 0x100c66bc0, metacall 0x100825e70). Opens when the server sends the
/// imbuing window (using an imbuing shrine).
class TImbuingDialogController final : public TDialogController<cip::tibia::gamewindow::TImbuingDialogController>
{
    Q_OBJECT
public:
    explicit TImbuingDialogController(imbuing::TImbuingStorage* storage, QObject* parent = nullptr);

    bool itemSelectionValid() const override;
    bool scrollIsAvailable() const override;
    bool imbueModeItem() const override;
    bool imbueModeScroll() const override;
    bool isPremium() const override;
    /// CIP 0x100c6a470.
    QString imbuementFilter() const override;
    /// CIP 0x100c6a750.
    void setImbuementFilter(const QString& v) override;
    /// CIP 0x100c6a4f0.
    QString selectedItemText() const override;

public Q_SLOTS:
    /// CIP 0x100c676d0.
    void showDialog() override;
    /// CIP 0x100c67e90.
    void closeDialog() override;
    void requestClose() override;
    /// CIP 0x100c67fa0.
    void requestImbue() override;
    /// CIP 0x100c68330.
    void requestClearImbuement() override;
    /// CIP 0x100c69ae0.
    void requestOpenStoreWithPremiumCategory() override;
    /// CIP 0x100c6a930.
    void openPremiumStore() override;
    /// CIP 0x100c68660.
    void handleSelectImbuementSlot(uint SlotIndex) override;
    /// CIP 0x100c69ac0.
    void handleSelectImbuement(uint ImbuementId) override;
    /// CIP 0x100c69a30 / 0x100c69a60 / 0x100c69a90.
    void handlePickFilterBasic() override;
    void handlePickFilterIntricate() override;
    void handlePickFilterPowerful() override;
    /// CIP 0x100c67cc0 (and handleGameAction 0x100c6a130).
    void handlePickItemToImbue() override;
    /// CIP 0x100c67e10.
    void handleImbueScroll() override;

private:
    /// CIP 0x100c677f0.
    void onImbuingStorageDataChanged();
    /// CIP 0x100c67700.
    void onDisplayResultMessage(const QString& message);
    /// CIP 0x100c671a0 / 0x100c67230.
    void onImbueSlotConfirmed();
    void onClearSlotConfirmed();
    void setFilter(int filter);
    const imbuing::TImbuementSlot* selectedSlot() const;
    /// CIP 0x100c69c90.
    void updateItem();
    /// CIP 0x100c68810.
    void updateExisting();
    /// CIP 0x100c68c50.
    void updateAvailable();
    /// CIP 0x100c691b0.
    void select(int imbuementId);
    void replaceObject(QObject*& member, QObject* object);

    imbuing::TImbuingStorage* m_storage;
    imbuing::TAvailableImbuementsListModel* m_available;
    imbuing::TImbuementSlotsListModel* m_slotsModel;
    TMessageDialogController* m_resultMessage;
    TMessageDialogController* m_imbueConfirm;
    TMessageDialogController* m_clearConfirm;
    bool m_picking = false;
    /// 0 basic, 1 intricate, 2 powerful.
    int m_filter = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
