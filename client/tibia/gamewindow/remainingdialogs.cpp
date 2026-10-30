#include "remainingdialogs.h"

#include "app/clientoptions.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>
#include <tibia/world/game.h>

#include <QColor>

#include <algorithm>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using Args = std::vector<std::any>;
using client::gamecore::argString;
constexpr auto AskBeforeStowOption = "askBeforeStowContainerContent";

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

QString vocationName(const uint8_t clientId)
{
    switch (clientId % 10) {
        case 1: return QStringLiteral("Knight");
        case 2: return QStringLiteral("Paladin");
        case 3: return QStringLiteral("Sorcerer");
        case 4: return QStringLiteral("Druid");
        case 5: return QStringLiteral("Monk");
        default: return QStringLiteral("None");
    }
}

QString worldTypeName(const uint8_t worldType)
{
    switch (worldType) {
        case 0: return QStringLiteral("Open PvP");
        case 1: return QStringLiteral("Optional PvP");
        case 2: return QStringLiteral("Hardcore PvP");
        case 3: return QStringLiteral("Retro Open PvP");
        case 4: return QStringLiteral("Retro Hardcore PvP");
        default: return {};
    }
}

QObject* tradeOutfitConfiguration(const Outfit& outfit, QObject* parent)
{
    auto* config = new QObject(parent);
    config->setProperty("id", static_cast<int>(outfit.getId()));
    config->setProperty("headColor", outfitColor(outfit.getHead()));
    config->setProperty("torsoColor", outfitColor(outfit.getBody()));
    config->setProperty("legsColor", outfitColor(outfit.getLegs()));
    config->setProperty("detailColor", outfitColor(outfit.getFeet()));
    config->setProperty("firstAddOn", (outfit.getAddons() & 1) != 0);
    config->setProperty("secondAddOn", (outfit.getAddons() & 2) != 0);
    return config;
}

} // namespace

TCharacterTradeNotificationModel::TCharacterTradeNotificationModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TCharacterTradeNotificationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TCharacterTradeNotificationModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case NotificationRole: return e.notification;
        case StatusRole: return e.status;
        default: return {};
    }
}

QHash<int, QByteArray> TCharacterTradeNotificationModel::roleNames() const
{
    return { { NotificationRole, "notification" }, { StatusRole, "status" } };
}

void TCharacterTradeNotificationModel::setEntries(std::vector<trade::TCharacterTradeConditionEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TCharacterTradeHighlightModel::TCharacterTradeHighlightModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TCharacterTradeHighlightModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TCharacterTradeHighlightModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case CategoryIconRole: return e.categoryIcon;
        case HighlightRole: return e.highlight;
        case HighlightIdRole: return e.highlightId;
        case CategoryRole: return e.category;
        default: return {};
    }
}

QHash<int, QByteArray> TCharacterTradeHighlightModel::roleNames() const
{
    return { { CategoryIconRole, "categoryicon" },
             { HighlightRole, "highlight" },
             { HighlightIdRole, "highlightid" },
             { CategoryRole, "category" } };
}

void TCharacterTradeHighlightModel::setEntries(std::vector<trade::TCharacterTradeHighlightEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TCharacterTradeItemModel::TCharacterTradeItemModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TCharacterTradeItemModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TCharacterTradeItemModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return e.id;
        case NameRole: return e.name;
        case AmountRole: return e.amount;
        case DescriptionRole: return e.description;
        case UpgradeTierRole: return e.upgradeTier;
        default: return {};
    }
}

QHash<int, QByteArray> TCharacterTradeItemModel::roleNames() const
{
    return { { IdRole, "id" },
             { NameRole, "name" },
             { AmountRole, "amount" },
             { DescriptionRole, "description" },
             { UpgradeTierRole, "upgradetier" } };
}

void TCharacterTradeItemModel::setEntries(std::vector<trade::TCharacterTradeItemEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TSoulSealsMonsterListModel::TSoulSealsMonsterListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TSoulSealsMonsterListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TSoulSealsMonsterListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case RaceIdRole:
        case RaceIDRole: return e.raceId;
        case NameRole: return e.name;
        case AnimusMasteryCompletedRole: return e.animusMasteryCompleted;
        case SoulPitFightPriceRole: return e.soulPitFightPrice;
        case TotalAmountRole: return e.soulPitFightPrice;
        case CategoryRole: return e.category;
        default: return {};
    }
}

QHash<int, QByteArray> TSoulSealsMonsterListModel::roleNames() const
{
    return { { RaceIdRole, "raceId" },
             { RaceIDRole, "raceID" },
             { NameRole, "name" },
             { AnimusMasteryCompletedRole, "animusMasteryCompleted" },
             { SoulPitFightPriceRole, "soulPitFightPrice" },
             { TotalAmountRole, "totalAmount" },
             { CategoryRole, "category" } };
}

void TSoulSealsMonsterListModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int TSoulSealsMonsterListModel::getRaceIDAtIndex(const int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_entries.size()))
        return 0;
    return m_entries[static_cast<size_t>(rowIndex)].raceId;
}

const TSoulSealsMonsterListModel::Entry* TSoulSealsMonsterListModel::entryForRace(const int raceId) const
{
    for (const Entry& e : m_entries) {
        if (e.raceId == raceId)
            return &e;
    }
    return nullptr;
}

TCharacterAuctionConfiguration::TCharacterAuctionConfiguration(QObject* parent)
    : cip::tibia::gamewindow::TCharacterAuctionConfiguration(parent)
{
    m_minimumMinimumBid = 50;
    m_minimumAuctionEndTimestamp = 0;
    m_maximumAuctionEndTimestamp = 0;
}

void TCharacterAuctionConfiguration::setSelectedHighlightCount(const int count)
{
    if (m_selectedHighlightCount == count)
        return;
    m_selectedHighlightCount = count;
    Q_EMIT selectedHighlightsChanged();
}

void TCharacterAuctionConfiguration::setSelectedItemCount(const int count)
{
    if (m_selectedItemCount == count)
        return;
    m_selectedItemCount = count;
    Q_EMIT selectedItemsChanged();
}

void TCharacterAuctionConfiguration::setAuctionBounds(const int minimumBid, const int minEnd, const int maxEnd)
{
    m_minimumMinimumBid = minimumBid;
    m_minimumAuctionEndTimestamp = minEnd;
    m_maximumAuctionEndTimestamp = maxEnd;
    Q_EMIT configurationChanged();
}

TCharacterTradeDialogController* TCharacterTradeDialogController::s_instance = nullptr;

TCharacterTradeDialogController::TCharacterTradeDialogController(QObject* parent)
    : TDialogController(QStringLiteral("charactertrade/CharacterTradeDialog.qml"), parent)
    , m_auction(new TCharacterAuctionConfiguration(this))
    , m_conditions(new TCharacterTradeNotificationModel(this))
    , m_highlights(new TCharacterTradeHighlightModel(this))
    , m_items(new TCharacterTradeItemModel(this))
    , m_confirmations(new TCharacterTradeNotificationModel(this))
    , m_overviewHighlights(new TCharacterTradeHighlightModel(this))
    , m_overviewItems(new TCharacterTradeItemModel(this))
{
    s_instance = this;
    m_auctionConfiguration = m_auction;
    m_characterData = new TQMLCharacterData(this);
    m_conditionsModel = m_conditions;
    m_highlightsModel = m_highlights;
    m_itemsModel = m_items;
    m_confirmationsModel = m_confirmations;
    m_characterOverviewHighlightsModel = m_overviewHighlights;
    m_characterOverviewItemsModel = m_overviewItems;

    if (trade::TCharacterTradeStorage* storage = trade::TCharacterTradeStorage::instance()) {
        connect(storage, &trade::TCharacterTradeStorage::infoUpdated, this, &TCharacterTradeDialogController::onStorageUpdated);
    }
}

TCharacterTradeDialogController* TCharacterTradeDialogController::instance()
{
    return s_instance;
}

void TCharacterTradeDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TCharacterTradeDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TCharacterTradeDialogController::closeButtonClicked()
{
    closeDialog();
}

void TCharacterTradeDialogController::requestClose()
{
    closeDialog();
}

void TCharacterTradeDialogController::addHighlight(const int HighlightID)
{
    m_selectedHighlightIds.insert(HighlightID);
    refreshOverviewModels();
}

void TCharacterTradeDialogController::removeHighlight(const int HighlightID)
{
    m_selectedHighlightIds.remove(HighlightID);
    refreshOverviewModels();
}

void TCharacterTradeDialogController::addItem(const int ItemID, const int UpgradeTier)
{
    m_selectedItems.insert({ ItemID, UpgradeTier });
    refreshOverviewModels();
}

void TCharacterTradeDialogController::removeItem(const int ItemID, const int UpgradeTier)
{
    m_selectedItems.remove({ ItemID, UpgradeTier });
    refreshOverviewModels();
}

void TCharacterTradeDialogController::requestConditions()
{
    g_game.requestCharacterTradeConfiguration(0);
}

void TCharacterTradeDialogController::requestCheckConfiguration()
{
    g_game.requestCharacterTradeConfiguration(1);
}

void TCharacterTradeDialogController::requestConfirmConfiguration()
{
    const int bid = m_auction ? m_auction->minimumBid() : 0;
    const int endTs = m_auction ? m_auction->auctionEndTimestamp() : 0;
    std::vector<std::tuple<uint16_t, uint8_t>> items;
    items.reserve(static_cast<size_t>(m_selectedItems.size()));
    for (const QPair<int, int>& item : m_selectedItems)
        items.emplace_back(static_cast<uint16_t>(item.first), static_cast<uint8_t>(item.second));
    std::vector<uint16_t> highlights;
    highlights.reserve(static_cast<size_t>(m_selectedHighlightIds.size()));
    for (const int id : m_selectedHighlightIds)
        highlights.push_back(static_cast<uint16_t>(id));
    g_game.confirmCharacterTradeConfiguration(static_cast<uint32_t>(std::max(0, bid)),
                                              static_cast<uint32_t>(std::max(0, endTs)), items, {}, highlights, true);
    closeDialog();
}

void TCharacterTradeDialogController::onStorageUpdated(const trade::TCharacterTradeStorage::DataType dataType)
{
    trade::TCharacterTradeStorage* storage = trade::TCharacterTradeStorage::instance();
    if (!storage)
        return;

    switch (dataType) {
        case trade::TCharacterTradeStorage::Conditions: {
            auto* character = static_cast<TQMLCharacterData*>(m_characterData);
            character->setCharacter(storage->characterName(), vocationName(storage->vocationClientId()), storage->characterLevel(),
                                    worldTypeName(storage->worldType()), tradeOutfitConfiguration(storage->outfit(), character));
            m_auction->setAuctionBounds(storage->minimumBid(), static_cast<int>(storage->startTimestamp()),
                                        static_cast<int>(storage->endTimestamp()));
            m_conditions->setEntries(storage->conditions());
            const bool unfulfilled = storage->hasUnfulfilledConditions();
            if (m_hasUnfulfilledConditions != unfulfilled) {
                m_hasUnfulfilledConditions = unfulfilled;
                Q_EMIT hasUnfulfilledConditionsChanged();
            }
            showDialog();
            break;
        }
        case trade::TCharacterTradeStorage::Items:
        case trade::TCharacterTradeStorage::StoreItems:
            m_items->setEntries(storage->items());
            break;
        case trade::TCharacterTradeStorage::Highlights:
            m_highlights->setEntries(storage->highlights());
            break;
        case trade::TCharacterTradeStorage::Summary: {
            m_confirmations->setEntries(storage->confirmations());
            const bool err = storage->hasConfirmationError();
            if (m_hasConfirmationError != err) {
                m_hasConfirmationError = err;
                Q_EMIT hasConfirmationErrorChanged();
            }
            break;
        }
    }
}

void TCharacterTradeDialogController::refreshOverviewModels()
{
    std::vector<trade::TCharacterTradeHighlightEntry> highlights;
    for (const trade::TCharacterTradeHighlightEntry& entry : m_highlights->entries()) {
        if (m_selectedHighlightIds.contains(entry.highlightId))
            highlights.push_back(entry);
    }
    m_overviewHighlights->setEntries(std::move(highlights));
    if (m_auction)
        m_auction->setSelectedHighlightCount(m_selectedHighlightIds.size());

    std::vector<trade::TCharacterTradeItemEntry> items;
    for (const trade::TCharacterTradeItemEntry& entry : m_items->entries()) {
        if (m_selectedItems.contains({ entry.id, entry.upgradeTier }))
            items.push_back(entry);
    }
    m_overviewItems->setEntries(std::move(items));
    if (m_auction)
        m_auction->setSelectedItemCount(m_selectedItems.size());
}

TCharacterTradeDuePaymentDialogController::TCharacterTradeDuePaymentDialogController(QObject* parent)
    : TDialogController(QStringLiteral("charactertrade/CharacterTradeDuePaymentDialog.qml"), parent)
{
    m_duePayments = new QObject(this);
}

void TCharacterTradeDuePaymentDialogController::onClosePressed(const bool DoNotShowAgain)
{
    Q_UNUSED(DoNotShowAgain);
    dialog()->close();
    Q_EMIT dialogClosed();
}

TCharacterWorldTransferDialogController::TCharacterWorldTransferDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CharacterWorldTransferDialog.qml"), parent)
{}

void TCharacterWorldTransferDialogController::setTransfer(const QString& characterName, const bool express,
                                                          QList<QObject*> conditions, QList<QObject*> worlds,
                                                          const bool canTransfer)
{
    m_characterName = characterName;
    m_isExpressWorldTransfer = express;
    m_conditions = std::move(conditions);
    m_worlds = std::move(worlds);
    m_canTransfer = canTransfer;
}

void TCharacterWorldTransferDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TCharacterWorldTransferDialogController::requestClose()
{
    closeDialog();
}

void TCharacterWorldTransferDialogController::requestWorldTransfer(const QString& TargetWorld)
{
    Q_EMIT worldTransferRequested(TargetWorld, m_isExpressWorldTransfer);
    closeDialog();
}

TCinematicSessiondumpPlayerController::TCinematicSessiondumpPlayerController(QObject* parent)
    : cip::tibia::gamewindow::TCinematicSessiondumpPlayerController(parent)
{}

void TCinematicSessiondumpPlayerController::onBenchmarkWindowClosed()
{
    // CIP empty moc slot: benchmark UI closed; sessiondump player stays owned by information hub.
}

TConfirmMoveObjectToStashDialogController::TConfirmMoveObjectToStashDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ConfirmMoveObjectToStashDialog.qml"), parent)
{
    m_showThisDialogAgain = app::ClientOptions::load().options().value(QLatin1String(AskBeforeStowOption)).toBool(true);
}

void TConfirmMoveObjectToStashDialogController::setConfirmHandler(std::function<void(bool)> onYes)
{
    m_onYes = std::move(onYes);
}

void TConfirmMoveObjectToStashDialogController::showDialog()
{
    m_showThisDialogAgain = app::ClientOptions::load().options().value(QLatin1String(AskBeforeStowOption)).toBool(true);
    dialog()->create();
    dialog()->show();
}

void TConfirmMoveObjectToStashDialogController::onYesClicked(const bool ShowDialogAgain)
{
    app::ClientOptions::store(QLatin1String(AskBeforeStowOption), ShowDialogAgain);
    m_showThisDialogAgain = ShowDialogAgain;
    if (m_onYes)
        m_onYes(ShowDialogAgain);
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TConfirmMoveObjectToStashDialogController::onNoClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TCrashReportDialogController::TCrashReportDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CrashReportDialog.qml"), parent)
{}

void TCrashReportDialogController::prepare(const QString& minimal, const QString& character, const QString& logs)
{
    m_minimalInformation = minimal;
    m_characterName = character;
    m_logMessages = logs;
}

void TCrashReportDialogController::sendReport(const QString& PlayerComment, const bool FullReport)
{
    // CIP CrashReportDialog.qml → sendReport; payload is owned by shared::TCrashReportSender.
    m_minimalInformation = FullReport ? m_logMessages : m_minimalInformation;
    if (!PlayerComment.isEmpty())
        m_logMessages = PlayerComment + QLatin1Char('\n') + m_logMessages;
    dialog()->close();
    Q_EMIT dialogClosed();
}

TEmergencyDialogController::TEmergencyDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ImportantBalancingMessageDialog.qml"), parent)
{
    m_showAgain = true;
}

void TEmergencyDialogController::showEmergency(const QVariant& data)
{
    m_dialogData = data;
    Q_EMIT dialogDataChanged();
    dialog()->create();
    dialog()->show();
}

void TEmergencyDialogController::onOKClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TGameSessionDisconnectReactionController::TGameSessionDisconnectReactionController(QObject* parent)
    : cip::tibia::gamewindow::TGameSessionDisconnectReactionController(parent)
{}

void TGameSessionDisconnectReactionController::requestLoginAgain()
{
    Q_EMIT loginAgainAfterLoginWaitElapsed();
}

void TGameSessionDisconnectReactionController::requestQuitAndPatch()
{
    Q_EMIT quitAndPatchClient();
}

TGenericConfirmWithDontShowAgainDialogController::TGenericConfirmWithDontShowAgainDialogController(QObject* parent)
    : TDialogController(QStringLiteral("GenericConfirmWithDontShowAgainDialog.qml"), parent)
{
    m_showThisDialogAgain = true;
}

void TGenericConfirmWithDontShowAgainDialogController::configure(const QString& caption, const QString& content,
                                                                 const QString& dontShowAgain, const bool showAgain)
{
    m_captionText = caption;
    m_contentText = content;
    m_dontShowAgainText = dontShowAgain;
    m_showThisDialogAgain = showAgain;
}

void TGenericConfirmWithDontShowAgainDialogController::setConfirmHandler(std::function<void(bool)> onYes)
{
    m_onYes = std::move(onYes);
}

void TGenericConfirmWithDontShowAgainDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TGenericConfirmWithDontShowAgainDialogController::onYesClicked(const bool ShowDialogAgain)
{
    if (m_onYes)
        m_onYes(ShowDialogAgain);
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TGenericConfirmWithDontShowAgainDialogController::onNoClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

THirelingNameChangeController* THirelingNameChangeController::s_instance = nullptr;

THirelingNameChangeController::THirelingNameChangeController(QObject* parent)
    : cip::tibia::gamewindow::THirelingNameChangeController(parent)
{
    s_instance = this;
}

THirelingNameChangeController* THirelingNameChangeController::instance()
{
    return s_instance;
}

TInfoBannerController::TInfoBannerController(QQuickItem* mapWindowPane, QObject* parent)
    : cip::tibia::gamewindow::TInfoBannerController(parent)
    , m_mapWindowPane(mapWindowPane)
{}

QQuickItem* TInfoBannerController::mapWindowPane()
{
    return m_mapWindowPane;
}

void TInfoBannerController::enqueue(const cip::tibia::qmlcomponents::enums::EInfoBannerType type, const QJsonValue& extra)
{
    if (!app::ClientOptions::load().options().value(QLatin1String("gameWindowShowInfoBanner")).toBool(true))
        return;
    Q_EMIT showInfoBanner(type, extra);
    Q_EMIT displayDataChanged();
}

void TInfoBannerController::clearQueue()
{
    Q_EMIT clearInfoBannerQueue();
}

TLenshelpController* TLenshelpController::s_instance = nullptr;

TLenshelpController::TLenshelpController(QObject* parent)
    : cip::tibia::gamewindow::TLenshelpController(parent)
{
    s_instance = this;
}

TLenshelpController* TLenshelpController::instance()
{
    return s_instance;
}

void TLenshelpController::showHelp(const QString& caption, const QString& content)
{
    m_caption = caption;
    m_content = content;
    Q_EMIT contentChanged();
    if (!m_lenshelpVisible) {
        m_lenshelpVisible = true;
        Q_EMIT lenshelpVisibleChanged();
    }
}

void TLenshelpController::hideHelp()
{
    if (!m_lenshelpVisible)
        return;
    m_lenshelpVisible = false;
    Q_EMIT lenshelpVisibleChanged();
}

TLinkCopyWarningDialogController::TLinkCopyWarningDialogController(QObject* parent)
    : cip::tibia::gamewindow::TLinkCopyWarningDialogController(parent)
{}

void TLinkCopyWarningDialogController::onButtonClicked(
    const cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier ButtonIdentifier)
{
    using B = cip::tibia::gamewindow::TMessageDialogButton::EMessageDialogButtonIdentifier;
    if (ButtonIdentifier == B::YES || ButtonIdentifier == B::OK)
        onYesClicked();
    else
        onNoClicked();
}

void TLinkCopyWarningDialogController::onYesClicked()
{
    Q_EMIT copyConfirmed();
}

void TLinkCopyWarningDialogController::onNoClicked()
{
    Q_EMIT copyRejected();
}

TLoginConfirmationCodeEnterTextDialogController::TLoginConfirmationCodeEnterTextDialogController(QObject* parent)
    : TDialogController(QStringLiteral("LoginConfirmationCodeEnterDialog.qml"), parent)
{}

void TLoginConfirmationCodeEnterTextDialogController::onCancelClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TLoginConfirmationCodeEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    m_enteredText = EnteredText;
    Q_EMIT codeEntered(EnteredText);
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TLoginConfirmationCodeEnterTextDialogController::onSendClicked()
{
    // CIP LoginConfirmationCodeEnterDialog.qml requestConfirmationCode → resend path.
    setHasPendingCode(true);
    Q_EMIT confirmationCodeResendRequested();
}

void TLoginConfirmationCodeEnterTextDialogController::getEnteredText()
{
    // CIP void getter slot (moc); QML reads the `enteredText` property directly.
}

void TLoginConfirmationCodeEnterTextDialogController::setHasPendingConfirmationCode(const bool hasCode)
{
    setHasPendingCode(hasCode);
}

TMainCharacterChangeDialogController::TMainCharacterChangeDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MainCharacterChangeDialog.qml"), parent)
{}

void TMainCharacterChangeDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TMainCharacterChangeDialogController::requestClose()
{
    closeDialog();
}

void TMainCharacterChangeDialogController::requestMainCharChange(const QString& ChosenCharacter)
{
    Q_EMIT mainCharacterChangeRequested(ChosenCharacter);
    closeDialog();
}

TMarketCoinTransactionDetailsDialogController::TMarketCoinTransactionDetailsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MarketCoinTransactionDetails.qml"), parent)
{}

void TMarketCoinTransactionDetailsDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TMarketCoinTransactionDetailsDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TMarketCoinTransactionDetailsDialogController::requestClose()
{
    closeDialog();
}

TMultiOfflineTrainingDialogController* TMultiOfflineTrainingDialogController::s_instance = nullptr;

TMultiOfflineTrainingDialogController::TMultiOfflineTrainingDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MultiOfflineTrainingDialog.qml"), parent)
{
    s_instance = this;
}

TMultiOfflineTrainingDialogController* TMultiOfflineTrainingDialogController::instance()
{
    return s_instance;
}

void TMultiOfflineTrainingDialogController::showSkillsDialog()
{
    dialog()->create();
    dialog()->show();
}

void TMultiOfflineTrainingDialogController::requestClose()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TMultiOfflineTrainingDialogController::startOfflineTraining(
    const cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType Skill)
{
    g_game.sendStartOfflineTraining(static_cast<uint8_t>(Skill));
    requestClose();
}

TOutfitMemorialDialogController* TOutfitMemorialDialogController::s_instance = nullptr;

TOutfitMemorialDialogController::TOutfitMemorialDialogController(QObject* parent)
    : TDialogController(QStringLiteral("OutfitMemorialDialog.qml"), parent)
{
    s_instance = this;
    m_hooks.emplace_back("g_game", "onOutfitMemorial", [this](LuaObject*, const Args& args) {
        showMemorial(argString(args, 0));
        return std::any{};
    });
}

TOutfitMemorialDialogController* TOutfitMemorialDialogController::instance()
{
    return s_instance;
}

void TOutfitMemorialDialogController::showMemorial(const QString& title)
{
    // Soft body: CIP lists/costs until GameserverMessageOutfitMemorial is fully parsed.
    // Surface the CipSoft title as the golden base list so Soft QML is not empty.
    m_baseGoldenOutfitOwners.clear();
    if (!title.isEmpty())
        m_baseGoldenOutfitOwners.append(title);
    dialog()->create();
    dialog()->show();
}

void TOutfitMemorialDialogController::requestClose()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TReturnerRewardDialogController::TReturnerRewardDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ReturnerRewardNotificationDialog.qml"), parent)
{}

void TReturnerRewardDialogController::showReward(const QString& text)
{
    m_dialogText = text;
    dialog()->create();
    dialog()->show();
}

void TReturnerRewardDialogController::onContinueClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TShowImageDialogController* TShowImageDialogController::s_instance = nullptr;

TShowImageDialogController::TShowImageDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ShowImageDialog.qml"), parent)
{
    s_instance = this;
}

TShowImageDialogController* TShowImageDialogController::instance()
{
    return s_instance;
}

void TShowImageDialogController::showImages(const QStringList& paths, const QString& caption, const bool dragonHeader,
                                            const bool crestHeader)
{
    m_paths = paths;
    m_caption = caption;
    m_showCaption = !caption.isEmpty();
    m_showDragonHeader = dragonHeader;
    m_showCrestHeader = crestHeader;
    m_imageCount = static_cast<uint>(paths.size());
    m_currentIndex = 0;
    m_imageTabIcons = paths;
    refreshCurrent();
    dialog()->create();
    dialog()->show();
}

void TShowImageDialogController::refreshCurrent()
{
    m_currentImagePath = m_paths.value(static_cast<int>(m_currentIndex));
    Q_EMIT dataChanged();
}

void TShowImageDialogController::requestClose()
{
    closeDialog();
}

void TShowImageDialogController::nextClicked()
{
    if (m_paths.isEmpty())
        return;
    m_currentIndex = (m_currentIndex + 1) % static_cast<uint>(m_paths.size());
    refreshCurrent();
}

void TShowImageDialogController::backClicked()
{
    if (m_paths.isEmpty())
        return;
    m_currentIndex = (m_currentIndex + static_cast<uint>(m_paths.size()) - 1) % static_cast<uint>(m_paths.size());
    refreshCurrent();
}

void TShowImageDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TShowSpellBookDialogController::TShowSpellBookDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ShowSpellBookDialog.qml"), parent)
{}

void TShowSpellBookDialogController::showSpellBook(const int spellBookId, const QString& text)
{
    m_spellBookID = spellBookId;
    m_spellBookText = text;
    dialog()->create();
    dialog()->show();
}

void TShowSpellBookDialogController::openCyclopedia()
{
    input::publish(input::makeAction(A::OPEN_CYCLOPEDIA));
}

void TShowSpellBookDialogController::requestClose()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TSoulSealsDialogController* TSoulSealsDialogController::s_instance = nullptr;

TSoulSealsDialogController::TSoulSealsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("taskboard/SoulSealsDialog.qml"), parent)
    , m_allMonsters(new TSoulSealsMonsterListModel(this))
    , m_visibleMonsters(new TSoulSealsMonsterListModel(this))
{
    s_instance = this;
    m_monsterList = m_visibleMonsters;
    m_availableMonsterDifficulties = QStringList{ QStringLiteral("All"), QStringLiteral("Easy"), QStringLiteral("Medium"),
                                                  QStringLiteral("Hard"), QStringLiteral("Expert") };
    m_indexOfSelectedDifficulty = 0;

    m_hooks.emplace_back("g_game", "onSoulsealsData", [this](LuaObject*, const Args& args) {
        std::vector<TSoulSealsMonsterListModel::Entry> entries;
        if (const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<TaskBoardSoulsealEntryData>>(&args[0])) {
            entries.reserve(list->size());
            for (const TaskBoardSoulsealEntryData& row : *list) {
                TSoulSealsMonsterListModel::Entry entry;
                entry.raceId = static_cast<int>(row.raceId);
                entry.name = QString::fromStdString(row.name);
                if (entry.name.isEmpty()) {
                    const RaceType& race = g_things.getRaceData(row.raceId);
                    entry.name = QString::fromStdString(race.name);
                    if (!entry.name.isEmpty())
                        entry.name[0] = entry.name[0].toUpper();
                }
                entry.animusMasteryCompleted = row.done != 0;
                entry.soulPitFightPrice = static_cast<int>(row.soulsealPoints);
                entry.category = static_cast<int>(row.category);
                entries.push_back(std::move(entry));
            }
        }
        applyEntries(std::move(entries));
        dialog()->create();
        dialog()->show();
        return std::any{};
    });
}

TSoulSealsDialogController* TSoulSealsDialogController::instance()
{
    return s_instance;
}

void TSoulSealsDialogController::applyEntries(std::vector<TSoulSealsMonsterListModel::Entry> entries)
{
    m_allMonsters->setEntries(std::move(entries));
    rebuildVisible();
}

void TSoulSealsDialogController::rebuildVisible()
{
    std::vector<TSoulSealsMonsterListModel::Entry> visible;
    // index 0 = All; CipSoft categories are 0-based Easy.. matching packet category.
    const int difficultyFilter = m_indexOfSelectedDifficulty <= 0 ? -1 : m_indexOfSelectedDifficulty - 1;
    const QString filter = m_monstersNameFilter.trimmed();
    for (int i = 0; i < m_allMonsters->rowCount(); ++i) {
        const int raceId = m_allMonsters->getRaceIDAtIndex(i);
        const TSoulSealsMonsterListModel::Entry* entry = m_allMonsters->entryForRace(raceId);
        if (!entry)
            continue;
        if (difficultyFilter >= 0 && entry->category != difficultyFilter)
            continue;
        if (!filter.isEmpty() && !entry->name.contains(filter, Qt::CaseInsensitive))
            continue;
        visible.push_back(*entry);
    }
    m_visibleMonsters->setEntries(std::move(visible));
    Q_EMIT monstersListChanged();
    refreshSelection();
}

void TSoulSealsDialogController::refreshSelection()
{
    const TSoulSealsMonsterListModel::Entry* entry = m_visibleMonsters->entryForRace(m_selectedMonsterRaceID);
    if (!entry) {
        m_selectedMonsterRaceID = 0;
        m_selectedMonstername.clear();
        m_selectedFightPrice = 0;
    } else {
        m_selectedMonstername = entry->name;
        m_selectedFightPrice = entry->soulPitFightPrice;
    }
    Q_EMIT selectedMonsterRaceIDChanged();
}

void TSoulSealsDialogController::setSelectedMonsterRaceID(const int v)
{
    if (m_selectedMonsterRaceID == v)
        return;
    m_selectedMonsterRaceID = v;
    refreshSelection();
}

void TSoulSealsDialogController::setIndexOfSelectedDifficulty(const int v)
{
    if (m_indexOfSelectedDifficulty == v)
        return;
    m_indexOfSelectedDifficulty = v;
    Q_EMIT selectedDifficultyChanged();
    rebuildVisible();
}

void TSoulSealsDialogController::setMonstersNameFilter(const QString& v)
{
    if (m_monstersNameFilter == v)
        return;
    m_monstersNameFilter = v;
    Q_EMIT monstersNameFilterChanged();
    rebuildVisible();
}

void TSoulSealsDialogController::closeDialog()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TSoulSealsDialogController::requestClose()
{
    closeDialog();
}

void TSoulSealsDialogController::fightselectedMonster()
{
    if (m_selectedMonsterRaceID > 0)
        g_game.soulsealFightAction(static_cast<uint16_t>(m_selectedMonsterRaceID));
    closeDialog();
}

TSurveyInvitationDialogController::TSurveyInvitationDialogController(QObject* parent)
    : TDialogController(QStringLiteral("SurveyInvitatioDialog.qml"), parent)
{}

void TSurveyInvitationDialogController::showInvitation(const QString& text, const QString& date, const bool askLater)
{
    m_invitationText = text;
    m_dateString = date;
    m_showAskLater = askLater;
    Q_EMIT contentChanged();
    dialog()->create();
    dialog()->show();
}

void TSurveyInvitationDialogController::onYesClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TSurveyInvitationDialogController::onNoClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TSurveyInvitationDialogController::onAskLaterClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::
    TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController(QObject* parent)
    : TDialogController(QStringLiteral("taskboard/HuntingTaskRewardPurchaseConfirmationDialog.qml"), parent)
{}

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::prepare(const int appearanceId, const int optionalId,
                                                                              const QString& title, const QString& cost,
                                                                              const QVariant& rewardType)
{
    m_appearanceId = appearanceId;
    m_optionalId = optionalId;
    m_title = title;
    m_cost = cost;
    m_rewardType = rewardType;
}

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::purchaseRequested()
{
    Q_EMIT purchaseAccepted(m_optionalId);
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TTaskboardHuntingTaskRewardPurchaseConfirmationDialogController::closeRequested()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

TTutorialOverlayController::TTutorialOverlayController(QObject* parent)
    : TDialogController(QStringLiteral("tutorial/TutorialOverlayTemplate.qml"), parent)
{}

void TTutorialOverlayController::onMouseFilterAreaEntered()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->onTutorialOverlayEntered();
}

void TTutorialOverlayController::onMouseFilterAreaExited()
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->onTutorialOverlayExited();
}

TTutorialOverlayItemController::TTutorialOverlayItemController(QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayItemController(parent)
{}

TTutorialOverlayMouseFilterController::TTutorialOverlayMouseFilterController(QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayMouseFilterController(parent)
{}

TTutorialOverlayMouseTrailAnimationController::TTutorialOverlayMouseTrailAnimationController(QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayMouseTrailAnimationController(parent)
{}

TTwoFactorCodeEnterTextDialogController::TTwoFactorCodeEnterTextDialogController(QObject* parent)
    : TDialogController(QStringLiteral("TwoFactorTextEnterDialog.qml"), parent)
{
    m_maxInputLength = 6;
}

void TTwoFactorCodeEnterTextDialogController::onCancelClicked()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TTwoFactorCodeEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    m_enteredText = EnteredText;
    Q_EMIT codeEntered(EnteredText, m_trustedDevice);
    dialog()->close();
    Q_EMIT dialogClosed();
}

void TTwoFactorCodeEnterTextDialogController::onButtonClicked()
{
    // CIP TwoFactorTextEnterDialog.qml resend-email / secondary button.
    setEmailResendPossible(false);
    Q_EMIT emailResendRequested();
}

void TTwoFactorCodeEnterTextDialogController::onControllerFinished()
{
    dialog()->close();
    Q_EMIT dialogClosed();
}

bool TTwoFactorCodeEnterTextDialogController::getIsTrustedDevice()
{
    return m_trustedDevice;
}

void TTwoFactorCodeEnterTextDialogController::setIsTrustedDevice(const bool TrustedDevice)
{
    setTrustedDevice(TrustedDevice);
}

TTwoFactorCodeEnterTextDialogController::ETwoFactorMethod TTwoFactorCodeEnterTextDialogController::getTwoFactorMethod()
{
    return m_twoFactorMethod;
}

QString TTwoFactorCodeEnterTextDialogController::getAdditionalInfoText()
{
    return m_additionalInfoText;
}

uint TTwoFactorCodeEnterTextDialogController::getMaxInputLength()
{
    return m_maxInputLength;
}

} // namespace tibia::gamewindow
