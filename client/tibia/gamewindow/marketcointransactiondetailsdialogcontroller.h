#pragma once

#include "gamewindow/dialogmanager.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QList>
#include <QObject>
#include <QString>

namespace tibia {
namespace gamewindow {

/// MarketCoinTransactionDetails.qml (CIP: TMarketCoinTransactionDetailsDialogController): the
/// details of a Tibia Coin market transaction, a single one or one sold in parts (with one
/// TQmlChildTransaction per part).
class TMarketCoinTransactionDetailsDialogController final
    : public TDialogController<cip::tibia::gamewindow::TMarketCoinTransactionDetailsDialogController>
{
    Q_OBJECT
public:
    explicit TMarketCoinTransactionDetailsDialogController(QObject* parent = nullptr);
    ~TMarketCoinTransactionDetailsDialogController() override;

    /// Host bridge: the store dialog hands over the storage's details when they arrive.
    void setTransactionDetails(const store::TTransactionDetails& details);

    EMarketTransactionType transactionType() const override;
    QString singleTransactionDate() const override;
    QString singleTransactionDescription() const override;
    QString singleTransactionCharacterName() const override;
    qlonglong singleTransactionCoinBalanceChange() const override;
    qlonglong singleTransactionPiecePrice() const override;
    qlonglong singleTransactionGoldBalanceChange() const override;
    QString multiTransactionCharacterName() const override;
    int multiTransactionTotalCoins() const override;
    int multiTransactionSoldCoins() const override;
    int multiTransactionCoinsReainingInMarket() const override;
    qlonglong multiTransactionPiecePrice() const override;
    qlonglong multiTransactionReceivedGold() const override;
    QList<QObject*> multiTransactionChildTransactionList() const override;

public Q_SLOTS:
    void showDialog() override;
    /// Empties the child transactions and marks the transaction as not loaded.
    void closeDialog() override;
    Q_INVOKABLE void requestClose() override;

private:
    /// One TQmlChildTransaction per child transaction of the details; the old ones are deleted.
    void updateChildTransactionList();

    store::TTransactionDetails m_details;
};

}
}
