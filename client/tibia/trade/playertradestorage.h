#pragma once

#include "container/containerciptypes.h"

#include <cip_gen.h>

#include <QString>

#include <vector>

namespace tibia {
namespace trade {

/// The objects of one side of a player trade.
using TPlayerTradeOffer = std::vector<container::TInventorySlot>;

/// The trade between two players: both names and both offers. Filled by
/// TPlayerTradeProtocolMessageHandler.
class TPlayerTradeStorage final : public cip::tibia::trade::TPlayerTradeStorage
{
    Q_OBJECT
public:
    explicit TPlayerTradeStorage(QObject* parent = nullptr);

    /// Forgets both sides without notifying.
    void reset();
    const QString& ownName() const { return m_ownName; }
    const TPlayerTradeOffer& ownOffer() const { return m_ownOffer; }
    /// A new own offer starts a new trade: the counter offer is forgotten.
    void setOwnOffer(const QString& name, const TPlayerTradeOffer& offer);
    const QString& counterName() const { return m_counterName; }
    const TPlayerTradeOffer& counterOffer() const { return m_counterOffer; }
    void setCounterOffer(const QString& name, const TPlayerTradeOffer& offer);

private:
    QString m_ownName;
    QString m_counterName;
    TPlayerTradeOffer m_ownOffer;
    TPlayerTradeOffer m_counterOffer;
};

}
}
