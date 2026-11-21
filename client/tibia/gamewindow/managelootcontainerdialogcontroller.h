#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QSortFilterProxyModel>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia {
namespace analysers {
class TAnalyserStorages;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
}

namespace tibia {
namespace game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace quickloot {
class TManagedContainerStorage;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// One category row (CIP: TLootContainerQMLInfo, metacall).
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

/// ManageLootContainerDialog.qml (CIP ctor , metacall).
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
    /// CIP connector: the dialog closes and its list empties; then it follows
    /// the session's managed containers and the loot list.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP: the premium time the dialog's premium state follows.
    void connectToPremiumTimeStorage(game::TPremiumTimeStorage* storage);

public Q_SLOTS:
    /// CIP / handleGameAction 251–252.
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    /// CIP .
    void getPremiumClicked() override;
    /// CIP .
    void requestClearItemBlackWhitelist() override;
    /// CIP .
    void requestLootBlackWhitelistSwitch(int NewListMode) override;
    /// CIP .
    void requestAddItemBlackWhitelist() override;
    /// CIP .
    void removeItemFromList(int AppearanceTypeIDRaw) override;
    /// CIP .
    void showItemDetails(int AppearanceTypeIDRaw) override;
    void onPremiumTimeChanged() override;
    /// CIP .
    void onLootContainersUpdated() override;
    /// CIP: the loot list changed; the dialog shows it again.
    void onLootBlackWhitelisteChanged();

private:
    enum class PickMode { None, LootContainer, ObtainContainer };

    void installHandlers();
    void uninstallHandlers();
    bool handleGameAction(const input::IGameAction& action);
    void rebuildListItems();
    void applyListFilter();
    void syncPremium();
    void syncFallback();
    void startContainerPick(int category, bool lootColumn);
    void finishPick(const input::TTargetSelectedPtr& target);
    void cancelPick();
    void onClearItemBlackWhitelistConfirmed();

    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    game::TPremiumTimeStorage* m_premiumTimeStorage = nullptr;
    quickloot::TQuickLootBlackWhitelistStorage* m_lootBlackWhitelist = nullptr;
    std::shared_ptr<quickloot::TManagedContainerStorage> m_containers;
    TAppearanceTypeListModel m_listModel;
    TAppearanceTypeListModelSortFilterProxy m_listProxy;
    TMessageDialogController* m_confirm = nullptr;
    PickMode m_pick = PickMode::None;
    int m_pickCategory = 0;
    bool m_scrollOnShow = false;
    std::vector<client::gamecore::Hook> m_hooks;

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
