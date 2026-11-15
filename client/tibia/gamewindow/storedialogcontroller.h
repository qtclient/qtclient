#pragma once

#include "gamewindow/dialogmanager.h"
#include "store/storestorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <chrono>
#include <optional>

namespace tibia {
namespace store {
class TDynamicallyLoadedImageManager;
class TStoreDescriptionManager;
class TQmlStoreOffer;
}
}

namespace tibia {
namespace gamewindow {

class TGenericEnterTextDialogController;
class TMarketCoinTransactionDetailsDialogController;
class TMessageDialogController;

/// StorePurchaseConfirmationDialog.qml (CIP:
/// TStorePurchaseConfirmationDialogController): the offer about to be bought.
class TStorePurchaseConfirmationDialogController final
    : public TDialogController<cip::tibia::gamewindow::TStorePurchaseConfirmationDialogController>
{
    Q_OBJECT
public:
    explicit TStorePurchaseConfirmationDialogController(store::TDynamicallyLoadedImageManager* imageManager, QObject* parent = nullptr);

    /// Made anew for `offer`.
    void showDialog(int offerId, const store::TStoreOffer& offer, store::TStoreDescriptionManager* descriptionManager,
                    bool smoothTextureFiltering, bool showAskAgainCheckbox, bool buyWithCosmeticTokens);
    /// The same, showing the outfit of the hireling's sex.
    void showHirelingPurchaseDialog(int offerId, const store::TStoreOffer& offer, int sex,
                                    store::TStoreDescriptionManager* descriptionManager, bool smoothTextureFiltering,
                                    bool showAskAgainCheckbox, bool buyWithCosmeticTokens);

    /// Closes, then asks for the purchase.
    Q_INVOKABLE void purchaseRequested(bool AskForConfirmationAgainForOtherPurchases) override;
    Q_INVOKABLE void closeRequested() override;

    QVariantMap initialDialogProperties() const override;

Q_SIGNALS:
    void confirmPurchase(int offerId, bool askForConfirmationAgainForOtherPurchases);

private:
    void setOffer(int offerId, const store::TStoreOffer& offer, store::TStoreDescriptionManager* descriptionManager,
                  bool smoothTextureFiltering, bool showAskAgainCheckbox, bool buyWithCosmeticTokens);

    QPointer<store::TDynamicallyLoadedImageManager> m_imageManager;
    QPointer<store::TQmlStoreOffer> m_offer;
};

/// StorePurchaseSuccessDialog.qml (CIP: TStorePurchaseSuccessDialogController).
class TStorePurchaseSuccessDialogController final
    : public TDialogController<cip::tibia::gamewindow::TStorePurchaseSuccessDialogController>
{
    Q_OBJECT
public:
    explicit TStorePurchaseSuccessDialogController(QObject* parent = nullptr);

public Q_SLOTS:
    void requestClose() override;
    void showDialog(const QString& Message) override;
};

/// TransferCreditsDialog.qml (CIP: TTransferCreditsDialogController).
class TTransferCreditsDialogController final : public TDialogController<cip::tibia::gamewindow::TTransferCreditsDialogController>
{
    Q_OBJECT
public:
    explicit TTransferCreditsDialogController(QObject* parent = nullptr);

    /// Made anew with the transferable coins and the packet size.
    void showDialog(qint64 transferableCredits, qint64 creditPacketSize);

    /// Closes, then asks for the transfer.
    Q_INVOKABLE void requestTransfer(const QString& Receipient, int TransferAmount) override;
    Q_INVOKABLE void requestCancel() override;
    /// English digit grouping.
    Q_INVOKABLE QString formatCreditNumber(int Value) override;

Q_SIGNALS:
    void transferRequested(const QString& recipient, int amount);
};

/// HirelingConfigurationDialog.qml (CIP: THirelingConfigurationDialogController).
class THirelingConfigurationDialogController final
    : public TDialogController<cip::tibia::gamewindow::THirelingConfigurationDialogController>
{
    Q_OBJECT
public:
    explicit THirelingConfigurationDialogController(QObject* parent = nullptr);

    void showDialog(int offerId);

public Q_SLOTS:
    void requestClose() override;
    /// A name is required; closes after asking.
    void requestHireling(const QString& HirelingName, int Sex) override;

Q_SIGNALS:
    void hirelingConfigured(int offerId, const QString& name, int sex);

private:
    int m_offerId = 0;
};

/// The store, StoreDialog.qml (CIP: TStoreDialogController), on a
/// TStoreStorage and its TStoreProtocolMessageHandler (CIP takes both from the
/// game session). Crystal sends no world transfer or main character data, so
/// those purchases never start.
class TStoreDialogController final : public TDialogController<cip::tibia::gamewindow::TStoreDialogController>
{
    Q_OBJECT
public:
    TStoreDialogController(store::TStoreStorage* storage, store::TStoreProtocolMessageHandler* handler, QObject* parent = nullptr);
    ~TStoreDialogController() override;

    store::TStoreProtocolMessageHandler* messages() const { return m_messages; }

    /// Host: the store button and the market's "get coins" open the home page.
    void open();
    /// Host: OPEN_STORE with the open action and its detail (CIP's
    /// TStoreGameActionHandler hands the action's TStoreOpenParameters to the
    /// message handler): 1 premium time / boosts, 2 the category `text`, 3 the
    /// useful thing, 4 the offer, 5 search for `text`, anything else home.
    void openWith(int openAction, int detail, const QString& text = {});
    void openWith(const store::TStoreOpenParameters& parameters);

    QStringList sortOrderModel() const override;
    int selectedSortOrderIndex() const override;
    void setSelectedSortOrderIndex(int index) override;
    QStringList filterTagsModel() const override;
    int selectedFilterTagIndex() const override;
    void setSelectedFilterTagIndex(int index) override;
    void setSelectedOfferIndex(int index) override;
    bool smoothTextureFiltering() const override;

public Q_SLOTS:
    /// Shows the categories; opens the dialog unless it is open.
    void showDialog() override;
    /// Closes the store and everything it opened.
    void closeDialog() override;
    void getNextFeaturedOffer() override;
    void getPreviousFeaturedOffer() override;
    /// Opens the page the shown banner leads to.
    void onFeaturedOfferClicked() override;

public:
    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE void requestJumpToOfferWithID(int QmlOfferID) override;
    Q_INVOKABLE void selectCategory(const QString& CategoryName) override;
    Q_INVOKABLE void requestSearch(const QString& SearchString) override;
    Q_INVOKABLE void openGetCoinsUrl() override;
    Q_INVOKABLE void openTransactionHistory() override;
    Q_INVOKABLE void nextTransactionPage() override;
    Q_INVOKABLE void previousTransactionPage() override;
    Q_INVOKABLE void buyOffer(int QmlOfferID) override;
    Q_INVOKABLE void buyOffer(int QmlOfferID, bool BuyWithCosmeticTokens) override;
    /// REQUEST_OUTFITS_FROM_SERVER for the selected offer's outfit (try-on
    /// type 1) or mount (try-on type 2).
    Q_INVOKABLE void tryOutfitForCurrentOffer() override;
    /// The transfer dialog, or a message when fewer coins than one packet are
    /// transferable.
    Q_INVOKABLE void requestTransferCredits() override;
    Q_INVOKABLE void requestTransactionDetailsAndShowPage(int TransactionID) override;
    Q_INVOKABLE void requestTradeCharacter() override;

private:
    /// The purchase types: what a confirmed purchase sends with it.
    enum EPurchaseType
    {
        PURCHASE_PLAIN = 0,
        PURCHASE_NEW_CHARACTER_NAME = 1,
        PURCHASE_NEW_WORLD = 2,
        PURCHASE_HIRELING = 3,
        PURCHASE_NEW_MAIN_CHARACTER = 4,
        PURCHASE_WITHOUT_CONFIRMATION = 5,
    };

    void onRequestOpenStoreDialog();
    void onStoreImageBaseUrlChanged(const QUrl& baseUrl);
    /// Home first, then the server's categories; kept when unchanged.
    void onCategoriesChanged();
    void onHomePageChanged();
    void onOffersChanged(int preselectedOfferId);
    void onSearchResultsChanged(bool moreResultsAvailable);
    void onEmptySearchResultReceived();
    void onTransactionHistoryChanged();
    void onNewCharacterNameRequested(int offerId, bool buyWithCosmeticTokens);
    void onNewCharacterNameEntered(const QString& name);
    void onConfirmationDialogRequested(int offerId, const QString& message, bool buyWithCosmeticTokens);
    void onHirelingConfigurationRequested(int offerId, bool buyWithCosmeticTokens);
    void onHirelingConfigurationFinished(int offerId, const QString& name, int sex);
    void onTransferCoinsReceipientAndAmountSpecified(const QString& recipient, int amount);
    /// The success dialog, or in a fight the console (and the game window
    /// when store messages show there).
    void onPurchaseSuccessNotification(const QString& message);
    void onTransferSuccessNotification(const QString& message);
    void onPurchaseErrorNotification(const QString& message);
    void onTransferErrorNotification(const QString& message);
    void onTransactionHistoryErrorNotification(const QString& message);
    void onInformationNotification(const QString& message);
    /// Sends the purchase of the type asked for and waits for the result.
    void onPurchaseConfirmed(int offerId, bool askForConfirmationAgainForOtherPurchases);
    /// The selected offer was looked at for 2 seconds (not on the home page).
    void onViewEventTimeoutElapsed();
    void onExtraConfirmDialogClickedOK();
    void onExtraConfirmDialogClickedNotOK();
    /// Host: HirelingNameChange (S 0xDB) → enter name → SetHirelingName (C 0xEC).
    void onHirelingNameChangeRequested(quint32 creatureId, quint32 objectId);
    void onHirelingNameChangeEntered(const QString& name);

    /// The offers of the active filter tag, the first one selected.
    void rebuildAvailableOffers();
    /// Selects the offer that has `offerId` (else the first one).
    void selectOfferWithId(int offerId);
    /// One offer-viewed event for every quantity of the offer.
    void sendOfferViewedEvents(int offerId);
    void showProcessingOrderMessage();
    void showConfiguringOrderMessage();
    /// The banners of the home page, one picked at random.
    void updateFeaturedOffers();
    void setSelectedCategoryName(const QString& name);
    void showMessage(TMessageDialogController* dialog, const QString& caption, const QString& message);
    void openStore(const store::TStoreOpenParameters& parameters);

    QPointer<store::TStoreStorage> m_storage;
    QPointer<store::TStoreProtocolMessageHandler> m_messages;
    store::TDynamicallyLoadedImageManager* m_imageManager;
    store::TStoreDescriptionManager* m_descriptionManager;
    TStorePurchaseConfirmationDialogController m_confirmationDialog;
    TGenericEnterTextDialogController* m_newCharacterNameDialog;
    THirelingConfigurationDialogController m_hirelingDialog;
    /// The server's extra confirmation (OK / Cancel).
    TMessageDialogController* m_extraConfirmationDialog;
    TTransferCreditsDialogController m_transferDialog;
    /// Wait, success and error messages.
    TMessageDialogController* m_messageDialog;
    /// Search result messages.
    TMessageDialogController* m_searchMessageDialog;
    TStorePurchaseSuccessDialogController m_successDialog;
    /// Whether the selected offer's view was reported.
    bool m_offerViewReported = false;
    std::chrono::steady_clock::time_point m_openedAt{};
    bool m_opened = false;
    QTimer m_viewEventTimer;
    /// The offer to select once the offers are in (after a purchase or a jump).
    int m_offerIdToSelect = 0;
    int m_offerIdForNameChange = 0;
    int m_purchaseType = PURCHASE_PLAIN;
    QString m_newWorldName;
    QString m_newName;
    int m_hirelingSex = 0;
    QString m_newMainCharacter;
    int m_extraConfirmationOfferId = 0;
    bool m_buyWithCosmeticTokens = false;
    TMarketCoinTransactionDetailsDialogController* m_transactionDetailsDialog;
    int m_transactionDetailsId = 0;
    /// Host: hireling renaming outside the store.
    TGenericEnterTextDialogController* m_hirelingNameDialog;
};

/// The control buttons' store button (CIP: TStoreButtonController); the sale
/// and new flags follow the store button indicators message. The server's
/// store deep link (SetStoreButtonDeeplink, 0xA8) highlights the button for 10
/// minutes and is where the next click opens the store.
class TStoreButtonController final : public cip::tibia::gamewindow::TStoreButtonController
{
    Q_OBJECT
public:
    explicit TStoreButtonController(TStoreDialogController* dialog, QObject* parent = nullptr);

    bool highlighted() const override { return m_highlightTimer.isActive(); }

public Q_SLOTS:
    /// OPEN_STORE with the deep link (the home page without one).
    void onOpenStoreButtonClicked() override;
    void onStoreButtonIndicatorsChanged(bool SaleFlag, bool NewFlag) override;
    void onHighlightStoreButtonEnds(bool Protected) override;
    /// The store opened; the deep link is used up.
    void onRequestOpenStoreDialog() override;

private:
    void onStoreDeeplinkChanged(int openAction, int detail);
    void clearDeeplink();

    QPointer<TStoreDialogController> m_dialog;
    std::vector<client::gamecore::Hook> m_hooks;
    /// The highlight (600000 ms, single shot).
    QTimer m_highlightTimer;
    int m_deeplinkAction = 0;
    int m_deeplinkDetail = -1;
};

}
}
