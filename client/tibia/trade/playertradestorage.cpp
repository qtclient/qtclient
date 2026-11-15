#include "playertradestorage.h"

namespace tibia::trade {

TPlayerTradeStorage::TPlayerTradeStorage(QObject* parent)
    : cip::tibia::trade::TPlayerTradeStorage(parent)
{
}

void TPlayerTradeStorage::reset()
{
    m_ownName.clear();
    m_counterName.clear();
    m_ownOffer.clear();
    m_counterOffer.clear();
}

void TPlayerTradeStorage::setOwnOffer(const QString& name, const TPlayerTradeOffer& offer)
{
    reset();
    m_ownName = name;
    m_ownOffer = offer;
    Q_EMIT ownOfferChanged();
    Q_EMIT counterOfferChanged();
}

void TPlayerTradeStorage::setCounterOffer(const QString& name, const TPlayerTradeOffer& offer)
{
    m_counterName = name;
    m_counterOffer = offer;
    Q_EMIT counterOfferChanged();
}

}
