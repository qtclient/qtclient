#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QSortFilterProxyModel>

#include <cstdint>
#include <vector>

namespace tibia::cyclopedia {
class TItemInfoStorage;
}

namespace tibia::quickloot {
class TManagedContainerStorage;
}

namespace tibia::gamewindow {

class TMessageDialogController;

/// One category row (CIP: TLootContainerQMLInfo, metacall 0x10082c050).
class TLootContainerQMLInfo final : public cip::tibia::gamewindow::TLootContainerQMLInfo
{
    Q_OBJECT
    Q_PROPERTY(bool isRetrieve READ isRetrieve CONSTANT FINAL)
public:
    TLootContainerQMLInfo(int category, QObject* parent = nullptr);

    int category() const { return m_category; }
    bool isRetrieve() const { return false; }

    void setLootContainer(uint16_t typeId);
    void setObtainContainer(uint16_t typeId);

public Q_SLOTS:
    void onSelectLootContainerClicked() override;
    void onClearLootContainerClicked() override;
    void onOpenLootContainerClicked() override;
    void onSelectObtainContainerClicked() override;
    void onClearObtainContainerClicked() override;
    void onOpenObtainContainerClicked() override;

Q_SIGNALS:
    void requestSelectLootContainer(int category);
    void requestClearLootContainer(int category);
    void requestOpenLootContainer(int category);
    void requestSelectObtainContainer(int category);
    void requestClearObtainContainer(int category);
    void requestOpenObtainContainer(int category);

private:
    int m_category = 0;
};

/// ManageLootContainerDialog.qml (CIP ctor 0x100d27e70, metacall 0x10082c910).
class TManageLootContainerDialogController final
    : public TDialogController<cip::tibia::gamewindow::TManageLootContainerDialogController>
{
    Q_OBJECT
public:
    /// CIP Unassigned / market_category_default.
    static constexpr int UnassignedCategory = 31;

    explicit TManageLootContainerDialogController(QObject* parent = nullptr);
    ~TManageLootContainerDialogController() override;

    void setUseMainBackpackAsFallback(bool v) override;
    void setFilterString(const QString& v) override;

public Q_SLOTS:
    /// CIP 0x100d2bd90 / handleGameAction 251–252.
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    /// CIP 0x100d2be00.
    void getPremiumClicked() override;
    /// CIP 0x100d2bfa0.
    void requestClearItemBlackWhitelist() override;
    /// CIP 0x100d2c270.
    void requestLootBlackWhitelistSwitch(int NewListMode) override;
    /// CIP 0x100d2c360.
    void requestAddItemBlackWhitelist() override;
    /// CIP 0x100d2c460.
    void removeItemFromList(int AppearanceTypeIDRaw) override;
    /// CIP 0x100d2c540.
    void showItemDetails(int AppearanceTypeIDRaw) override;
    void onPremiumTimeChanged() override;
    /// CIP 0x100d2aa30.
    void onLootContainersUpdated() override;

private:
    enum class PickMode { None, LootContainer, ObtainContainer, ListItem };

    void installHandlers();
    void uninstallHandlers();
    bool handleGameAction(const input::TGameAction& action);
    void rebuildListItems();
    void syncPremium();
    void syncFallback();
    void startContainerPick(int category, bool lootColumn);
    void startListItemPick();
    void finishPick(const input::TGameActionPtr& target);
    void cancelPick();
    void sendContainerAction(uint8_t action, uint8_t category, const ItemPtr& item = {});
    void openAssignedContainer(uint8_t category, uint16_t typeId, uint8_t action);
    void onClearConfirmed();

    cyclopedia::TItemInfoStorage* m_itemInfo = nullptr;
    quickloot::TManagedContainerStorage* m_containers = nullptr;
    TAppearanceTypeListModel m_listModel;
    TAppearanceTypeListModelSortFilterProxy m_listProxy;
    TMessageDialogController* m_confirm = nullptr;
    PickMode m_pick = PickMode::None;
    int m_pickCategory = 0;
    bool m_scrollOnShow = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
