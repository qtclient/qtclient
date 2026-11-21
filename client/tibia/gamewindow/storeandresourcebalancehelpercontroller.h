#pragma once

#include "client/gamecore.h"
#include "exaltation/exaltationstorage.h"
#include "store/storestorage.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

/// The `storeAndResourceBalanceHelper` context property (CIP:
/// TStoreAndResourceBalanceHelperController, ctor , metacall
/// ): the gold, coins and other currencies the dialogs show. Read
/// from CIP's resource balances (guild bank = ResourceBalance type 0x02).
/// The store balance and the dust maximum follow the game session's store
/// and exaltation storages (none: no balance, a maximum of 100).
class TStoreAndResourceBalanceHelperController final : public cip::tibia::gamewindow::TStoreAndResourceBalanceHelperController
{
    Q_OBJECT
public:
    explicit TStoreAndResourceBalanceHelperController(QObject* parent = nullptr);

    /// CIP .
    Q_INVOKABLE bool canAffordWithTotalGoldBalance(int Price) override;

    /// The session's store and exaltation storages; the balances are read
    /// again.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    /// CIP .
    void updateGold();
    /// CIP .
    void updateStoreBalance();
    /// CIP / / /.
    void updateResources();

    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    QPointer<store::TStoreStorage> m_storage;
    QPointer<exaltation::TExaltationStorage> m_exaltation;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
