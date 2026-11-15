#pragma once

// CIP-named store surfaces (promoted from cipwire).

#include "shared/cip_typed.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QHash>
#include <QObject>
#include <QPointer>

#include <vector>

namespace tibia {
namespace store {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// The offer descriptions the server sent, by offer id (CIP:
/// store::TStoreDescriptionManager). A description not known yet is asked
/// for every time it is read.
class TStoreDescriptionManager final : public cip::tibia::store::TStoreDescriptionManager
{
    Q_OBJECT
public:
    using cip::tibia::store::TStoreDescriptionManager::TStoreDescriptionManager;

    /// The handler that asks for descriptions and whose description messages
    /// fill this (CIP: the game session the manager is set to).
    void setMessageHandler(TStoreProtocolMessageHandler* handler);
    /// Forgets every description (the store closed).
    void clear();
    /// The description of `offerId`; when not known yet, asks the server for
    /// it and gives "loading".
    QString description(TOfferID offerId);

public Q_SLOTS:
    void onOfferDescriptionReceived(int offerId, const QString& description);

Q_SIGNALS:
    void offerDescriptionLoaded(int offerId);

private:
    QPointer<TStoreProtocolMessageHandler> m_handler;
    std::vector<QMetaObject::Connection> m_connections;
    QHash<TOfferID, QString> m_descriptions;
};

class TStoreGameActionHandler final : public cip::tibia::store::TStoreGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::store::TStoreGameActionHandler::TStoreGameActionHandler;
};

}
}
