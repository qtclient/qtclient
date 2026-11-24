#include "gamewindow/marketcointransactiondetailsdialogcontroller.h"

#include "gamewindow/gamewindowciptypes.h"
#include "qmlcomponents/texthelper.h"

#include <utility>

namespace tibia::gamewindow {

TMarketCoinTransactionDetailsDialogController::TMarketCoinTransactionDetailsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MarketCoinTransactionDetails.qml"), parent)
{}

TMarketCoinTransactionDetailsDialogController::~TMarketCoinTransactionDetailsDialogController()
{
    // The dialog closes first; that also lets go of the child transactions.
    closeDialog();
}

void TMarketCoinTransactionDetailsDialogController::setTransactionDetails(const store::TTransactionDetails& details)
{
    m_details = details;
    updateChildTransactionList();
    if (!m_transactionLoaded) {
        m_transactionLoaded = true;
        Q_EMIT transactionLoadedChanged();
    }
}

void TMarketCoinTransactionDetailsDialogController::updateChildTransactionList()
{
    const QList<QObject*> old = std::exchange(m_multiTransactionChildTransactionList, {});
    for (const store::TChildTransactionDetails& child : m_details.multi.childTransactions)
        m_multiTransactionChildTransactionList.append(new TQmlChildTransaction(child));
    Q_EMIT dataChanged();
    for (QObject* object : old) {
        if (object)
            object->deleteLater();
    }
}

TMarketCoinTransactionDetailsDialogController::EMarketTransactionType TMarketCoinTransactionDetailsDialogController::transactionType() const
{
    return m_details.type == store::TTransactionDetails::TYPE_MULTI ? MultiTransaction : SingleTransaction;
}

QString TMarketCoinTransactionDetailsDialogController::singleTransactionDate() const
{
    return qmlcomponents::TTextHelperQmlService().formatDateTime(static_cast<int>(m_details.single.date), true);
}

QString TMarketCoinTransactionDetailsDialogController::singleTransactionDescription() const { return m_details.single.description; }
QString TMarketCoinTransactionDetailsDialogController::singleTransactionCharacterName() const { return m_details.single.characterName; }
qlonglong TMarketCoinTransactionDetailsDialogController::singleTransactionCoinBalanceChange() const { return m_details.single.coinBalanceChange; }
qlonglong TMarketCoinTransactionDetailsDialogController::singleTransactionPiecePrice() const
{
    return static_cast<qlonglong>(m_details.single.piecePrice);
}
qlonglong TMarketCoinTransactionDetailsDialogController::singleTransactionGoldBalanceChange() const { return m_details.single.goldBalanceChange; }
QString TMarketCoinTransactionDetailsDialogController::multiTransactionCharacterName() const { return m_details.multi.characterName; }
int TMarketCoinTransactionDetailsDialogController::multiTransactionTotalCoins() const { return static_cast<int>(m_details.multi.totalCoins); }
int TMarketCoinTransactionDetailsDialogController::multiTransactionSoldCoins() const { return static_cast<int>(m_details.multi.soldCoins); }
int TMarketCoinTransactionDetailsDialogController::multiTransactionCoinsReainingInMarket() const
{
    return static_cast<int>(m_details.multi.coinsRemainingInMarket);
}
qlonglong TMarketCoinTransactionDetailsDialogController::multiTransactionPiecePrice() const
{
    return static_cast<qlonglong>(m_details.multi.piecePrice);
}
qlonglong TMarketCoinTransactionDetailsDialogController::multiTransactionReceivedGold() const
{
    return static_cast<qlonglong>(m_details.multi.receivedGold);
}
QList<QObject*> TMarketCoinTransactionDetailsDialogController::multiTransactionChildTransactionList() const
{
    return m_multiTransactionChildTransactionList;
}

void TMarketCoinTransactionDetailsDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TMarketCoinTransactionDetailsDialogController::closeDialog()
{
    const QList<QObject*> old = std::exchange(m_multiTransactionChildTransactionList, {});
    Q_EMIT dataChanged();
    dialog()->close();
    Q_EMIT dialogClosed();
    m_transactionLoaded = false;
    for (QObject* object : old) {
        if (object)
            object->deleteLater();
    }
}

void TMarketCoinTransactionDetailsDialogController::requestClose() { closeDialog(); }

}
