#pragma once

#include "client/gamecore.h"
#include "container/containerciptypes.h"

#include <cip_gen.h>

#include <QString>

#include <vector>

namespace tibia {
namespace trade {

/// The trade between two players (CIP: TPlayerTradeStorage, ctor
/// , vtable) fed by CIP's OwnTrade /
/// CounterTrade / CloseTrade (CIP's TPlayerTradeProtocolMessageHandler,
/// ctor , vtable).
class TPlayerTradeStorage final : public cip::tibia::trade::TPlayerTradeStorage
{
    Q_OBJECT
public:
    explicit TPlayerTradeStorage(QObject* parent = nullptr);

    bool isOpen() const { return m_open; }
    /// v0x68 / v0x70.
    const QString& ownName() const { return m_ownName; }
    const std::vector<container::TInventorySlot>& ownOffer() const { return m_ownOffer; }
    /// v0x80 / v0x88.
    const QString& counterName() const { return m_counterName; }
    const std::vector<container::TInventorySlot>& counterOffer() const { return m_counterOffer; }

    /// CIP / (rejecting also forgets the trade) /
    /// .
    void accept() const;
    void reject();
    void look(bool counterOffer, int index) const;

Q_SIGNALS:
    /// CIP TPlayerTradeProtocolMessageHandler::requestOpenPlayerTradeWidget /
    /// requestClosePlayerTradeWidget.
    void requestOpenPlayerTradeWidget();
    void requestClosePlayerTradeWidget();

private:
    /// v0x60 (CIP).
    void reset();
    void close();

    bool m_open = false;
    QString m_ownName;
    std::vector<container::TInventorySlot> m_ownOffer;
    QString m_counterName;
    std::vector<container::TInventorySlot> m_counterOffer;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
