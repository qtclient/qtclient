#pragma once

#include "store/storestorage.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <memory>
#include <optional>

namespace tibia {
namespace game {
class IGameSession;
}
namespace skillwheel {
class TSkillWheelStorage;
}
namespace gamewindow {

/// The `storeAndResourceBalanceHelper` context property (CIP:
/// TStoreAndResourceBalanceHelperController): the gold, coins and other
/// currencies the dialogs show, each one read from the game session's storage
/// that holds it when that storage reports a change.
class TStoreAndResourceBalanceHelperController final : public cip::tibia::gamewindow::TStoreAndResourceBalanceHelperController
{
    Q_OBJECT
public:
    explicit TStoreAndResourceBalanceHelperController(QObject* parent = nullptr);

    /// The bank and the carried gold pay for it.
    Q_INVOKABLE bool canAffordWithTotalGoldBalance(int Price) override;

    /// Follows the session's storages and reads every balance (no session:
    /// nothing changes).
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);

    /// Host: CipSoft reaches the skill wheel storage through the session;
    /// qtclient's belongs to the skill wheel manager.
    void setSkillWheelStorage(skillwheel::TSkillWheelStorage* storage) { m_skillWheelStorage = storage; }

    qulonglong totalGoldBalance() const override;
    QString totalGoldBalanceString() const override;
    QString bankGoldBalanceString() const override;
    QString inventoryGoldBalanceString() const override;

public Q_SLOTS:
    void onStoreBalanceChanged();
    void onGoldBalanceChanged();
    void onPreyWildcardsChanged();
    void onPreyHuntingTaskTokensChanged();
    void onExaltedCurrencyBalanceChanged();
    void onLesserFragmentsChanged();
    void onGreaterFragmentsChanged();
    void onLunarAscensionOrbsAmountChanged();
    void onCosmeticTokenCountChanged();

private:
    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    /// The store balance the shown balance object was made from.
    std::optional<store::TStoreBalance> m_storeBalanceValue;
    skillwheel::TSkillWheelStorage* m_skillWheelStorage = nullptr;
};

}
}
