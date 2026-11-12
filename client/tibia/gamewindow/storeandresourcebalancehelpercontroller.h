#pragma once

#include "client/gamecore.h"
#include "exaltation/exaltationstorage.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace gamewindow {

/// The `storeAndResourceBalanceHelper` context property (CIP:
/// TStoreAndResourceBalanceHelperController, ctor , metacall
/// ): the gold, coins and other currencies the dialogs show. Read
/// from CIP's resource balances (guild bank = ResourceBalance type 0x02).
/// The dust maximum is 100 until an exaltation storage is set.
class TStoreAndResourceBalanceHelperController final : public cip::tibia::gamewindow::TStoreAndResourceBalanceHelperController
{
    Q_OBJECT
public:
    explicit TStoreAndResourceBalanceHelperController(store::TStoreStorage* storage, QObject* parent = nullptr);

    /// CIP .
    Q_INVOKABLE bool canAffordWithTotalGoldBalance(int Price) override;

    /// CIP : exaltedDustMaximum is the storage's current dust limit.
    void setExaltationStorage(exaltation::TExaltationStorage* storage);

private:
    /// CIP .
    void updateGold();
    /// CIP .
    void updateStoreBalance();
    /// CIP / / /.
    void updateResources();

    QPointer<store::TStoreStorage> m_storage;
    QPointer<exaltation::TExaltationStorage> m_exaltation;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
