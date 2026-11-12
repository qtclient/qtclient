#include "storedialogcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameactionhandlers.h"
#include "store/dynamicallyloadedimage.h"
#include "store/qmlstoreobjects.h"

#include <QLocale>
#include <QRandomGenerator>

#include <algorithm>
#include <utility>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

constexpr auto AskBeforeBuyingOption = "storeAskBeforeBuyingProducts";

QString homeCategoryName()
{
    return qtTrId("store_home_catergory_name");
}

QString searchCategoryName()
{
    return qtTrId("store_search_result_catergory_name");
}

/// CIP : one "OK" button.
QList<TMessageDialogButton*> okButton()
{
    using Button = TMessageDialogController::Button;
    return { new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

void deleteObjectsLater(const QList<QObject*>& objects)
{
    for (QObject* object : objects)
        object->deleteLater();
}

}

TStorePurchaseConfirmationDialogController::TStorePurchaseConfirmationDialogController(QObject* parent)
    : TDialogController(QStringLiteral("StorePurchaseConfirmationDialog.qml"), parent)
{}

void TStorePurchaseConfirmationDialogController::showDialog(const int offerId, const store::TStoreOfferData& offer, const bool showAskAgainCheckbox)
{
    showDialog(offerId, offer, showAskAgainCheckbox, false);
}

// b1/28cadf84c8875751
void TStorePurchaseConfirmationDialogController::showDialog(const int offerId, const store::TStoreOfferData& offer,
                                                           const bool showAskAgainCheckbox, const bool buyWithCosmeticTokens)
{
    dialog()->destroy();
    if (m_offer)
        m_offer->deleteLater();
    m_offer = new store::TQmlStoreOffer(offer, this);
    m_offerToPurchase = m_offer;
    m_offerID = offerId;
    m_showAskAgainCheckbox = showAskAgainCheckbox;
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    dialog()->show();
}

void TStorePurchaseConfirmationDialogController::purchaseRequested(const bool AskForConfirmationAgainForOtherPurchases)
{
    dialog()->close();
    Q_EMIT purchaseConfirmed(m_offerID, AskForConfirmationAgainForOtherPurchases);
}

void TStorePurchaseConfirmationDialogController::closeRequested()
{
    dialog()->close();
}

TStorePurchaseSuccessDialogController::TStorePurchaseSuccessDialogController(QObject* parent)
    : TDialogController(QStringLiteral("StorePurchaseSuccessDialog.qml"), parent)
{}

void TStorePurchaseSuccessDialogController::requestClose()
{
    dialog()->close();
}

// b1/879bca3f56680f4f
void TStorePurchaseSuccessDialogController::showDialog(const QString& Message)
{
    dialog()->destroy();
    m_message = Message;
    dialog()->show();
}

TTransferCreditsDialogController::TTransferCreditsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("TransferCreditsDialog.qml"), parent)
{
    m_maximumNameLength = 29;
}

// b1/19b755d531f0f9b7
void TTransferCreditsDialogController::showDialog(const qint64 transferableCredits, const qint64 creditPacketSize)
{
    dialog()->destroy();
    m_transferableCredits = transferableCredits;
    m_creditPacketSize = creditPacketSize;
    dialog()->show();
}

// b1/91aa4cc6331ea853
void TTransferCreditsDialogController::requestTransfer(const QString& Receipient, const int TransferAmount)
{
    dialog()->close();
    Q_EMIT transferRequested(Receipient, TransferAmount);
}

// b1/b7cfd5015b64b338
void TTransferCreditsDialogController::requestCancel()
{
    dialog()->close();
}

// b1/e65203ee8d158a1a
QString TTransferCreditsDialogController::formatCreditNumber(const int Value)
{
    return QLocale(QLocale::English).toString(Value);
}

THirelingConfigurationDialogController::THirelingConfigurationDialogController(QObject* parent)
    : TDialogController(QStringLiteral("HirelingConfigurationDialog.qml"), parent)
{}

void THirelingConfigurationDialogController::showDialog(const int offerId)
{
    dialog()->close();
    m_offerId = offerId;
    dialog()->show();
}

// b1/63f43af11ed56eda
void THirelingConfigurationDialogController::requestClose()
{
    dialog()->close();
}

// b1/5794907ffe2530a8
void THirelingConfigurationDialogController::requestHireling(const QString& HirelingName, const int Sex)
{
    if (HirelingName.isEmpty())
        return;
    Q_EMIT hirelingConfigured(m_offerId, HirelingName, Sex);
    dialog()->close();
}

TStoreDialogController::TStoreDialogController(store::TStoreStorage* storage, store::TStoreProtocolMessageHandler* handler, QObject* parent)
    : TDialogController(QStringLiteral("StoreDialog.qml"), parent)
    , m_storage(storage)
    , m_messages(handler)
    , m_imageManager(new store::TDynamicallyLoadedImageManager(this))
    , m_messageDialog(new TMessageDialogController(this))
    , m_newNameDialog(new TGenericEnterTextDialogController(this))
    , m_hirelingNameDialog(new TGenericEnterTextDialogController(this))
{
    // CIP THirelingNameChangeController keep-alive beside the enter-text rename dialog.
    new THirelingNameChangeController(this);
    m_newNameDialog->setCaption(qtTrId("enter_text_new_character_caption"));
    m_newNameDialog->setDescription(qtTrId("enter_text_new_character_description"));
    m_newNameDialog->setMaxInputLength(29);
    connect(m_newNameDialog, &TGenericEnterTextDialogController::textEntered, this, &TStoreDialogController::onNewCharacterNameEntered);
    m_hirelingNameDialog->setCaption(qtTrId("request_hireling_name_change_caption"));
    m_hirelingNameDialog->setDescription(qtTrId("request_hireling_name_change_text"));
    m_hirelingNameDialog->setMaxInputLength(21);
    connect(m_hirelingNameDialog, &TGenericEnterTextDialogController::textEntered, this,
            &TStoreDialogController::onHirelingNameChangeEntered);
    connect(&m_hirelingDialog, &THirelingConfigurationDialogController::hirelingConfigured, this,
            &TStoreDialogController::onHirelingConfigurationFinished);
    connect(&m_transferDialog, &TTransferCreditsDialogController::transferRequested, this,
            &TStoreDialogController::onTransferCoinsRecipientAndAmountSpecified);
    connect(handler, &store::TStoreProtocolMessageHandler::characterNameChangeRequested, this,
            &TStoreDialogController::onNewCharacterNameRequested);
    connect(handler, &store::TStoreProtocolMessageHandler::hirelingConfigurationRequested, this,
            &TStoreDialogController::onHirelingConfigurationRequested);
    connect(handler, &store::TStoreProtocolMessageHandler::hirelingNameChangeRequested, this,
            &TStoreDialogController::onHirelingNameChangeRequested);
    connect(handler, &store::TStoreProtocolMessageHandler::requestTransferSuccessNotification, this,
            &TStoreDialogController::onTransferSuccessNotification);
    m_dynamicallyLoadedImageManager = m_imageManager;
    m_searchResultCategory = new store::TQmlStoreCategory(searchCategoryName(), {}, this);
    m_sortOrderModel = { qtTrId("store_sort_popularity"), qtTrId("store_sort_name"), qtTrId("store_sort_newest") };
    m_filterTagsModel = { qtTrId("store_filter_tag_all") };
    m_selectedOfferIndex = -1;
    m_featuredOfferIndex = -1;
    m_messageDialog->setCloseOnButtonClicked(true);

    m_imageManager->setBaseUrl(handler->imageBaseUrl());
    connect(handler, &store::TStoreProtocolMessageHandler::storeImageBaseUrlChanged, m_imageManager,
            &store::TDynamicallyLoadedImageManager::setBaseUrl);
    connect(handler, &store::TStoreProtocolMessageHandler::requestPurchaseSuccessNotification, this,
            &TStoreDialogController::onPurchaseSuccessNotification);
    connect(handler, &store::TStoreProtocolMessageHandler::requestPurchaseErrorNotification, this,
            &TStoreDialogController::onPurchaseErrorNotification);
    connect(handler, &store::TStoreProtocolMessageHandler::requestTransferErrorNotification, this,
            &TStoreDialogController::onTransferErrorNotification);
    connect(handler, &store::TStoreProtocolMessageHandler::requestTransactionHistoryErrorNotification, this,
            &TStoreDialogController::onTransactionHistoryErrorNotification);
    connect(handler, &store::TStoreProtocolMessageHandler::requestInformationNotification, this,
            &TStoreDialogController::onInformationNotification);
    connect(storage, &store::TStoreStorage::categoriesChanged, this, &TStoreDialogController::onCategoriesChanged);
    connect(storage, &store::TStoreStorage::homePageChanged, this, &TStoreDialogController::onHomePageChanged);
    connect(storage, &store::TStoreStorage::offersChanged, this, &TStoreDialogController::onOffersChanged);
    connect(storage, &store::TStoreStorage::searchResultsChanged, this, &TStoreDialogController::onSearchResultsChanged);
    connect(storage, &store::TStoreStorage::emptySearchResultReceived, this, &TStoreDialogController::onEmptySearchResult);
    connect(storage, &store::TStoreStorage::transactionHistoryChanged, this, &TStoreDialogController::onTransactionHistoryChanged);
    connect(storage, &store::TStoreStorage::offerDescriptionChanged, this, [this](const int offerId) {
        const store::TStoreOfferData* data = m_storage ? m_storage->offerById(static_cast<uint32_t>(offerId)) : nullptr;
        for (QObject* object : std::as_const(m_availableOffers)) {
            auto* offer = static_cast<store::TQmlStoreOffer*>(object);
            if (data && offer->hasQuantity(offerId))
                offer->setDescription(data->description);
        }
    });
    connect(&m_confirmationDialog, &TStorePurchaseConfirmationDialogController::purchaseConfirmed, this,
            &TStoreDialogController::onPurchaseConfirmed);

    input::setActionHandler(input::EGameAction::CLOSE_STORE_WAIT_OR_ERROR_MESSAGE_DIALOG, [this](const input::TGameAction&) {
        m_messageDialog->dialog()->close();
        return true;
    });
    input::setActionHandler(input::EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE, [this](const input::TGameAction& action) {
        openWith(action.parameter > 0 ? action.parameter : 0, action.instance);
        return true;
    });
    input::setActionHandler(input::EGameAction::SET_HIRELING_NAME, [](const input::TGameAction& action) {
        if (!action.text.isEmpty())
            tibia::game::sendSetHirelingName(action.text.toStdString());
        return true;
    });
}

TStoreDialogController::~TStoreDialogController()
{
    input::setActionHandler(input::EGameAction::CLOSE_STORE_WAIT_OR_ERROR_MESSAGE_DIALOG, nullptr);
    input::setActionHandler(input::EGameAction::BUY_HIRELING_NAME_OR_SEX_CHANGE, nullptr);
    input::setActionHandler(input::EGameAction::SET_HIRELING_NAME, nullptr);
}

void TStoreDialogController::open()
{
    if (m_storage)
        m_storage->openStore();
    showDialog();
}

void TStoreDialogController::openWith(const int openAction, const int detail, const QString& text)
{
    m_pendingOpen = std::make_pair(openAction, detail);
    m_pendingSearchText = text;
    open();
}

void TStoreDialogController::requestPendingOpen()
{
    const auto [openAction, detail] = *m_pendingOpen;
    m_pendingOpen.reset();
    const QString searchText = m_pendingSearchText;
    m_pendingSearchText.clear();
    switch (openAction) {
        case 1: m_storage->requestPremiumOrBoosts(detail == 1); break;
        case 3: m_storage->requestUsefulThing(detail); break;
        case 4: requestJumpToOfferWithID(detail); break;
        case 5:
            if (!searchText.isEmpty()) {
                requestSearch(searchText);
                setSelectedCategoryName(searchCategoryName());
            } else {
                selectCategory(homeCategoryName());
            }
            break;
        default: selectCategory(homeCategoryName()); break;
    }
}

// b1/f3a74d8a57ea2af5
void TStoreDialogController::showDialog()
{
    onCategoriesChanged();
    if (!dialog()->isShown())
        dialog()->show();
}

void TStoreDialogController::closeDialog()
{
    dialog()->close();
}

// b1/f79f2bf457028c4f
void TStoreDialogController::requestClose()
{
    dialog()->close();
}

void TStoreDialogController::setSelectedSortOrderIndex(const int index)
{
    if (m_selectedSortOrderIndex == index)
        return;
    m_selectedSortOrderIndex = index;
    Q_EMIT selectedSortOrderIndexChanged();
    if (m_selectedCategoryName != homeCategoryName())
        rebuildOffers();
}

void TStoreDialogController::setSelectedFilterTagIndex(const int index)
{
    if (m_selectedFilterTagIndex == index)
        return;
    m_selectedFilterTagIndex = index;
    Q_EMIT selectedFilterTagIndexChanged();
}

void TStoreDialogController::setSelectedOfferIndex(const int index)
{
    if (m_selectedOfferIndex == index)
        return;
    m_selectedOfferIndex = index;
    Q_EMIT selectedOfferIndexChanged();
}

void TStoreDialogController::getNextFeaturedOffer()
{
    if (m_featuredOffers.isEmpty())
        return;
    m_featuredOfferIndex = (m_featuredOfferIndex + 1) % static_cast<int>(m_featuredOffers.size());
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::getPreviousFeaturedOffer()
{
    if (m_featuredOffers.isEmpty())
        return;
    m_featuredOfferIndex = (m_featuredOfferIndex + static_cast<int>(m_featuredOffers.size()) - 1) % static_cast<int>(m_featuredOffers.size());
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::onFeaturedOfferClicked()
{
    if (m_featuredOfferIndex >= 0 && m_featuredOfferIndex < m_featuredOfferIds.size() && m_featuredOfferIds[m_featuredOfferIndex] != 0)
        requestJumpToOfferWithID(static_cast<int>(m_featuredOfferIds[m_featuredOfferIndex]));
}

// b1/152238c33a360b1c
void TStoreDialogController::requestJumpToOfferWithID(const int QmlOfferID)
{
    if (!m_storage)
        return;
    m_pendingJumpOfferId = QmlOfferID;
    m_storage->requestOffer(static_cast<uint32_t>(QmlOfferID));
}

// b1/2a9f4e67019d699b
void TStoreDialogController::selectCategory(const QString& CategoryName)
{
    if (!m_storage)
        return;
    if (CategoryName == homeCategoryName())
        m_storage->requestHome();
    else
        m_storage->requestCategory(CategoryName);
}

// b1/d8ca36831700f642
void TStoreDialogController::requestSearch(const QString& SearchString)
{
    if (!m_storage)
        return;
    m_lastSearch = SearchString;
    m_storage->requestSearch(SearchString);
}

// b1/903ba05aa05cf9c8
void TStoreDialogController::openGetCoinsUrl()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_GET_COINS));
}

// b1/2bba13e329686aa4
void TStoreDialogController::openTransactionHistory()
{
    if (!m_storage)
        return;
    m_storage->openTransactionHistory();
    onTransactionHistoryChanged();
    m_transactionHistoryLoading = true;
    Q_EMIT transactionHistoryLoadingChanged();
}

// b1/963dd02219d4e72b
void TStoreDialogController::nextTransactionPage()
{
    if (m_storage)
        m_storage->requestTransactionHistoryPage(m_storage->historyPage() + 1);
}

void TStoreDialogController::previousTransactionPage()
{
    if (m_storage && m_storage->historyPage() > 0)
        m_storage->requestTransactionHistoryPage(m_storage->historyPage() - 1);
}

// b1/ba8f4d3ec904cc78
void TStoreDialogController::buyOffer(const int QmlOfferID)
{
    buyOffer(QmlOfferID, false);
}

// b1/ba8f4d3ec904cc78
void TStoreDialogController::buyOffer(const int QmlOfferID, const bool BuyWithCosmeticTokens)
{
    if (!m_storage)
        return;
    const store::TStoreOfferData* offer = m_storage->offerById(static_cast<uint32_t>(QmlOfferID));
    if (!offer)
        return;
    // TODO: cosmetic-token purchase currency once CIP resource id + store buy
    // payload field are known; UI path keeps BuyWithCosmeticTokens for confirmation.
    if (offer->needsUserConfiguration) {
        m_storage->buy(static_cast<uint32_t>(QmlOfferID));
        m_messageDialog->setButtons(okButton());
        m_messageDialog->showMessage(qtTrId("store_configuring_order_caption"), qtTrId("store_configuring_order"));
        return;
    }
    const bool ask = app::ClientOptions::load().options().value(QLatin1String(AskBeforeBuyingOption)).toBool(true);
    if (ask) {
        m_confirmationDialog.showDialog(QmlOfferID, *offer, true, BuyWithCosmeticTokens);
        return;
    }
    purchase(QmlOfferID);
}

// b1/ebe05f7682b907dd
void TStoreDialogController::onPurchaseConfirmed(const int offerId, const bool askAgain)
{
    purchase(offerId);
    app::ClientOptions::store(QLatin1String(AskBeforeBuyingOption), askAgain);
}

/// CIP : the buy carries the purchase data the server asked for.
// b1/12f21f865e4baeca
void TStoreDialogController::purchase(const int offerId)
{
    if (!m_storage)
        return;
    const auto id = static_cast<uint32_t>(offerId);
    switch (m_purchaseType) {
        case 1: m_storage->buyConfigured(id, 1, m_purchaseName); break;
        case 3: m_storage->buyConfigured(id, 3, m_purchaseName, static_cast<uint8_t>(m_purchaseSex)); break;
        default: m_storage->buy(id); break;
    }
    m_purchaseType = 0;
    m_purchaseName.clear();
    m_messageDialog->setButtons(okButton());
    m_messageDialog->showMessage(qtTrId("store_processing_order_caption"), qtTrId("store_processing_order"));
    m_storage->setBalanceUpdating(true);
    m_pendingJumpOfferId = offerId;
}

// b1/4a23d19548f21bd4
void TStoreDialogController::tryOutfitForCurrentOffer()
{
    if (m_selectedOfferIndex < 0 || m_selectedOfferIndex >= static_cast<int>(m_sortedOffers.size()))
        return;
    const store::TStoreOfferData& offer = m_sortedOffers[static_cast<size_t>(m_selectedOfferIndex)];
    const store::TStoreVisual& visual = offer.visual;
    if (visual.type != store::TStoreVisual::Mount && visual.type != store::TStoreVisual::Outfit
        && visual.type != store::TStoreVisual::BothGenderOutfits)
        return;
    int typeId = visual.typeId;
    if (visual.type == store::TStoreVisual::BothGenderOutfits) {
        // CIP picks by the player's sex, which qtclient does not track.
        const bool female = tibia::game::localPlayer()
                            && tibia::game::localPlayerOutfit().getId() == static_cast<uint16_t>(visual.femaleTypeId);
        typeId = female ? visual.femaleTypeId : visual.maleTypeId;
    }
    int mode = 0;
    if (offer.tryOnType == 1)
        mode = 1;
    else if (offer.tryOnType == 2)
        mode = 3;
    else
        return;
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::REQUEST_OUTFITS_FROM_SERVER));
    action->parameter = mode;
    action->instance = typeId;
    input::publish(action);
}

// b1/fdd382775e29b448
void TStoreDialogController::requestTransferCredits()
{
    if (!m_storage)
        return;
    const auto transferable = static_cast<qint64>(m_storage->transferableCoins());
    const qint64 packetSize = m_messages ? m_messages->creditPacketSize() : 1;
    if (transferable < packetSize) {
        showMessage(qtTrId("store_not_enough_coins_for_transfer_caption"), qtTrId("store_not_enough_coins_for_transfer"));
        return;
    }
    m_transferDialog.showDialog(transferable, packetSize);
}

// b1/069098ffa42334a3
void TStoreDialogController::requestTransactionDetailsAndShowPage(const int TransactionID)
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::REQUEST_TRANSACTION_DETAILS));
    action->instance = TransactionID;
    input::publish(action);
}

// b1/c8dfc553def00e66
void TStoreDialogController::requestTradeCharacter()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_TRADE_CHARACTER_DIALOG));
}

// b1/41cf06960997352b
void TStoreDialogController::onNewCharacterNameRequested(const int offerId)
{
    m_messageDialog->dialog()->close();
    m_configuredOfferId = offerId;
    m_newNameDialog->setText({});
    m_newNameDialog->dialog()->destroy();
    m_newNameDialog->dialog()->show();
}

void TStoreDialogController::onNewCharacterNameEntered(const QString& name)
{
    m_purchaseType = 1;
    m_purchaseName = name;
    showConfiguredConfirmation(m_configuredOfferId);
}

// b1/14089aa415489f39
void TStoreDialogController::onHirelingConfigurationRequested(const int offerId)
{
    m_messageDialog->dialog()->close();
    m_hirelingDialog.showDialog(offerId);
}

void TStoreDialogController::onHirelingConfigurationFinished(const int offerId, const QString& name, const int sex)
{
    m_purchaseType = 3;
    m_purchaseName = name;
    m_purchaseSex = sex;
    showConfiguredConfirmation(offerId);
}

void TStoreDialogController::onHirelingNameChangeRequested(const quint32 creatureId, const quint32 objectId)
{
    Q_UNUSED(creatureId);
    Q_UNUSED(objectId);
    m_messageDialog->dialog()->close();
    m_hirelingNameDialog->setText({});
    m_hirelingNameDialog->dialog()->destroy();
    m_hirelingNameDialog->dialog()->show();
}

void TStoreDialogController::onHirelingNameChangeEntered(const QString& name)
{
    if (name.trimmed().isEmpty() || !tibia::game::isOnline())
        return;
    tibia::game::sendSetHirelingName(name.trimmed().toStdString());
}

void TStoreDialogController::showConfiguredConfirmation(const int offerId)
{
    const store::TStoreOfferData* offer = m_storage ? m_storage->offerById(static_cast<uint32_t>(offerId)) : nullptr;
    if (!offer)
        return;
    const bool ask = app::ClientOptions::load().options().value(QLatin1String(AskBeforeBuyingOption)).toBool(true);
    m_confirmationDialog.showDialog(offerId, *offer, ask);
}

// b1/fadf1be5dc489f49
void TStoreDialogController::onTransferCoinsRecipientAndAmountSpecified(const QString& recipient, const int amount)
{
    if (!m_storage || amount <= 0)
        return;
    m_storage->transferCoins(recipient, static_cast<uint32_t>(amount));
    m_storage->setBalanceUpdating(true);
}

// b1/01612db19028b0d6
void TStoreDialogController::onTransferSuccessNotification(const QString& message)
{
    showMessage(qtTrId("store_success_caption"), message);
}

void TStoreDialogController::onTransferErrorNotification(const QString& message)
{
    showMessage(qtTrId("store_transfer_failed"), message);
    if (m_storage)
        m_storage->setBalanceUpdating(false);
}

// b1/626f39f9e476a78e
void TStoreDialogController::onCategoriesChanged()
{
    if (!m_storage)
        return;
    const QList<QObject*> previous = m_categoryList;
    m_categoryList.clear();
    m_categoryList.append(new store::TQmlStoreCategory(homeCategoryName(), {}, this));
    for (const store::TStoreCategoryData& category : m_storage->categories())
        m_categoryList.append(new store::TQmlStoreCategory(category, this));
    Q_EMIT categoryListChanged();
    deleteObjectsLater(previous);
    if (m_storage->categories().empty())
        setSelectedCategoryName({});
    else if (m_pendingOpen)
        requestPendingOpen();
    else if (m_selectedCategoryName.isEmpty() && dialog()->isShown())
        selectCategory(homeCategoryName());
}

// b1/77b42202fe6c898c
void TStoreDialogController::onHomePageChanged()
{
    setSelectedCategoryName(homeCategoryName());
    rebuildOffers();
    rebuildFeaturedOffers();
}

// b1/95b213cbdd8e59ce
void TStoreDialogController::onOffersChanged(const int offerId)
{
    if (!m_storage)
        return;
    setSelectedCategoryName(m_storage->offersCategoryName());
    rebuildOffers();
    if (offerId != 0 || m_pendingJumpOfferId != 0)
        selectOffer(offerId != 0 ? offerId : m_pendingJumpOfferId);
}

// b1/38efc31e5438b8f2
void TStoreDialogController::onSearchResultsChanged(const bool moreResultsAvailable)
{
    setSelectedCategoryName(searchCategoryName());
    rebuildOffers();
    if (m_pendingJumpOfferId != 0)
        selectOffer(m_pendingJumpOfferId);
    if (moreResultsAvailable && dialog()->isShown())
        showMessage(qtTrId("store_information"), qtTrId("store_search_result_more_results_available").arg(m_lastSearch));
}

// b1/9018beaaae17b850
void TStoreDialogController::onEmptySearchResult()
{
    if (dialog()->isShown())
        showMessage(qtTrId("store_information"), qtTrId("store_search_result_empty").arg(m_lastSearch));
}

// b1/097b7f582a6f3d2e
void TStoreDialogController::onTransactionHistoryChanged()
{
    if (!m_storage)
        return;
    QObject* previous = m_transactionPage;
    m_transactionPage = new store::TQmlStoreTransactionHistoryPage(*m_storage, this);
    Q_EMIT transactionPageChanged();
    if (previous)
        previous->deleteLater();
    if (m_transactionHistoryLoading) {
        m_transactionHistoryLoading = false;
        Q_EMIT transactionHistoryLoadingChanged();
    }
}

// b1/e4b955e81c14c7d6
void TStoreDialogController::onPurchaseSuccessNotification(const QString& message)
{
    m_messageDialog->dialog()->close();
    m_successDialog.showDialog(message.trimmed());
}

// b1/e757d2d4bc24d68e
void TStoreDialogController::onPurchaseErrorNotification(const QString& message)
{
    showMessage(qtTrId("store_purchase_failed"), message);
    if (m_storage)
        m_storage->setBalanceUpdating(false);
    m_pendingJumpOfferId = 0;
}

void TStoreDialogController::onTransactionHistoryErrorNotification(const QString& message)
{
    showMessage(qtTrId("store_history_error"), message);
    if (m_transactionHistoryLoading) {
        m_transactionHistoryLoading = false;
        Q_EMIT transactionHistoryLoadingChanged();
    }
}

void TStoreDialogController::onInformationNotification(const QString& message)
{
    showMessage(qtTrId("store_information"), message);
}

void TStoreDialogController::showMessage(const QString& caption, const QString& message)
{
    m_messageDialog->setButtons(okButton());
    m_messageDialog->showMessage(caption, message);
}

// b1/3d7a1a5c8161ce1b
void TStoreDialogController::rebuildOffers()
{
    if (!m_storage)
        return;
    m_sortedOffers = m_storage->offers();
    if (m_selectedCategoryName != homeCategoryName()) {
        switch (m_selectedSortOrderIndex) {
            case 0:
                std::stable_sort(m_sortedOffers.begin(), m_sortedOffers.end(),
                                 [](const auto& a, const auto& b) { return a.popularity > b.popularity; });
                break;
            case 1:
                std::stable_sort(m_sortedOffers.begin(), m_sortedOffers.end(),
                                 [](const auto& a, const auto& b) { return a.name.localeAwareCompare(b.name) < 0; });
                break;
            case 2:
                std::stable_sort(m_sortedOffers.begin(), m_sortedOffers.end(),
                                 [](const auto& a, const auto& b) { return a.newUntil > b.newUntil; });
                break;
            default: break;
        }
    }
    const QList<QObject*> previous = m_availableOffers;
    m_availableOffers.clear();
    for (const store::TStoreOfferData& offer : m_sortedOffers)
        m_availableOffers.append(new store::TQmlStoreOffer(offer, this));
    m_selectedOfferIndex = m_availableOffers.isEmpty() ? -1 : 0;
    Q_EMIT availableOffersChanged();
    Q_EMIT selectedOfferIndexChanged();
    deleteObjectsLater(previous);
}

// b1/762f9dfe671e0bc5
void TStoreDialogController::selectOffer(const int offerId)
{
    for (int i = 0; i < m_availableOffers.size(); ++i) {
        if (static_cast<store::TQmlStoreOffer*>(m_availableOffers[i])->hasQuantity(offerId)) {
            setSelectedOfferIndex(i);
            break;
        }
    }
    m_pendingJumpOfferId = 0;
}

// b1/946b0ac7d2d16e71
void TStoreDialogController::rebuildFeaturedOffers()
{
    if (!m_storage)
        return;
    m_featuredOffers.clear();
    for (const QString& banner : m_storage->banners())
        m_featuredOffers.append(m_imageManager->url(banner).toString());
    m_featuredOfferIds = m_storage->bannerOfferIds();
    // CIP takes the delay as is; a 0 would make StoreMainPage's timer spin.
    m_displayTimePerFeature = std::max(1, m_storage->bannerDelaySeconds()) * 1000;
    Q_EMIT featuredOffersChanged();
    Q_EMIT displayTimePerFeatureChanged();
    m_featuredOfferIndex = m_featuredOffers.isEmpty() ? -1 : QRandomGenerator::global()->bounded(static_cast<int>(m_featuredOffers.size()));
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::setSelectedCategoryName(const QString& name)
{
    if (m_selectedCategoryName == name)
        return;
    m_selectedCategoryName = name;
    Q_EMIT selectedCategoryChanged();
}

TStoreButtonController::TStoreButtonController(TStoreDialogController* dialog, QObject* parent)
    : cip::tibia::gamewindow::TStoreButtonController(parent)
    , m_dialog(dialog)
{
    if (dialog && dialog->messages())
        connect(dialog->messages(), &store::TStoreProtocolMessageHandler::storeButtonIndicatorsChanged, this,
                &TStoreButtonController::onStoreButtonIndicatorsChanged);

    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, [this] { onHighlightStoreButtonEnds(false); });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onStoreButtonDeeplink", [this](GameObject*, const std::vector<std::any>& args) {
        onStoreDeeplinkChanged(static_cast<int>(client::gamecore::argInt(args, 0)),
                               static_cast<int>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        clearDeeplink();
    });
}

// b1/51a9654f33399e5a
void TStoreButtonController::onOpenStoreButtonClicked()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = m_deeplinkAction;
    action->instance = m_deeplinkDetail;
    input::publish(action);
    onRequestOpenStoreDialog();
}

// b1/e7a42ac26a112044
void TStoreButtonController::onStoreDeeplinkChanged(const int openAction, const int detail)
{
    if (openAction == 0) {
        clearDeeplink();
        return;
    }
    m_deeplinkAction = openAction;
    m_deeplinkDetail = detail;
    m_highlightTimer.start();
    Q_EMIT changed();
}

// b1/d2c24f6e4ff984dc
void TStoreButtonController::onHighlightStoreButtonEnds(bool)
{
    clearDeeplink();
}

void TStoreButtonController::clearDeeplink()
{
    m_highlightTimer.stop();
    m_deeplinkAction = 0;
    m_deeplinkDetail = -1;
    Q_EMIT changed();
}

void TStoreButtonController::onStoreButtonIndicatorsChanged(const bool SaleFlag, const bool NewFlag)
{
    m_saleFlag = SaleFlag;
    m_newFlag = NewFlag;
    Q_EMIT changed();
}

// b1/48ef7efb555aab4e
void TStoreButtonController::onRequestOpenStoreDialog()
{
    clearDeeplink();
}

}
