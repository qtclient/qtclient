#pragma once

#include "input/gameaction.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia::store {

/// CIP: TStoreGameActionHandler. Opens the store (OPEN_STORE and
/// BUY_HIRELING_NAME_OR_SEX_CHANGE) and asks for a transaction's details
/// (REQUEST_TRANSACTION_DETAILS) through the store protocol.
class TStoreGameActionHandler final : public cip::tibia::store::TStoreGameActionHandler
{
    Q_OBJECT
public:
    explicit TStoreGameActionHandler(std::shared_ptr<TStoreProtocolMessageHandler> storeProtocolMessageHandler);

public Q_SLOTS:
    /// The router's game actions (connected to publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void handleOpenStoreGameAction(const ::tibia::input::TGameActionOpenStore& pGameAction);
    void handleBuyHirelingNameOrSexChangeAction(const ::tibia::input::TGameActionBuyHirelingNameOrSexChange& pGameAction);
    void handleRequestTransactionDetailsGameAction(const ::tibia::input::TGameActionRequestTransactionDetailsFromServer& pGameAction);

private:
    std::shared_ptr<TStoreProtocolMessageHandler> m_storeProtocolMessageHandler;
};

}
