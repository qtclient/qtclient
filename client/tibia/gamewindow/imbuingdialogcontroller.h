#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "imbuing/imbuingstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

}
}

namespace tibia {
namespace imbuing {

/// The imbuements to choose from (CIP: imbuing::TAvailableImbuementsListModel,
/// data 's roles imbuementId, imageSource, category).
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
}

namespace tibia {
namespace gamewindow {

/// ImbuingDialog.qml (CIP: gamewindow::TImbuingDialogController, ctor
/// , metacall). Opens when the server sends the
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
    /// CIP .
    QString imbuementFilter() const override;
    /// CIP .
    void setImbuementFilter(const QString& v) override;
    /// CIP .
    QString selectedItemText() const override;

public Q_SLOTS:
    /// CIP .
    void showDialog() override;
    /// CIP .
    void closeDialog() override;
    void requestClose() override;
    /// CIP .
    void requestImbue() override;
    /// CIP .
    void requestClearImbuement() override;
    /// CIP .
    void requestOpenStoreWithPremiumCategory() override;
    /// CIP .
    void openPremiumStore() override;
    /// CIP .
    void handleSelectImbuementSlot(uint SlotIndex) override;
    /// CIP .
    void handleSelectImbuement(uint ImbuementId) override;
    /// CIP /.
    void handlePickFilterBasic() override;
    void handlePickFilterIntricate() override;
    void handlePickFilterPowerful() override;
    /// CIP (and handleGameAction).
    void handlePickItemToImbue() override;
    /// CIP .
    void handleImbueScroll() override;

private:
    /// CIP .
    void onImbuingStorageDataChanged();
    /// CIP .
    void onDisplayResultMessage(const QString& message);
    /// CIP.
    void onImbueSlotConfirmed();
    void onClearSlotConfirmed();
    void setFilter(int filter);
    const imbuing::TImbuementSlot* selectedSlot() const;
    /// CIP .
    void updateItem();
    /// CIP .
    void updateExisting();
    /// CIP .
    void updateAvailable();
    /// CIP .
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
}
