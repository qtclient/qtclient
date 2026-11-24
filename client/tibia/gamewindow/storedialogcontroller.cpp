#include "storedialogcontroller.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/marketcointransactiondetailsdialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameactionhandlers.h"
#include "store/dynamicallyloadedimage.h"
#include "store/qmlstoreobjects.h"
#include "store/storeciptypes.h"
#include "game/gamesessionbase.h"
#include "creatures/creaturestorage.h"

#include <client/const.h>

#include <QLocale>
#include <QRandomGenerator>

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

constexpr auto AskBeforeBuyingOption = "storeAskBeforeBuyingProducts";
constexpr auto ShowStoreMessagesOption = "gameWindowShowStoreMessages";
/// The transaction history's page size.
constexpr uint32_t TransactionHistoryEntriesPerPage = 26;
/// The name the server gives the home page.
const QString HomeCategoryName = QStringLiteral("Home");

bool optionBool(const char* name, const bool fallback)
{
    return app::ClientOptions::load().options().value(QLatin1String(name)).toBool(fallback);
}

/// One "OK" button (Return).
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

/// The store's sort order of a sort order label: popularity, name or newest.
int sortOrderOfLabel(const QString& label)
{
    if (label == qtTrId("store_sort_name"))
        return store::TStoreStorage::SORT_BY_NAME_ALTERNATIVE;
    if (label == qtTrId("store_sort_newest"))
        return store::TStoreStorage::SORT_BY_NEWEST;
    return store::TStoreStorage::SORT_BY_POPULARITY;
}

/// The same name, image identifiers and subcategories.
bool sameCategory(const QObject* left, const QObject* right)
{
    const auto* a = qobject_cast<const cip::tibia::store::TQmlStoreCategory*>(left);
    const auto* b = qobject_cast<const cip::tibia::store::TQmlStoreCategory*>(right);
    if (!a || !b)
        return a == b;
    if (a->name() != b->name() || a->imageIdentifiers() != b->imageIdentifiers())
        return false;
    const QList<QObject*> left_ = a->subcategoryList();
    const QList<QObject*> right_ = b->subcategoryList();
    if (left_.size() != right_.size())
        return false;
    for (qsizetype i = 0; i < left_.size(); ++i) {
        if (!sameCategory(left_[i], right_[i]))
            return false;
    }
    return true;
}

store::TStoreOpenParameters openParameters(const int openAction, const int serviceType)
{
    store::TStoreOpenParameters parameters;
    parameters.openAction = openAction;
    parameters.serviceType = serviceType;
    return parameters;
}

input::IGameActionPtr textMessageAction(const input::EGameAction type, const QString& text)
{
    auto message = std::make_shared<input::TGameActionAddTextMessage>(type, text, QColor(0x18, 0x72, 0xc3));
    return message;
}

}

TStorePurchaseConfirmationDialogController::TStorePurchaseConfirmationDialogController(store::TDynamicallyLoadedImageManager* imageManager,
                                                                                       QObject* parent)
    : TDialogController(QStringLiteral("StorePurchaseConfirmationDialog.qml"), parent)
    , m_imageManager(imageManager)
{}

void TStorePurchaseConfirmationDialogController::setOffer(const int offerId, const store::TStoreOffer& offer,
                                                          store::TStoreDescriptionManager* descriptionManager,
                                                          const bool smoothTextureFiltering, const bool showAskAgainCheckbox,
                                                          const bool buyWithCosmeticTokens)
{
    dialog()->close();
    if (m_offer)
        m_offer->deleteLater();
    m_offer = new store::TQmlStoreOffer(offer, descriptionManager);
    m_offerToPurchase = m_offer;
    m_offerID = offerId;
    m_smoothTextureFiltering = smoothTextureFiltering;
    m_showAskAgainCheckbox = showAskAgainCheckbox;
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
}

void TStorePurchaseConfirmationDialogController::showDialog(const int offerId, const store::TStoreOffer& offer,
                                                            store::TStoreDescriptionManager* descriptionManager,
                                                            const bool smoothTextureFiltering, const bool showAskAgainCheckbox,
                                                            const bool buyWithCosmeticTokens)
{
    setOffer(offerId, offer, descriptionManager, smoothTextureFiltering, showAskAgainCheckbox, buyWithCosmeticTokens);
    dialog()->create();
    dialog()->show();
}

void TStorePurchaseConfirmationDialogController::showHirelingPurchaseDialog(const int offerId, const store::TStoreOffer& offer, const int sex,
                                                                            store::TStoreDescriptionManager* descriptionManager,
                                                                            const bool smoothTextureFiltering,
                                                                            const bool showAskAgainCheckbox,
                                                                            const bool buyWithCosmeticTokens)
{
    setOffer(offerId, offer, descriptionManager, smoothTextureFiltering, showAskAgainCheckbox, buyWithCosmeticTokens);
    if (auto* visualisation = qobject_cast<store::TQmlStoreVisualisation*>(m_offer->visualisation())) {
        visualisation->setGenderToShow(sex != 1 ? store::TQmlStoreVisualisation::FEMALE : store::TQmlStoreVisualisation::MALE);
    }
    dialog()->create();
    dialog()->show();
}

void TStorePurchaseConfirmationDialogController::purchaseRequested(const bool AskForConfirmationAgainForOtherPurchases)
{
    dialog()->close();
    Q_EMIT confirmPurchase(m_offerID, AskForConfirmationAgainForOtherPurchases);
}

void TStorePurchaseConfirmationDialogController::closeRequested()
{
    dialog()->close();
}

QVariantMap TStorePurchaseConfirmationDialogController::initialDialogProperties() const
{
    QVariantMap properties = TDialogController::initialDialogProperties();
    properties.insert(QStringLiteral("dynamicallyLoadedImageManager"), QVariant::fromValue<QObject*>(m_imageManager.data()));
    return properties;
}

TStorePurchaseSuccessDialogController::TStorePurchaseSuccessDialogController(QObject* parent)
    : TDialogController(QStringLiteral("StorePurchaseSuccessDialog.qml"), parent)
{}

void TStorePurchaseSuccessDialogController::requestClose()
{
    dialog()->close();
}

void TStorePurchaseSuccessDialogController::showDialog(const QString& Message)
{
    m_message = Message;
    dialog()->create();
    dialog()->show();
}

TTransferCreditsDialogController::TTransferCreditsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("TransferCreditsDialog.qml"), parent)
{
    m_maximumNameLength = 29;
}

void TTransferCreditsDialogController::showDialog(const qint64 transferableCredits, const qint64 creditPacketSize)
{
    dialog()->close();
    m_transferableCredits = transferableCredits;
    m_creditPacketSize = creditPacketSize;
    dialog()->create();
    dialog()->show();
}

void TTransferCreditsDialogController::requestTransfer(const QString& Receipient, const int TransferAmount)
{
    dialog()->close();
    Q_EMIT transferRequested(Receipient, TransferAmount);
}

void TTransferCreditsDialogController::requestCancel()
{
    dialog()->close();
}

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
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

void THirelingConfigurationDialogController::requestClose()
{
    dialog()->close();
}

void THirelingConfigurationDialogController::requestHireling(const QString& HirelingName, const int Sex)
{
    if (HirelingName.isEmpty())
        return;
    Q_EMIT hirelingConfigured(m_offerId, HirelingName, Sex);
    dialog()->close();
}

TStoreDialogController::TStoreDialogController(std::shared_ptr<store::TDynamicallyLoadedImageManager> DynamicallyLoadedImageManager,
                                               config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                                               QObject* parent)
    : TDialogController(QStringLiteral("StoreDialog.qml"), Engine, DialogParentItem, parent)
    , m_imageManager(std::move(DynamicallyLoadedImageManager))
    , m_descriptionManager(new store::TStoreDescriptionManager(this))
    , m_clientOptions(ClientOptions)
    , m_confirmationDialog(m_imageManager.get())
    , m_newCharacterNameDialog(new TGenericEnterTextDialogController(this))
    , m_worldTransferDialog(new TCharacterWorldTransferDialogController(this))
    , m_mainCharChangeDialog(new TMainCharacterChangeDialogController(this))
    , m_extraConfirmationDialog(new TMessageDialogController(this))
    , m_messageDialog(new TMessageDialogController(this))
    , m_searchMessageDialog(new TMessageDialogController(this))
    , m_transactionDetailsDialog(new TMarketCoinTransactionDetailsDialogController(this))
    , m_hirelingNameChange(new THirelingNameChangeController(this))
{
    m_dynamicallyLoadedImageManager = m_imageManager.get();
    m_searchResultCategory = new store::TQmlStoreCategory(qtTrId("store_search_result_catergory_name"), {}, {}, this);
    m_selectedCategoryName = HomeCategoryName;
    m_selectedOfferIndex = -1;
    m_featuredOfferIndex = -1;

    m_newCharacterNameDialog->setCaption(qtTrId("enter_text_new_character_caption"));
    m_newCharacterNameDialog->setDescription(qtTrId("enter_text_new_character_description"));
    m_newCharacterNameDialog->setMaxInputLength(29);
    connect(m_newCharacterNameDialog, &TGenericEnterTextDialogController::textEntered, this,
            &TStoreDialogController::onNewCharacterNameEntered);
    connect(m_worldTransferDialog, &TCharacterWorldTransferDialogController::targetWorldConfirmed, this,
            &TStoreDialogController::onCharacterWorldTransferWorldEntered);
    connect(m_mainCharChangeDialog, &TMainCharacterChangeDialogController::mainCharChangeConfirmed, this,
            [this](const uint32_t offerId, const QString& characterName) {
                onMainCharChangeConfigurationFinished(static_cast<int>(offerId), characterName);
            });
    connect(&m_hirelingDialog, &THirelingConfigurationDialogController::hirelingConfigured, this,
            &TStoreDialogController::onHirelingConfigurationFinished);
    connect(&m_transferDialog, &TTransferCreditsDialogController::transferRequested, this,
            &TStoreDialogController::onTransferCoinsReceipientAndAmountSpecified);
    connect(&m_confirmationDialog, &TStorePurchaseConfirmationDialogController::confirmPurchase, this,
            &TStoreDialogController::onPurchaseConfirmed);
    connect(&m_viewEventTimer, &QTimer::timeout, this, &TStoreDialogController::onViewEventTimeoutElapsed);
    connect(m_extraConfirmationDialog, &TMessageDialogController::acceptClicked, this, &TStoreDialogController::onExtraConfirmDialogClickedOK);
    connect(m_extraConfirmationDialog, &TMessageDialogController::discardClicked, this,
            &TStoreDialogController::onExtraConfirmDialogClickedNotOK);
    m_messageDialog->setCloseOnButtonClicked(true);
    m_messageDialog->setTextCentered(true);
    m_searchMessageDialog->setCloseOnButtonClicked(true);
    m_viewEventTimer.setInterval(2000);
    m_viewEventTimer.setSingleShot(true);

    // The hireling rename prompt's SET_HIRELING_NAME goes to the router (the session's player game action handler sends it).
    connect(m_hirelingNameChange, &THirelingNameChangeController::publishGameAction, this,
            &TStoreDialogController::publishGameAction);

    m_gameActions.on(input::EGameAction::CLOSE_STORE_WAIT_OR_ERROR_MESSAGE_DIALOG, [this](const input::IGameAction&) {
        m_messageDialog->dialog()->close();
        return true;
    });
}

TStoreDialogController::~TStoreDialogController()
{
    closeDialog();
}

void TStoreDialogController::open()
{
    openWith(openParameters(store::OPEN_HOME, 0));
}

void TStoreDialogController::openWith(const store::TStoreOpenParameters& parameters)
{
    openStore(parameters);
}

void TStoreDialogController::openStore(const store::TStoreOpenParameters& parameters)
{
    if (m_messages)
        m_messages->openStore(parameters);
}

/// The session's store handler and storage: closes the store and connects
/// to the new session's (none without a session).
void TStoreDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_storage = nullptr;
    m_gameSession = GameSession;
    m_hirelingNameChange->connectToGameSession(m_gameSession);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_messages = session ? session->storeProtocolMessageHandler().get() : nullptr;
    m_descriptionManager->setMessageHandler(m_messages);
    if (!session)
        return;
    store::TStoreProtocolMessageHandler* handler = m_messages;
    using Handler = store::TStoreProtocolMessageHandler;
    const auto add = [this](const QMetaObject::Connection& connection) { m_gameSessionConnections.add(connection); };
    add(connect(handler, &Handler::requestOpenStoreDialog, this, &TStoreDialogController::showDialog));
    add(connect(handler, &Handler::storeImageBaseUrlChanged, this, &TStoreDialogController::onStoreImageBaseUrlChanged));
    add(connect(handler, &Handler::requestCharacterNameChangeInputDialog, this, &TStoreDialogController::onNewCharacterNameRequested));
    add(connect(handler, &Handler::requestCharacterWorldChangeInputDialog, this,
                &TStoreDialogController::onCharacterWorldTransferWorldRequested));
    add(connect(handler, &Handler::requestMainCharacterChangeInputDialog, this, &TStoreDialogController::onMainCharChangeRequested));
    add(connect(handler, &Handler::requestHirelingInputDialog, this, &TStoreDialogController::onHirelingConfigurationRequested));
    add(connect(handler, &Handler::requestConfirmationDialog, this, &TStoreDialogController::onConfirmationDialogRequested));
    add(connect(handler, &Handler::requestPurchaseSuccessNotification, this, &TStoreDialogController::onPurchaseSuccessNotification));
    add(connect(handler, &Handler::requestTransferSuccessNotification, this, &TStoreDialogController::onTransferSuccessNotification));
    add(connect(handler, &Handler::requestPurchaseErrorNotification, this, &TStoreDialogController::onPurchaseErrorNotification));
    add(connect(handler, &Handler::requestTransferErrorNotification, this, &TStoreDialogController::onTransferErrorNotification));
    add(connect(handler, &Handler::requestTransactionHistoryErrorNotification, this,
                &TStoreDialogController::onTransactionHistoryErrorNotification));
    add(connect(handler, &Handler::requestInformationNotification, this, &TStoreDialogController::onInformationNotification));

    m_storage = session->storeStorage().get();
    store::TStoreStorage* storage = m_storage;
    using Storage = store::TStoreStorage;
    add(connect(storage, &Storage::offersChanged, this, &TStoreDialogController::onOffersChanged));
    add(connect(storage, &Storage::homePageChanged, this, &TStoreDialogController::onHomePageChanged));
    add(connect(storage, &Storage::searchResultsChanged, this, &TStoreDialogController::onSearchResultsChanged));
    add(connect(storage, &Storage::emptySearchResultReceived, this, &TStoreDialogController::onEmptySearchResultReceived));
    add(connect(storage, &Storage::transactionHistoryChanged, this, &TStoreDialogController::onTransactionHistoryChanged));
    add(connect(storage, &Storage::sortOrderChanged, this, &TStoreDialogController::selectedSortOrderIndexChanged));
    add(connect(storage, &Storage::activeFilterTagChanged, this, &TStoreDialogController::rebuildAvailableOffers));
    add(connect(storage, &Storage::transactionDetailsLoaded, this, &TStoreDialogController::onTransactionDetailsChanged));
}

void TStoreDialogController::onTransactionDetailsChanged()
{
    if (!m_storage)
        return;
    // Only the details of the transaction the dialog asked for.
    if (m_transactionDetailsId == static_cast<int>(m_storage->transactionDetails().transactionId))
        m_transactionDetailsDialog->setTransactionDetails(m_storage->transactionDetails());
}

void TStoreDialogController::showDialog()
{
    onCategoriesChanged();
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
        m_openedAt = std::chrono::steady_clock::now();
        m_opened = true;
    }
    if (m_messages)
        m_messages->setRefreshOffersAfterPurchase(true);
}

void TStoreDialogController::closeDialog()
{
    m_descriptionManager->clear();
    QObject* transactionPage = m_transactionPage;
    m_transactionPage = nullptr;
    if (transactionPage)
        transactionPage->deleteLater();
    m_transactionHistoryLoading = false;
    const QList<QObject*> categories = m_categoryList;
    m_categoryList.clear();
    Q_EMIT categoryListChanged();
    const QList<QObject*> offers = m_availableOffers;
    m_availableOffers.clear();
    Q_EMIT availableOffersChanged();
    m_selectedOfferIndex = -1;
    m_offerViewReported = false;
    m_offerIdForNameChange = 0;
    m_viewEventTimer.stop();
    dialog()->close();
    m_confirmationDialog.dialog()->close();
    m_newCharacterNameDialog->dialog()->close();
    m_worldTransferDialog->closeDialog();
    m_mainCharChangeDialog->closeDialog();
    m_hirelingDialog.dialog()->close();
    m_transferDialog.dialog()->close();
    m_messageDialog->dialog()->close();
    m_searchMessageDialog->dialog()->close();
    m_successDialog.dialog()->close();
    m_extraConfirmationDialog->dialog()->close();
    deleteObjectsLater(categories);
    deleteObjectsLater(offers);
    if (m_storage) {
        m_storage->setCategoriesLoaded(false);
        m_storage->setSortOrder(store::TStoreStorage::SORT_BY_POPULARITY);
    }
    if (m_messages) {
        if (m_opened) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - m_openedAt).count();
            const uint64_t seconds = elapsed < -999999 ? 0 : static_cast<uint64_t>(elapsed / 1000000) / 1000;
            m_messages->sendStoreEventVisitDuration(static_cast<uint32_t>(seconds));
            m_opened = false;
        }
        m_messages->setRefreshOffersAfterPurchase(false);
    }
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setPointingHand(false);
    m_transactionDetailsDialog->closeDialog();
}

void TStoreDialogController::requestClose()
{
    closeDialog();
}

void TStoreDialogController::onStoreImageBaseUrlChanged(const QUrl& baseUrl)
{
    m_imageManager->setBaseUrl(baseUrl);
}

void TStoreDialogController::onCategoriesChanged()
{
    QList<QObject*> categories;
    categories.append(new store::TQmlStoreCategory(HomeCategoryName, {}, {}));
    if (m_storage) {
        for (const store::TStoreCategory& category : m_storage->categories()) {
            QList<QObject*> subcategories;
            for (const store::TStoreCategory& subcategory : category.subcategories)
                subcategories.append(new store::TQmlStoreCategory(subcategory.name, subcategory.icons, {}));
            categories.append(new store::TQmlStoreCategory(category.name, category.icons, subcategories));
        }
    }
    bool changed = categories.size() != m_categoryList.size();
    for (qsizetype i = 0; !changed && i < categories.size(); ++i)
        changed = !sameCategory(categories[i], m_categoryList[i]);
    if (!changed) {
        deleteObjectsLater(categories);
        return;
    }
    const QList<QObject*> previous = m_categoryList;
    m_categoryList = categories;
    Q_EMIT categoryListChanged();
    deleteObjectsLater(previous);
}

void TStoreDialogController::onHomePageChanged()
{
    if (!m_storage)
        return;
    setSelectedCategoryName(HomeCategoryName);
    rebuildAvailableOffers();
    updateFeaturedOffers();
}

void TStoreDialogController::onOffersChanged(int preselectedOfferId)
{
    if (!m_storage)
        return;
    if (const store::TStoreCategory* category = m_storage->currentCategory())
        setSelectedCategoryName(category->name);
    rebuildAvailableOffers();
    if (preselectedOfferId != 0 || (preselectedOfferId = m_offerIdToSelect) != 0)
        selectOfferWithId(preselectedOfferId);
}

void TStoreDialogController::onSearchResultsChanged(const bool moreResultsAvailable)
{
    if (!m_storage)
        return;
    setSelectedCategoryName(qtTrId("store_search_result_catergory_name"));
    rebuildAvailableOffers();
    if (m_offerIdToSelect != 0)
        selectOfferWithId(m_offerIdToSelect);
    if (moreResultsAvailable && m_messages && !m_messages->searchRefreshPending())
        showMessage(m_searchMessageDialog, qtTrId("store_information"),
                    qtTrId("store_search_result_more_results_available").arg(m_storage->searchText()));
}

void TStoreDialogController::onEmptySearchResultReceived()
{
    if (!m_messages)
        return;
    m_messages->openStore(openParameters(store::OPEN_HOME, 0));
    if (!m_messages->searchRefreshPending() && m_storage)
        showMessage(m_searchMessageDialog, qtTrId("store_information"), qtTrId("store_search_result_empty").arg(m_storage->searchText()));
}

void TStoreDialogController::onTransactionHistoryChanged()
{
    if (m_storage) {
        QObject* previous = m_transactionPage;
        m_transactionPage = new store::TQmlStoreTransactionHistoryPage(m_storage->transactionHistory());
        if (previous)
            previous->deleteLater();
    }
    setSelectedCategoryName(QString());
    m_transactionHistoryLoading = false;
    Q_EMIT transactionHistoryLoadingChanged();
    Q_EMIT transactionPageChanged();
}

void TStoreDialogController::onNewCharacterNameRequested(const int offerId, const bool buyWithCosmeticTokens)
{
    m_messageDialog->dialog()->close();
    m_offerIdForNameChange = offerId;
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    m_newCharacterNameDialog->setText(QString());
    m_newCharacterNameDialog->dialog()->create();
    m_newCharacterNameDialog->dialog()->show();
}

void TStoreDialogController::onNewCharacterNameEntered(const QString& name)
{
    if (!m_messages || !m_storage)
        return;
    m_purchaseType = PURCHASE_NEW_CHARACTER_NAME;
    m_newName = name;
    if (const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(m_offerIdForNameChange)))
        m_confirmationDialog.showDialog(m_offerIdForNameChange, *offer, m_descriptionManager, smoothTextureFiltering(), false,
                                        m_buyWithCosmeticTokens);
}

void TStoreDialogController::onCharacterWorldTransferWorldRequested(const int offerId,
                                                                    const store::TWorldTransferInformation& worldTransferInformation,
                                                                    const bool buyWithCosmeticTokens)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_messageDialog->dialog()->close();
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    const std::shared_ptr<creatures::TCreature> player = session->playerCreature().lock();
    m_worldTransferDialog->showDialog(offerId, player ? player->name() : QString(), worldTransferInformation);
}

void TStoreDialogController::onMainCharChangeRequested(const int offerId,
                                                       const std::vector<store::TPossibleMainCharacterInformation>& possibleMainCharInfo,
                                                       const bool buyWithCosmeticTokens)
{
    if (!m_gameSession)
        return;
    m_messageDialog->dialog()->close();
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    m_mainCharChangeDialog->showDialog(static_cast<uint32_t>(offerId), possibleMainCharInfo);
}

void TStoreDialogController::onCharacterWorldTransferWorldEntered(const int offerId, const QString& worldName)
{
    if (!m_messages || !m_storage)
        return;
    m_purchaseType = PURCHASE_NEW_WORLD;
    m_newWorldName = worldName;
    if (const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(offerId)))
        m_confirmationDialog.showDialog(offerId, *offer, m_descriptionManager, smoothTextureFiltering(), false,
                                        m_buyWithCosmeticTokens);
}

void TStoreDialogController::onMainCharChangeConfigurationFinished(const int offerId, const QString& characterName)
{
    if (!m_messages || !m_storage)
        return;
    m_purchaseType = PURCHASE_NEW_MAIN_CHARACTER;
    m_newMainCharacter = characterName;
    if (const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(offerId)))
        m_confirmationDialog.showDialog(offerId, *offer, m_descriptionManager, smoothTextureFiltering(), false,
                                        m_buyWithCosmeticTokens);
}

void TStoreDialogController::onConfirmationDialogRequested(const int offerId, const QString& message, const bool buyWithCosmeticTokens)
{
    if (!m_messages)
        return;
    m_messageDialog->dialog()->close();
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    using Button = TMessageDialogController::Button;
    m_extraConfirmationDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), Button::OK, QKeySequence()),
                                            new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) });
    m_extraConfirmationDialog->showMessage(qtTrId("store_extra_confirmation_caption"), message);
    m_extraConfirmationOfferId = offerId;
}

void TStoreDialogController::onHirelingConfigurationRequested(const int offerId, const bool buyWithCosmeticTokens)
{
    m_messageDialog->dialog()->close();
    m_buyWithCosmeticTokens = buyWithCosmeticTokens;
    m_hirelingDialog.showDialog(offerId);
}

void TStoreDialogController::onHirelingConfigurationFinished(const int offerId, const QString& name, const int sex)
{
    if (!m_messages || !m_storage)
        return;
    m_purchaseType = PURCHASE_HIRELING;
    m_newName = name;
    m_hirelingSex = sex;
    if (const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(offerId)))
        m_confirmationDialog.showHirelingPurchaseDialog(offerId, *offer, sex, m_descriptionManager, smoothTextureFiltering(), false,
                                                        m_buyWithCosmeticTokens);
}

void TStoreDialogController::onTransferCoinsReceipientAndAmountSpecified(const QString& recipient, const int amount)
{
    if (m_messages)
        m_messages->transferCredits(recipient, amount);
}

void TStoreDialogController::onPurchaseSuccessNotification(const QString& message)
{
    m_messageDialog->dialog()->close();
    if (!m_messages || !tibia::game::hasLocalPlayer())
        return;
    if ((tibia::game::localPlayerStates() & tibia::client::IconSwords) == 0) {
        m_successDialog.showDialog(message.trimmed());
        return;
    }
    if (optionBool(ShowStoreMessagesOption, true))
        Q_EMIT publishGameAction(textMessageAction(input::EGameAction::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, message.trimmed()));
    Q_EMIT publishGameAction(textMessageAction(input::EGameAction::ADD_SYSTEM_CHAT_MESSAGE, message.trimmed()));
}

void TStoreDialogController::onTransferSuccessNotification(const QString& message)
{
    showMessage(m_messageDialog, qtTrId("store_success_caption"), message);
}

void TStoreDialogController::onPurchaseErrorNotification(const QString& message)
{
    showMessage(m_messageDialog, qtTrId("store_purchase_failed"), message);
    if (m_storage)
        m_storage->setKeepActiveFilterTag(false);
    m_offerIdToSelect = 0;
}

void TStoreDialogController::onTransferErrorNotification(const QString& message)
{
    showMessage(m_messageDialog, qtTrId("store_transfer_failed"), message);
}

void TStoreDialogController::onTransactionHistoryErrorNotification(const QString& message)
{
    showMessage(m_messageDialog, qtTrId("store_history_error"), message);
    m_transactionHistoryLoading = false;
    Q_EMIT transactionHistoryLoadingChanged();
    m_transactionDetailsDialog->closeDialog();
}

void TStoreDialogController::onInformationNotification(const QString& message)
{
    showMessage(m_messageDialog, qtTrId("store_information"), message);
}

void TStoreDialogController::onPurchaseConfirmed(const int offerId, const bool askForConfirmationAgainForOtherPurchases)
{
    if (!m_messages)
        return;
    if (!m_offerViewReported)
        sendOfferViewedEvents(offerId);
    const auto id = static_cast<store::TOfferID>(offerId);
    switch (m_purchaseType) {
        case PURCHASE_PLAIN: m_messages->buyOffer(id, m_buyWithCosmeticTokens); break;
        case PURCHASE_NEW_CHARACTER_NAME: m_messages->buyOfferWithNewCharacterName(id, m_newName, m_buyWithCosmeticTokens); break;
        case PURCHASE_NEW_WORLD: m_messages->buyOfferWithNewWorld(id, m_newWorldName, m_buyWithCosmeticTokens); break;
        case PURCHASE_HIRELING: m_messages->buyHireling(id, m_newName, m_hirelingSex, m_buyWithCosmeticTokens); break;
        case PURCHASE_NEW_MAIN_CHARACTER: m_messages->buyOfferWithNewMainCharacter(id, m_newMainCharacter, m_buyWithCosmeticTokens); break;
        case PURCHASE_WITHOUT_CONFIRMATION: m_messages->buyOfferWithoutConfirmation(id, m_buyWithCosmeticTokens); break;
        default: break;
    }
    showProcessingOrderMessage();
    m_purchaseType = PURCHASE_PLAIN;
    m_offerViewReported = false;
    m_viewEventTimer.stop();
    if (m_storage)
        m_storage->setKeepActiveFilterTag(true);
    m_offerIdToSelect = offerId;
    app::ClientOptions::store(QLatin1String(AskBeforeBuyingOption), askForConfirmationAgainForOtherPurchases);
}

void TStoreDialogController::onViewEventTimeoutElapsed()
{
    const int index = m_selectedOfferIndex;
    if (index < 0 || index >= m_availableOffers.size() || m_offerViewReported || m_selectedCategoryName.isEmpty())
        return;
    if (m_selectedCategoryName == HomeCategoryName)
        return;
    const store::TStoreOffer& offer = static_cast<store::TQmlStoreOffer*>(m_availableOffers[index])->offer();
    sendOfferViewedEvents(offer.quantities.empty() ? 0 : static_cast<int>(offer.quantities.front().offerId));
    m_offerViewReported = true;
}

void TStoreDialogController::onExtraConfirmDialogClickedOK()
{
    m_purchaseType = PURCHASE_WITHOUT_CONFIRMATION;
    if (m_storage && m_storage->findOffer(static_cast<store::TOfferID>(m_extraConfirmationOfferId)))
        onPurchaseConfirmed(m_extraConfirmationOfferId, optionBool(AskBeforeBuyingOption, true));
    m_extraConfirmationDialog->dialog()->close();
}

void TStoreDialogController::onExtraConfirmDialogClickedNotOK()
{
    m_extraConfirmationDialog->dialog()->close();
}

QStringList TStoreDialogController::sortOrderModel() const
{
    return { qtTrId("store_sort_popularity"), qtTrId("store_sort_name"), qtTrId("store_sort_newest") };
}

int TStoreDialogController::selectedSortOrderIndex() const
{
    const QStringList model = sortOrderModel();
    for (qsizetype i = 0; m_storage && i < model.size(); ++i) {
        if (sortOrderOfLabel(model[i]) == m_storage->sortOrder())
            return static_cast<int>(i);
    }
    return 0;
}

void TStoreDialogController::setSelectedSortOrderIndex(const int index)
{
    const QStringList model = sortOrderModel();
    if (index < 0 || index >= model.size() || !m_storage)
        return;
    const int sortOrder = sortOrderOfLabel(model[index]);
    m_storage->setSortOrder(sortOrder);
    if (m_messages)
        m_messages->sendStoreEventSortOrder(sortOrder);
    rebuildAvailableOffers();
}

QStringList TStoreDialogController::filterTagsModel() const
{
    QStringList model{ qtTrId("store_filter_tag_all") };
    if (m_storage) {
        for (const QString& tag : m_storage->filterTags())
            model.append(qtTrId("store_filter_tag").arg(tag));
    }
    return model;
}

int TStoreDialogController::selectedFilterTagIndex() const
{
    if (!m_storage || m_storage->activeFilterTag().isEmpty())
        return 0;
    const QStringList model = filterTagsModel();
    const QString active = qtTrId("store_filter_tag").arg(m_storage->activeFilterTag());
    const auto index = model.indexOf(active);
    return index < 0 ? 0 : static_cast<int>(index);
}

void TStoreDialogController::setSelectedFilterTagIndex(const int index)
{
    const QStringList model = filterTagsModel();
    if (index < 0 || index >= model.size() || !m_storage)
        return;
    if (index == 0) {
        m_storage->setActiveFilterTag(QString());
    } else {
        QString tag = model[index];
        tag.replace(qtTrId("store_filter_tag").arg(QString()), QString());
        m_storage->setActiveFilterTag(tag);
        if (m_messages)
            m_messages->sendStoreEventFilterTag(tag);
    }
    rebuildAvailableOffers();
}

void TStoreDialogController::setSelectedOfferIndex(const int index)
{
    if (index < 0 || index >= m_availableOffers.size())
        return;
    if (m_selectedOfferIndex != index) {
        m_selectedOfferIndex = index;
        Q_EMIT selectedOfferIndexChanged();
    }
    m_offerViewReported = false;
    m_viewEventTimer.start();
}

bool TStoreDialogController::smoothTextureFiltering() const
{
    return m_clientOptions->antialiasingMode() == 1;
}

void TStoreDialogController::getNextFeaturedOffer()
{
    if (m_featuredOfferIndex == -1)
        return;
    m_featuredOfferIndex = m_featuredOfferIndex == static_cast<int>(m_featuredOffers.size()) - 1 ? 0 : m_featuredOfferIndex + 1;
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::getPreviousFeaturedOffer()
{
    if (m_featuredOfferIndex == -1)
        return;
    m_featuredOfferIndex = (m_featuredOfferIndex == 0 ? static_cast<int>(m_featuredOffers.size()) : m_featuredOfferIndex) - 1;
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::onFeaturedOfferClicked()
{
    if (!m_messages || !m_storage || m_featuredOfferIndex == -1)
        return;
    const std::vector<store::TStoreBanner>& banners = m_storage->banners();
    if (m_featuredOfferIndex >= static_cast<int>(banners.size()))
        return;
    m_messages->openStore(banners[static_cast<size_t>(m_featuredOfferIndex)].parameters);
}

void TStoreDialogController::requestJumpToOfferWithID(const int QmlOfferID)
{
    store::TStoreOpenParameters parameters = openParameters(store::OPEN_OFFER, 1);
    parameters.offerId = static_cast<store::TOfferID>(QmlOfferID);
    openStore(parameters);
}

void TStoreDialogController::selectCategory(const QString& CategoryName)
{
    if (CategoryName == HomeCategoryName) {
        openStore(openParameters(store::OPEN_HOME, 1));
        return;
    }
    store::TStoreOpenParameters parameters = openParameters(store::OPEN_CATEGORY, 1);
    parameters.category = CategoryName;
    openStore(parameters);
}

void TStoreDialogController::requestSearch(const QString& SearchString)
{
    store::TStoreOpenParameters parameters = openParameters(store::OPEN_SEARCH, 1);
    parameters.category = SearchString;
    openStore(parameters);
}

void TStoreDialogController::openGetCoinsUrl()
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_URL_GET_COINS));
}

void TStoreDialogController::openTransactionHistory()
{
    if (!m_messages)
        return;
    m_messages->openTransactionHistory(TransactionHistoryEntriesPerPage);
    onTransactionHistoryChanged();
    m_transactionHistoryLoading = true;
    Q_EMIT transactionHistoryLoadingChanged();
}

void TStoreDialogController::nextTransactionPage()
{
    if (m_messages && m_storage)
        m_messages->requestTransactionHistoryPage(m_storage->transactionHistory().currentPage + 1, TransactionHistoryEntriesPerPage);
}

void TStoreDialogController::previousTransactionPage()
{
    if (m_messages && m_storage)
        m_messages->requestTransactionHistoryPage(m_storage->transactionHistory().currentPage - 1, TransactionHistoryEntriesPerPage);
}

void TStoreDialogController::buyOffer(const int QmlOfferID)
{
    buyOffer(QmlOfferID, false);
}

void TStoreDialogController::buyOffer(const int QmlOfferID, const bool BuyWithCosmeticTokens)
{
    m_buyWithCosmeticTokens = BuyWithCosmeticTokens;
    if (!m_storage || !m_messages)
        return;
    const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(QmlOfferID));
    if (!offer)
        return;
    if (offer->configurable) {
        m_messages->buyOffer(static_cast<store::TOfferID>(QmlOfferID), BuyWithCosmeticTokens);
        showConfiguringOrderMessage();
        return;
    }
    if (optionBool(AskBeforeBuyingOption, true)) {
        m_purchaseType = PURCHASE_PLAIN;
        m_confirmationDialog.showDialog(QmlOfferID, *offer, m_descriptionManager, smoothTextureFiltering(), true, BuyWithCosmeticTokens);
        return;
    }
    if (!m_offerViewReported)
        sendOfferViewedEvents(QmlOfferID);
    m_messages->buyOffer(static_cast<store::TOfferID>(QmlOfferID), BuyWithCosmeticTokens);
    showProcessingOrderMessage();
    m_purchaseType = PURCHASE_PLAIN;
    m_offerViewReported = false;
    m_viewEventTimer.stop();
    m_storage->setKeepActiveFilterTag(true);
    m_offerIdToSelect = QmlOfferID;
}

void TStoreDialogController::tryOutfitForCurrentOffer()
{
    const int index = m_selectedOfferIndex;
    if (index < 0 || index >= m_availableOffers.size() || !m_storage || !m_messages)
        return;
    const store::TStoreOffer& selected = static_cast<store::TQmlStoreOffer*>(m_availableOffers[index])->offer();
    const store::TStoreOffer* offer =
        m_storage->findOffer(selected.quantities.empty() ? 0 : selected.quantities.front().offerId);
    if (!offer)
        return;
    const store::TStoreVisualisation& visualisation = offer->visualisation;
    if (visualisation.type != store::TStoreVisualisation::MOUNT && visualisation.type != store::TStoreVisualisation::OUTFIT
        && visualisation.type != store::TStoreVisualisation::BOTH_GENDER_OUTFITS)
        return;
    const uint16_t typeId = visualisation.type == store::TStoreVisualisation::BOTH_GENDER_OUTFITS
                                ? (visualisation.gender != store::TStoreVisualisation::MALE ? visualisation.femaleAppearanceTypeId
                                                                                            : visualisation.maleAppearanceTypeId)
                                : visualisation.appearanceTypeId;
    int mode = 0;
    if (offer->tryOnType == 1)
        mode = 1;
    else if (offer->tryOnType == 2)
        mode = 3;
    else
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionRequestOutfitsFromServer>(mode, static_cast<uint16_t>(typeId)));
}

void TStoreDialogController::requestTransferCredits()
{
    if (!m_storage)
        return;
    const qint64 transferable = m_storage->balance().value_or(store::TStoreBalance{}).confirmed;
    const qint64 packetSize = m_storage->creditPacketSize();
    if (transferable < packetSize) {
        showMessage(m_messageDialog, qtTrId("store_not_enough_coins_for_transfer_caption"), qtTrId("store_not_enough_coins_for_transfer"));
        return;
    }
    m_transferDialog.showDialog(transferable, packetSize);
}

void TStoreDialogController::requestTransactionDetailsAndShowPage(const int TransactionID)
{
    m_transactionDetailsDialog->showDialog();
    m_transactionDetailsId = TransactionID;
    auto action = std::make_shared<input::TGameActionRequestTransactionDetailsFromServer>(TransactionID);
    Q_EMIT publishGameAction(action);
}

void TStoreDialogController::requestTradeCharacter()
{
    Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_TRADE_CHARACTER_DIALOG));
}

void TStoreDialogController::rebuildAvailableOffers()
{
    if (!m_storage)
        return;
    const QList<QObject*> previous = m_availableOffers;
    m_availableOffers.clear();
    for (const store::TStoreOffer& offer : m_storage->filteredOffers())
        m_availableOffers.append(new store::TQmlStoreOffer(offer, m_descriptionManager));
    Q_EMIT filterTagsChanged();
    Q_EMIT selectedFilterTagIndexChanged();
    Q_EMIT availableOffersChanged();
    deleteObjectsLater(previous);
    if (m_availableOffers.isEmpty())
        return;
    if (m_selectedOfferIndex != 0) {
        m_selectedOfferIndex = 0;
        Q_EMIT selectedOfferIndexChanged();
    }
    m_offerViewReported = false;
    m_viewEventTimer.start();
}

void TStoreDialogController::selectOfferWithId(const int offerId)
{
    if (!m_availableOffers.isEmpty()) {
        int index = 0;
        for (qsizetype i = 0; i < m_availableOffers.size(); ++i) {
            if (static_cast<store::TQmlStoreOffer*>(m_availableOffers[i])->hasQuantity(offerId)) {
                index = static_cast<int>(i);
                break;
            }
        }
        if (m_selectedOfferIndex != index) {
            m_selectedOfferIndex = index;
            Q_EMIT selectedOfferIndexChanged();
        }
        m_offerViewReported = false;
        m_viewEventTimer.start();
    }
    m_offerIdToSelect = 0;
}

void TStoreDialogController::sendOfferViewedEvents(const int offerId)
{
    if (!m_storage || !m_messages)
        return;
    const store::TStoreOffer* offer = m_storage->findOffer(static_cast<store::TOfferID>(offerId));
    if (!offer)
        return;
    for (const store::TStoreOfferQuantityConfiguration& quantity : offer->quantities)
        m_messages->sendStoreEventOfferViewed(quantity.offerId);
}

void TStoreDialogController::showProcessingOrderMessage()
{
    showMessage(m_messageDialog, qtTrId("store_processing_order_caption"), qtTrId("store_processing_order"));
}

void TStoreDialogController::showConfiguringOrderMessage()
{
    showMessage(m_messageDialog, qtTrId("store_configuring_order_caption"), qtTrId("store_configuring_order"));
}

void TStoreDialogController::updateFeaturedOffers()
{
    const std::vector<store::TStoreBanner> banners = m_storage->banners();
    const QString baseUrl = m_imageManager->baseUrl().toString();
    m_featuredOffers.clear();
    for (const store::TStoreBanner& banner : banners)
        m_featuredOffers.append(baseUrl + QLatin1Char('/') + banner.image);
    Q_EMIT featuredOffersChanged();
    m_displayTimePerFeature = static_cast<int>(m_storage->bannerDelay()) * 1000;
    Q_EMIT displayTimePerFeatureChanged();
    m_featuredOfferIndex = banners.empty() ? -1 : QRandomGenerator::global()->bounded(static_cast<int>(banners.size()));
    Q_EMIT featuredOfferIndexChanged();
}

void TStoreDialogController::setSelectedCategoryName(const QString& name)
{
    if (m_selectedCategoryName == name)
        return;
    m_selectedCategoryName = name;
    Q_EMIT selectedCategoryChanged();
}

void TStoreDialogController::showMessage(TMessageDialogController* dialog, const QString& caption, const QString& message)
{
    dialog->setButtons(okButton());
    dialog->showMessage(caption, message);
}

TStoreButtonController::TStoreButtonController(TStoreDialogController* dialog, QObject* parent)
    : cip::tibia::gamewindow::TStoreButtonController(parent)
    , m_dialog(dialog)
{
    connect(&m_highlightTimer, &shared::THighlightTimer::changed, this, &TStoreButtonController::changed);
    connect(&m_highlightTimer, &shared::THighlightTimer::timeout, this, &TStoreButtonController::onHighlightStoreButtonEnds);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() { clearDeeplink(); });
}

/// The session's store handler's deep link, open requests and indicators.
void TStoreButtonController::connectToGameSession(const std::shared_ptr<game::IGameSession>& pGameSession)
{
    m_gameSession = pGameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    store::TStoreProtocolMessageHandler* handler = session->storeProtocolMessageHandler().get();
    connect(handler, &store::TStoreProtocolMessageHandler::storeButtonDeeplinkChanged, this, &TStoreButtonController::onStoreDeeplinkChanged);
    connect(handler, &store::TStoreProtocolMessageHandler::requestOpenStoreDialog, this, &TStoreButtonController::onRequestOpenStoreDialog);
    connect(handler, &store::TStoreProtocolMessageHandler::storeButtonIndicatorsChanged, this,
            &TStoreButtonController::onStoreButtonIndicatorsChanged);
}

void TStoreButtonController::onOpenStoreButtonClicked()
{
    // The deep link stays until the store asks to open its dialog.
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenStore>(m_deeplink));
}

void TStoreButtonController::onStoreDeeplinkChanged(const store::TStoreOpenParameters& StoreDeeplink)
{
    m_deeplink = StoreDeeplink;
    if (m_deeplink.openAction != 0) {
        m_highlightTimer.start(false);
        return;
    }
    m_highlightTimer.stop(false);
    m_deeplink = {};
}

void TStoreButtonController::onHighlightStoreButtonEnds(const bool Protected)
{
    m_highlightTimer.stop(Protected);
    m_deeplink = {};
}

void TStoreButtonController::clearDeeplink()
{
    m_highlightTimer.stop(true);
    m_deeplink = {};
}

void TStoreButtonController::onStoreButtonIndicatorsChanged(const bool SaleFlag, const bool NewFlag)
{
    if (m_saleFlag != SaleFlag) {
        m_saleFlag = SaleFlag;
        Q_EMIT changed();
    }
    if (m_saleFlag)
        m_highlightTimer.start(true);
    if (m_newFlag != NewFlag) {
        m_newFlag = NewFlag;
        Q_EMIT changed();
    }
    if (m_newFlag)
        m_highlightTimer.start(true);
}

void TStoreButtonController::onRequestOpenStoreDialog()
{
    clearDeeplink();
}

}
