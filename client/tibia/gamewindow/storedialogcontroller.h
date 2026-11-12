#pragma once

#include "gamewindow/dialogmanager.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>
#include <optional>
#include <utility>

namespace tibia {
namespace store {
class TDynamicallyLoadedImageManager;
class TQmlStoreOffer;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// StorePurchaseConfirmationDialog.qml (CIP:
/// TStorePurchaseConfirmationDialogController, ctor , metacall
/// ).
class TStorePurchaseConfirmationDialogController final
    : public TDialogController<cip::tibia::gamewindow::TStorePurchaseConfirmationDialogController>
{
    Q_OBJECT
public:
    explicit TStorePurchaseConfirmationDialogController(QObject* parent = nullptr);

    /// CIP : the dialog made anew for `offer`.
    void showDialog(int offerId, const store::TStoreOfferData& offer, bool showAskAgainCheckbox);
    void showDialog(int offerId, const store::TStoreOfferData& offer, bool showAskAgainCheckbox, bool buyWithCosmeticTokens);

    Q_INVOKABLE void purchaseRequested(bool AskForConfirmationAgainForOtherPurchases) override;
    Q_INVOKABLE void closeRequested() override;

Q_SIGNALS:
    void purchaseConfirmed(int offerId, bool askAgain);

private:
    QPointer<store::TQmlStoreOffer> m_offer;
};

/// StorePurchaseSuccessDialog.qml (CIP: TStorePurchaseSuccessDialogController,
/// ctor , metacall).
class TStorePurchaseSuccessDialogController final
    : public TDialogController<cip::tibia::gamewindow::TStorePurchaseSuccessDialogController>
{
    Q_OBJECT
public:
    explicit TStorePurchaseSuccessDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void requestClose() override;
    /// CIP .
    void showDialog(const QString& Message) override;
};

/// TransferCreditsDialog.qml (CIP: TTransferCreditsDialogController, show
/// , metacall).
class TTransferCreditsDialogController final : public TDialogController<cip::tibia::gamewindow::TTransferCreditsDialogController>
{
    Q_OBJECT
public:
    explicit TTransferCreditsDialogController(QObject* parent = nullptr);

    /// CIP : made anew with the transferable coins and the packet size.
    void showDialog(qint64 transferableCredits, qint64 creditPacketSize);

    /// CIP : closes, then asks for the transfer.
    Q_INVOKABLE void requestTransfer(const QString& Receipient, int TransferAmount) override;
    /// CIP .
    Q_INVOKABLE void requestCancel() override;
    /// CIP : English digit grouping.
    Q_INVOKABLE QString formatCreditNumber(int Value) override;

Q_SIGNALS:
    void transferRequested(const QString& recipient, int amount);
};

/// HirelingConfigurationDialog.qml (CIP: THirelingConfigurationDialogController,
/// show , metacall).
class THirelingConfigurationDialogController final
    : public TDialogController<cip::tibia::gamewindow::THirelingConfigurationDialogController>
{
    Q_OBJECT
public:
    explicit THirelingConfigurationDialogController(QObject* parent = nullptr);

    void showDialog(int offerId);

public Q_SLOTS:
    /// CIP .
    void requestClose() override;
    /// CIP : a name is required; closes after asking.
    void requestHireling(const QString& HirelingName, int Sex) override;

Q_SIGNALS:
    void hirelingConfigured(int offerId, const QString& name, int sex);

private:
    int m_offerId = 0;
};

class TGenericEnterTextDialogController;

/// The store, StoreDialog.qml (CIP: TStoreDialogController, ctor ,
/// metacall), on a TStoreStorage. Crystal has no protocol for
/// world transfers, main character changes, transaction details or the
/// character bazaar; sorting and filter tags stay local.
class TStoreDialogController final : public TDialogController<cip::tibia::gamewindow::TStoreDialogController>
{
    Q_OBJECT
public:
    TStoreDialogController(store::TStoreStorage* storage, store::TStoreProtocolMessageHandler* handler, QObject* parent = nullptr);
    ~TStoreDialogController() override;

    store::TStoreProtocolMessageHandler* messages() const { return m_messages; }

    /// The store button: asks the server for the store and shows it.
    void open();
    /// OPEN_STORE (CIP TStoreGameActionHandler hands the
    /// TStoreOpenParameters to TStoreProtocolMessageHandler : the
    /// categories if not loaded yet, then the offers of `openAction`): 0 home,
    /// 1 premium time / boosts (`detail` 1), 3 the useful thing `detail`,
    /// 4 the offer `detail`, 5 search (`text` is the query). Category (2)
    /// with an empty string opens home.
    void openWith(int openAction, int detail, const QString& text = {});

    void setSelectedSortOrderIndex(int index) override;
    void setSelectedFilterTagIndex(int index) override;
    void setSelectedOfferIndex(int index) override;

public Q_SLOTS:
    /// CIP .
    void showDialog() override;
    void closeDialog() override;
    void getNextFeaturedOffer() override;
    void getPreviousFeaturedOffer() override;
    void onFeaturedOfferClicked() override;

public:
    /// CIP .
    Q_INVOKABLE void requestClose() override;
    /// CIP : the server sends the offer's category.
    Q_INVOKABLE void requestJumpToOfferWithID(int QmlOfferID) override;
    /// CIP .
    Q_INVOKABLE void selectCategory(const QString& CategoryName) override;
    /// CIP .
    Q_INVOKABLE void requestSearch(const QString& SearchString) override;
    /// CIP .
    Q_INVOKABLE void openGetCoinsUrl() override;
    /// CIP .
    Q_INVOKABLE void openTransactionHistory() override;
    /// CIP.
    Q_INVOKABLE void nextTransactionPage() override;
    Q_INVOKABLE void previousTransactionPage() override;
    /// CIP .
    Q_INVOKABLE void buyOffer(int QmlOfferID) override;
    Q_INVOKABLE void buyOffer(int QmlOfferID, bool BuyWithCosmeticTokens) override;
    /// CIP : REQUEST_OUTFITS_FROM_SERVER for the selected offer's
    /// outfit (try-on type 1, mode 1) or mount (type 2, mode 3).
    Q_INVOKABLE void tryOutfitForCurrentOffer() override;
    /// CIP : the transfer dialog, or a message when fewer coins
    /// than one packet are transferable.
    Q_INVOKABLE void requestTransferCredits() override;
    /// CIP : REQUEST_TRANSACTION_DETAILS → C 0xC9 GetTransactionDetails.
    Q_INVOKABLE void requestTransactionDetailsAndShowPage(int TransactionID) override;
    /// CIP : OPEN_TRADE_CHARACTER_DIALOG → C 0x76 charbazaar conditions.
    Q_INVOKABLE void requestTradeCharacter() override;

private:
    /// CIP / : the new character name, then the
    /// confirmation.
    void onNewCharacterNameRequested(int offerId);
    void onNewCharacterNameEntered(const QString& name);
    /// CIP.
    void onHirelingConfigurationRequested(int offerId);
    void onHirelingConfigurationFinished(int offerId, const QString& name, int sex);
    /// CIP HirelingNameChange (S 0xDB) → SetHirelingName (C 0xEC).
    void onHirelingNameChangeRequested(quint32 creatureId, quint32 objectId);
    void onHirelingNameChangeEntered(const QString& name);
    /// CIP .
    void onTransferCoinsRecipientAndAmountSpecified(const QString& recipient, int amount);
    /// CIP.
    void onTransferSuccessNotification(const QString& message);
    void onTransferErrorNotification(const QString& message);
    void showConfiguredConfirmation(int offerId);

    /// CIP : Home first, then the server's categories.
    void onCategoriesChanged();
    /// CIP .
    void onHomePageChanged();
    /// CIP .
    void onOffersChanged(int offerId);
    /// CIP .
    void onSearchResultsChanged(bool moreResultsAvailable);
    /// CIP .
    void onEmptySearchResult();
    /// CIP .
    void onTransactionHistoryChanged();
    /// CIP .
    void onPurchaseSuccessNotification(const QString& message);
    /// CIP /.
    void onPurchaseErrorNotification(const QString& message);
    void onTransactionHistoryErrorNotification(const QString& message);
    void onInformationNotification(const QString& message);
    /// CIP .
    void onPurchaseConfirmed(int offerId, bool askAgain);
    /// CIP : sends the purchase and waits.
    void purchase(int offerId);
    /// CIP .
    void rebuildOffers();
    /// CIP .
    void selectOffer(int offerId);
    /// CIP .
    void rebuildFeaturedOffers();
    void setSelectedCategoryName(const QString& name);
    void showMessage(const QString& caption, const QString& message);

    QPointer<store::TStoreStorage> m_storage;
    QPointer<store::TStoreProtocolMessageHandler> m_messages;
    store::TDynamicallyLoadedImageManager* m_imageManager;
    TMessageDialogController* m_messageDialog;
    TStorePurchaseConfirmationDialogController m_confirmationDialog;
    TStorePurchaseSuccessDialogController m_successDialog;
    TTransferCreditsDialogController m_transferDialog;
    THirelingConfigurationDialogController m_hirelingDialog;
    TGenericEnterTextDialogController* m_newNameDialog;
    TGenericEnterTextDialogController* m_hirelingNameDialog;
    /// CIP +0x1d4 / +0x1d8 / +0x1f8 / +0x210: the offer waiting for its
    /// purchase data, the purchase type it is bought with (0 plain, 1 name
    /// change, 3 hireling), the name and the hireling's sex.
    int m_configuredOfferId = 0;
    int m_purchaseType = 0;
    QString m_purchaseName;
    int m_purchaseSex = 0;
    std::vector<store::TStoreOfferData> m_sortedOffers;
    QList<uint32_t> m_featuredOfferIds;
    QString m_lastSearch;
    int m_pendingJumpOfferId = 0;
    /// openWith's open action and detail, asked for once the categories are
    /// in (the premium category is looked up by name). `m_pendingSearchText`
    /// is used when open action is search (5).
    std::optional<std::pair<int, int>> m_pendingOpen;
    QString m_pendingSearchText;
    void requestPendingOpen();
};

/// The control buttons' store button (CIP: TStoreButtonController, ctor
/// , metacall); the sale and new flags follow the
/// store button indicators message. The server's store deep link
/// (SetStoreButtonDeeplink, 0xA8) highlights the button for 10 minutes and
/// is where the next click opens the store.
class TStoreButtonController final : public cip::tibia::gamewindow::TStoreButtonController
{
    Q_OBJECT
public:
    explicit TStoreButtonController(TStoreDialogController* dialog, QObject* parent = nullptr);

    bool highlighted() const override { return m_highlightTimer.isActive(); }

public Q_SLOTS:
    /// CIP : OPEN_STORE with the deep link (the home page without one).
    void onOpenStoreButtonClicked() override;
    void onStoreButtonIndicatorsChanged(bool SaleFlag, bool NewFlag) override;
    /// CIP .
    void onHighlightStoreButtonEnds(bool Protected) override;
    /// CIP : the store opened; the deep link is used up.
    void onRequestOpenStoreDialog() override;

private:
    /// CIP .
    void onStoreDeeplinkChanged(int openAction, int detail);
    void clearDeeplink();

    QPointer<TStoreDialogController> m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP's highlight helper (600000 ms, single shot).
    QTimer m_highlightTimer;
    int m_deeplinkAction = 0;
    int m_deeplinkDetail = -1;
};

}
}
