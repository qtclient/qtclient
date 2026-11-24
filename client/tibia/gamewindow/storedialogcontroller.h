#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/hirelingnamechangecontroller.h"
#include "store/storestorage.h"
#include "highlighttimer.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <chrono>
#include <memory>
#include <optional>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
}
namespace store {
class TDynamicallyLoadedImageManager;
class TStoreDescriptionManager;
class TQmlStoreOffer;
}
}

namespace tibia {
namespace gamewindow {

class TCharacterWorldTransferDialogController;
class TGenericEnterTextDialogController;
class TMainCharacterChangeDialogController;
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

/// The store, StoreDialog.qml (CIP: TStoreDialogController), on the game
/// session's TStoreStorage and TStoreProtocolMessageHandler. The world transfer
/// and main character purchases start when the server asks for their input.
class TStoreDialogController final : public TDialogController<cip::tibia::gamewindow::TStoreDialogController>
{
    Q_OBJECT
public:
    TStoreDialogController(std::shared_ptr<store::TDynamicallyLoadedImageManager> DynamicallyLoadedImageManager,
                           config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                           QObject* parent = nullptr);
    ~TStoreDialogController() override;

    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// Host: the store button and the market's "get coins" open the home page.
    void open();
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

    void onStoreImageBaseUrlChanged(const QUrl& baseUrl);
    /// Home first, then the server's categories; kept when unchanged.
    void onCategoriesChanged();
    void onHomePageChanged();
    void onOffersChanged(int preselectedOfferId);
    void onSearchResultsChanged(bool moreResultsAvailable);
    void onEmptySearchResultReceived();
    void onTransactionHistoryChanged();
    /// The storage's transaction details, shown when they are the requested ones.
    void onTransactionDetailsChanged();
    void onNewCharacterNameRequested(int offerId, bool buyWithCosmeticTokens);
    void onNewCharacterNameEntered(const QString& name);
    /// The world transfer dialog for the own character and the offer.
    void onCharacterWorldTransferWorldRequested(int offerId, const store::TWorldTransferInformation& worldTransferInformation,
                                                bool buyWithCosmeticTokens);
    /// The main character dialog with the offer's possible characters.
    void onMainCharChangeRequested(int offerId, const std::vector<store::TPossibleMainCharacterInformation>& possibleMainCharInfo,
                                   bool buyWithCosmeticTokens);
    /// The world transfer dialog chose a world: confirm the offer for it.
    void onCharacterWorldTransferWorldEntered(int offerId, const QString& worldName);
    /// The main character dialog chose a character: confirm the offer for it.
    void onMainCharChangeConfigurationFinished(int offerId, const QString& characterName);
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

    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<store::TStoreProtocolMessageHandler> m_messages;
    QPointer<store::TStoreStorage> m_storage;
    shared::TQtConnectionManager m_gameSessionConnections;
    /// Shared with the other dialogs that show downloaded images.
    std::shared_ptr<store::TDynamicallyLoadedImageManager> m_imageManager;
    store::TStoreDescriptionManager* m_descriptionManager;
    config::TClientOptions* m_clientOptions;
    TStorePurchaseConfirmationDialogController m_confirmationDialog;
    TGenericEnterTextDialogController* m_newCharacterNameDialog;
    TCharacterWorldTransferDialogController* m_worldTransferDialog;
    TMainCharacterChangeDialogController* m_mainCharChangeDialog;
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
    /// Host: the hireling rename prompt lives with the store (CIP creates it with the game client).

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

/// The control buttons' store button (CIP: TStoreButtonController); the sale
/// and new flags follow the store button indicators message. The server's
/// store deep link (SetStoreButtonDeeplink, 0xA8) highlights the button for 10
/// minutes and is where the next click opens the store.
class TStoreButtonController final : public cip::tibia::gamewindow::TStoreButtonController
{
    Q_OBJECT
public:
    explicit TStoreButtonController(QObject* parent = nullptr);

    bool highlighted() const override { return m_highlightTimer.isHighlighted(); }

    void connectToGameSession(const std::shared_ptr<game::IGameSession>& pGameSession);

public Q_SLOTS:
    /// OPEN_STORE with the deep link (the home page without one).
    void onOpenStoreButtonClicked() override;
    /// A sale or a new offer highlights the button (protected).
    void onStoreButtonIndicatorsChanged(bool SaleFlag, bool NewFlag) override;
    void onHighlightStoreButtonEnds(bool Protected) override;
    /// The store opened: the highlight stops (even a protected one) and the
    /// deep link is used up.
    void onRequestOpenStoreDialog() override;

private:
    /// The server's deep link: kept for the next click; any link but the home
    /// page highlights the button, the home page ends an unprotected highlight.
    void onStoreDeeplinkChanged(const store::TStoreOpenParameters& StoreDeeplink);
    void clearDeeplink();

    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The highlight (600000 ms).
    shared::THighlightTimer m_highlightTimer{ 600000 };
    store::TStoreOpenParameters m_deeplink;
};

}
}
