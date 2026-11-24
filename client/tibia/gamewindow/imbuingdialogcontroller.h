#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "imbuing/imbuingstorage.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace appearances {
class TAppearanceInstanceFactory;
}
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

}
}

namespace tibia {
namespace imbuing {

/// The imbuements to choose from, roles imbuementId, imageSource and category.
class TAvailableImbuementsListModel final : public cip::tibia::imbuing::TAvailableImbuementsListModel
{
    Q_OBJECT
public:
    using cip::tibia::imbuing::TAvailableImbuementsListModel::TAvailableImbuementsListModel;

    void setImbuements(std::vector<TImbuement> imbuements);
    const std::vector<TImbuement>& imbuements() const { return m_imbuements; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<TImbuement> m_imbuements;
};

/// The item's slots, roles slotIndex, imbuementId, imageSource and category.
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

/// One resource an imbuement needs: how many it takes and how many the player
/// owns.
class TQmlAstralSource final : public cip::tibia::imbuing::TQmlAstralSource
{
    Q_OBJECT
public:
    TQmlAstralSource(const QString& name, uint needed, uint owned, uint objectTypeId, QObject* parent = nullptr);
};

/// The selected imbuement; owns its astral sources.
class TQmlImbuementData final : public cip::tibia::imbuing::TQmlImbuementData
{
    Q_OBJECT
public:
    TQmlImbuementData(const TImbuement& imbuement, const QList<QObject*>& astralSources, bool enoughAstralSources, QObject* parent = nullptr);
};

/// The imbuement in the selected slot.
class TQmlExistingImbuement final : public cip::tibia::imbuing::TQmlExistingImbuement
{
    Q_OBJECT
public:
    TQmlExistingImbuement(uint32_t imbuementId, const QString& name, uint64_t remainingDuration, uint64_t totalDuration,
                          uint32_t clearingGoldCost, QObject* parent = nullptr);
};

}
}

namespace tibia {
namespace gamewindow {

/// ImbuingDialog.qml. Opens when the server sends the imbuing window (using an
/// imbuing shrine).
class TImbuingDialogController final : public TDialogController<cip::tibia::gamewindow::TImbuingDialogController>
{
    Q_OBJECT
public:
    /// CIP: made with the appearance instance factory, the client options, the
    /// engine and the dialog parent item; its three message dialogs (result,
    /// imbue and clear confirmation) use the same engine and parent item.
    TImbuingDialogController(std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
                             config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                             QObject* parent = nullptr);

    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void connectToPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);
    bool clientSettingSmoothFiltering() const override;

    bool itemSelectionValid() const override;
    bool scrollIsAvailable() const override;
    bool imbueModeItem() const override;
    bool imbueModeScroll() const override;
    bool isPremium() const override;
    QString imbuementFilter() const override;
    void setImbuementFilter(const QString& v) override;
    /// The imbued item's (or the blank scroll's) name, each word capitalised.
    QString selectedItemText() const override;

public Q_SLOTS:
    void showDialog() override;
    /// Closes the confirmations too, tells the server and forgets the item.
    void closeDialog() override;
    void requestClose() override;
    /// Asks to confirm imbuing the selected imbuement for its gold.
    void requestImbue() override;
    /// Asks to confirm clearing the selected slot for its gold.
    void requestClearImbuement() override;
    void requestOpenStoreWithPremiumCategory() override;
    /// The premium store page, then closes.
    void openPremiumStore() override;
    void handleSelectImbuementSlot(uint SlotIndex) override;
    void handleSelectImbuement(uint ImbuementId) override;
    void handlePickFilterBasic() override;
    void handlePickFilterIntricate() override;
    void handlePickFilterPowerful() override;
    /// Hides the dialog while the player picks the item to imbue.
    void handlePickItemToImbue() override;
    void handleImbueScroll() override;

private:
    void onImbuingStorageDataChanged();
    void onDisplayResultMessage(const QString& message);
    void onImbueSlotConfirmed();
    void onClearSlotConfirmed();
    void setFilter(uint32_t filter);
    const imbuing::TImbuementSlot* selectedSlot() const;
    /// The imbued item from the storage; resets the slot index the item does
    /// not have.
    void updateImbuedItem();
    /// The selected slot's imbuement and the slot list.
    void updateExistingImbuement();
    /// The imbuements of the current filter, ordered for the list.
    void updateAvailableImbuements();
    /// `imbuementId` becomes the selected imbuement, with its astral sources.
    void selectImbuement(uint32_t imbuementId);
    void replaceObject(QObject*& member, QObject* object);

    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_pAppearanceInstanceFactory;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<imbuing::TImbuingStorage> m_storage;
    shared::TQtConnectionManager m_gameSessionConnections;
    imbuing::TAvailableImbuementsListModel* m_available;
    imbuing::TImbuementSlotsListModel* m_slotsModel;
    TMessageDialogController* m_resultMessage;
    TMessageDialogController* m_imbueConfirm;
    TMessageDialogController* m_clearConfirm;
    bool m_picking = false;
    /// The imbuement tier listed: 0 basic, 1 intricate, 2 powerful.
    uint32_t m_filter = 0;
};

}
}
