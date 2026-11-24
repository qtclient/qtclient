#include "store/storeciptypes.h"

namespace tibia::store {

void TStoreDescriptionManager::setMessageHandler(TStoreProtocolMessageHandler* handler)
{
    for (const QMetaObject::Connection& connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    m_handler = handler;
    if (handler)
        m_connections.push_back(connect(handler, &TStoreProtocolMessageHandler::storeOfferDescriptionReceived, this,
                                        &TStoreDescriptionManager::onOfferDescriptionReceived));
}

void TStoreDescriptionManager::clear()
{
    m_descriptions.clear();
}

QString TStoreDescriptionManager::description(const TOfferID offerId)
{
    const auto it = m_descriptions.constFind(offerId);
    if (it != m_descriptions.cend())
        return it.value();
    if (m_handler)
        m_handler->requestOfferDescription(offerId);
    return qtTrId("store_description_loading");
}

void TStoreDescriptionManager::onOfferDescriptionReceived(const int offerId, const QString& description)
{
    m_descriptions[static_cast<TOfferID>(offerId)] = description;
    Q_EMIT offerDescriptionLoaded(offerId);
}

} // namespace tibia::store
