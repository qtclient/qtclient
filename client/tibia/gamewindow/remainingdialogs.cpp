#include "remainingdialogs.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <set>

#include "app/clientoptions.h"
#include "game/gameciptypes.h"
#include "game/offlineskilltrainingdata.h"
#include "game/tgamenotify.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>

#include <QColor>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using Args = std::vector<std::any>;
using client::gamecore::argString;
constexpr auto AskBeforeStowOption = "askBeforeStowContainerContent";

QColor outfitColor(const int index)
{
    const QColor c = Outfit::getColor(index);
    return QColor(c.red(), c.green(), c.blue());
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
    , m_conditions(new TCharacterTradeNotificationsAbstractListModel(this))
    , m_highlights(new TTextualHighlightsSortProxyModel(this))
    , m_items(new TItemHighlightsSortProxyModel(this))
    , m_confirmations(new TCharacterTradeNotificationsAbstractListModel(this))
    , m_overviewHighlights(new TTextualHighlightsAbstractListModel(this))
    , m_overviewItems(new TItemHighlightsAbstractListModel(this))
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

QObject* TCharacterTradeDialogController::characterData() const
{
    return m_characterData;
}

QObject* TCharacterTradeDialogController::auctionConfiguration() const
{
    return m_auctionConfiguration;
}

QObject* TCharacterTradeDialogController::conditionsModel() const
{
    return m_conditionsModel;
}

QObject* TCharacterTradeDialogController::highlightsModel() const
{
    return m_highlightsModel;
}

QObject* TCharacterTradeDialogController::itemsModel() const
{
    return m_itemsModel;
}

QObject* TCharacterTradeDialogController::confirmationsModel() const
{
    return m_confirmationsModel;
}

bool TCharacterTradeDialogController::hasUnfulfilledConditions() const
{
    return m_hasUnfulfilledConditions;
}

bool TCharacterTradeDialogController::hasConfirmationError() const
{
    return m_hasConfirmationError;
}

QObject* TCharacterTradeDialogController::characterOverviewHighlightsModel() const
{
    return m_characterOverviewHighlightsModel;
}

QObject* TCharacterTradeDialogController::characterOverviewItemsModel() const
{
    return m_characterOverviewItemsModel;
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
    dialog()->close();
}

void TCharacterTradeDialogController::requestClose()
{
    dialog()->close();
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

TCharacterTradeDialogController::TConfiguration TCharacterTradeDialogController::configuration() const
{
    TConfiguration configuration;
    configuration.minimumBid = m_auction ? static_cast<uint32_t>(std::max(0, m_auction->minimumBid())) : 0;
    configuration.auctionEnd = m_auction ? static_cast<uint32_t>(std::max(0, m_auction->auctionEndTimestamp())) : 0;
    // Highlight ids past 0xffff are store items (store item id + 0xffff).
    for (const int id : m_selectedHighlightIds) {
        const auto value = static_cast<uint32_t>(id);
        if (value < 0x10000)
            configuration.highlights.push_back(static_cast<uint16_t>(value));
        else
            configuration.storeItems.emplace_back(static_cast<uint16_t>((value + 1) & 0xffff), 0);
    }
    for (const QPair<int, int>& item : m_selectedItems)
        configuration.items.emplace_back(static_cast<uint16_t>(item.first), static_cast<uint8_t>(item.second));
    return configuration;
}

void TCharacterTradeDialogController::requestConditions()
{
    tibia::game::requestCharacterTradeConfiguration(0);
    m_hasUnfulfilledConditions = true;
    Q_EMIT hasUnfulfilledConditionsChanged();
}

void TCharacterTradeDialogController::requestCheckConfiguration()
{
    const TConfiguration config = configuration();
    tibia::game::confirmCharacterTradeConfiguration(config.minimumBid, config.auctionEnd, config.items, config.storeItems,
                                              config.highlights, false);
    m_hasConfirmationError = true;
    Q_EMIT hasConfirmationErrorChanged();
}

void TCharacterTradeDialogController::requestConfirmConfiguration()
{
    const TConfiguration config = configuration();
    tibia::game::confirmCharacterTradeConfiguration(config.minimumBid, config.auctionEnd, config.items, config.storeItems,
                                              config.highlights, true);
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
            m_items->source()->setEntries(storage->items());
            break;
        case trade::TCharacterTradeStorage::Highlights:
            m_highlights->source()->setEntries(storage->highlights());
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
    for (const trade::TCharacterTradeHighlightEntry& entry : m_highlights->source()->entries()) {
        if (m_selectedHighlightIds.contains(entry.highlightId))
            highlights.push_back(entry);
    }
    m_overviewHighlights->setEntries(std::move(highlights));
    if (m_auction)
        m_auction->setSelectedHighlightCount(m_selectedHighlightIds.size());

    std::vector<trade::TCharacterTradeItemEntry> items;
    for (const trade::TCharacterTradeItemEntry& entry : m_items->source()->entries()) {
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
    // The benchmark window closing ends the cinematic player.
    Q_EMIT cinematicSessiondumpPlayerClosed();
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

QString TCrashReportDialogController::minimalInformation() const
{
    const std::set<QString> shownKeys = { QStringLiteral("LogMessages"), QStringLiteral("Character") };
    QStringList lines;
    for (const auto& [key, value] : m_crashReportData->values()) {
        if (shownKeys.count(key))
            lines.append(QStringLiteral("%1: %2").arg(key, value));
    }
    return lines.join(QLatin1Char('\n'));
}

QString TCrashReportDialogController::characterName() const
{
    const auto& values = m_crashReportData->values();
    const auto it = values.find(QStringLiteral("Character"));
    if (it == values.end())
        return QString();
    return values.at(QStringLiteral("Character"));
}

QString TCrashReportDialogController::logMessages() const
{
    QStringList lines;
    const QJsonObject annotations = m_crashReportData->toJsonDocument().object();
    const auto logMessages = annotations.constFind(QStringLiteral("LogMessages"));
    if (logMessages == annotations.constEnd()) {
        lines.append(qtTrId("crashreport_dialog_no_log_messages"));
    } else {
        const QString json = QStringLiteral("{\"%1\": %2}").arg(QStringLiteral("LogMessages"), logMessages->toString(QString()));
        const QJsonArray messages =
            QJsonDocument::fromJson(json.toUtf8()).object()[QStringLiteral("LogMessages")].toArray();
        for (const QJsonValue message : messages) {
            const QString time = QDateTime::fromSecsSinceEpoch(message.toObject()[QStringLiteral("Timestamp")].toInt(0))
                                     .toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
            lines.append(QStringLiteral("%1: %2").arg(time, message.toObject()[QStringLiteral("Message")].toString(QString())));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void TCrashReportDialogController::showDialog(std::shared_ptr<tibia::shared::TCrashReportData> CrashReportData)
{
    m_crashReportData = std::move(CrashReportData);
    dialog()->create();
    dialog()->show();
}

void TCrashReportDialogController::sendReport(const QString& PlayerComment, const bool FullReport)
{
    (*m_crashReportData)[QStringLiteral("Comment")] = PlayerComment;
    QString reportType = QStringLiteral("Full");
    if (!FullReport) {
        reportType = QStringLiteral("Minimal");
        m_crashReportData->erase(QStringLiteral("LogMessages"));
        m_crashReportData->anonymize();
    }
    (*m_crashReportData)[QStringLiteral("ReportType")] = reportType;
    dialog()->close();
    Q_EMIT requestSendCrashReport(FullReport);
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

/// CIP: the code is dropped, the information line cleared and the dialog aborted.
void TLoginConfirmationCodeEnterTextDialogController::onCancelClicked()
{
    m_enteredText.clear();
    setAdditionalInfoText(QString());
    Q_EMIT abort();
    dialog()->close();
    Q_EMIT dialogClosed();
}

/// CIP: the dialog closes; an empty code cancels, otherwise it is kept and
/// textEntered goes out.
void TLoginConfirmationCodeEnterTextDialogController::onOkClicked(const QString& EnteredText)
{
    dialog()->close();
    if (EnteredText.isEmpty()) {
        onCancelClicked();
        return;
    }
    m_enteredText = EnteredText;
    setAdditionalInfoText(QString());
    Q_EMIT textEntered(EnteredText);
}

QVariantMap TLoginConfirmationCodeEnterTextDialogController::getAvailableMethods()
{
    return { { QStringLiteral("primaryEmail"), m_targets.primaryEmail },
             { QStringLiteral("secondaryEmail"), m_targets.secondaryEmail },
             { QStringLiteral("sms"), m_targets.sms } };
}

void TLoginConfirmationCodeEnterTextDialogController::setAvailableMethods(const ConfirmationMethodTargets& Targets)
{
    if (m_targets == Targets)
        return;
    m_targets = Targets;
    Q_EMIT availableMethodsChanged();
}

QString TLoginConfirmationCodeEnterTextDialogController::getChosenMethod()
{
    switch (m_method) {
        case EmailPrimary: return QStringLiteral("emailPrimary");
        case EmailSecondary: return QStringLiteral("emailSecondary");
        case Sms: return QStringLiteral("sms");
    }
    return QStringLiteral("emailPrimary");
}

/// CIP: "emailPrimary", "emailSecondary", anything else is SMS.
void TLoginConfirmationCodeEnterTextDialogController::setChosenMethod(const QString& Value)
{
    const EMethod method = Value == QLatin1String("emailPrimary") ? EmailPrimary
        : Value == QLatin1String("emailSecondary")                ? EmailSecondary
                                                                  : Sms;
    if (method == m_method)
        return;
    m_method = method;
    Q_EMIT chosenMethodChanged();
}

/// CIP: asks for a code by the chosen method; the information line clears.
void TLoginConfirmationCodeEnterTextDialogController::onSendClicked()
{
    setAdditionalInfoText(QString());
    Q_EMIT confirmationCodeRequested(m_method);
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

namespace {

using OfflineTrainingSkill = cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType;

/// CIP MultiOfflineTrainingDialog skill list order (Sword..Fist, Magic).
constexpr OfflineTrainingSkill kOfflineTrainingSkills[] = {
    OfflineTrainingSkill::OfflineTrainingSkillTypeSword,
    OfflineTrainingSkill::OfflineTrainingSkillTypeAxe,
    OfflineTrainingSkill::OfflineTrainingSkillTypeClub,
    OfflineTrainingSkill::OfflineTrainingSkillTypeDistance,
    OfflineTrainingSkill::OfflineTrainingSkillTypeMagic,
    OfflineTrainingSkill::OfflineTrainingSkillTypeFist,
};

}

TMultiOfflineTrainingDialogController::TMultiOfflineTrainingDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MultiOfflineTrainingDialog.qml"), parent)
{
    s_instance = this;
    rebuildSkillsModel();
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMultiOfflineTrainingDialog", [this](GameObject*, const Args&) {
        showSkillsDialog();
        return std::any{};
    });
}

TMultiOfflineTrainingDialogController* TMultiOfflineTrainingDialogController::instance()
{
    return s_instance;
}

QList<QObject*> TMultiOfflineTrainingDialogController::skillsModel() const { return m_skillsModel; }

void TMultiOfflineTrainingDialogController::rebuildSkillsModel()
{
    qDeleteAll(m_skillsModel);
    m_skillsModel.clear();
    for (const OfflineTrainingSkill skill : kOfflineTrainingSkills) {
        auto* data = new game::TOfflineSkillTrainingData(skill, this);
        data->assign(game::buildOfflineTrainingEntry(skill));
        m_skillsModel.append(data);
    }
    Q_EMIT skillsModelChanged();
}

void TMultiOfflineTrainingDialogController::showSkillsDialog()
{
    rebuildSkillsModel();
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
    tibia::game::sendStartOfflineTraining(static_cast<uint8_t>(Skill));
    requestClose();
}

TOutfitMemorialDialogController* TOutfitMemorialDialogController::s_instance = nullptr;

TOutfitMemorialDialogController::TOutfitMemorialDialogController(QObject* parent)
    : TDialogController(QStringLiteral("OutfitMemorialDialog.qml"), parent)
{
    s_instance = this;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOutfitMemorial", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<OutfitMemorialData>(&args[0]))
            showMemorial(*data);
        return std::any{};
    });
}

TOutfitMemorialDialogController* TOutfitMemorialDialogController::instance()
{
    return s_instance;
}

void TOutfitMemorialDialogController::showMemorial(const OutfitMemorialData& data)
{
    const auto toList = [](const std::vector<std::string>& names) {
        QStringList list;
        list.reserve(int(names.size()));
        for (const std::string& name : names)
            list.append(QString::fromStdString(name));
        return list;
    };

    m_baseGoldenOutfitCost = int(data.baseGoldenOutfitCost);
    m_oneAddonGoldenOutfitCost = int(data.oneAddonGoldenOutfitCost);
    m_fullGoldenOutfitCost = int(data.fullGoldenOutfitCost);
    m_baseGoldenOutfitOwners = toList(data.baseGoldenOutfitOwners);
    m_oneAddonGoldenOutfitOwners = toList(data.oneAddonGoldenOutfitOwners);
    m_fullGoldenOutfitOwners = toList(data.fullGoldenOutfitOwners);

    m_baseTokenOutfitSilverToken = data.baseTokenOutfitSilverToken;
    m_baseTokenOutfitGoldToken = data.baseTokenOutfitGoldToken;
    m_oneAddonTokenOutfitSilverToken = data.oneAddonTokenOutfitSilverToken;
    m_oneAddonTokenOutfitGoldToken = data.oneAddonTokenOutfitGoldToken;
    m_fullTokenOutfitSilverToken = data.fullTokenOutfitSilverToken;
    m_fullTokenOutfitGoldToken = data.fullTokenOutfitGoldToken;
    m_baseTokenOutfitOwners = toList(data.baseTokenOutfitOwners);
    m_oneAddonTokenOutfitOwners = toList(data.oneAddonTokenOutfitOwners);
    m_fullTokenOutfitOwners = toList(data.fullTokenOutfitOwners);

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
    TData data;
    data.showDragonHeader = dragonHeader;
    data.showCrestHeader = crestHeader;
    data.showCaption = !caption.isEmpty();
    data.caption = caption;
    for (const QString& path : paths)
        data.images.push_back({ path, path });
    setData(data);
    if (!dialog()->item()) {
        dialog()->create();
        dialog()->show();
    }
}

/// CIP: new data replaces the old (the tab icons follow the images); the
/// current index stays.
void TShowImageDialogController::setData(const TData& data)
{
    if (m_data == data)
        return;
    m_data = data;
    m_showDragonHeader = data.showDragonHeader;
    m_showCrestHeader = data.showCrestHeader;
    m_showCaption = data.showCaption;
    m_caption = data.caption;
    m_bannerPath = data.bannerPath;
    m_imageTabIcons.clear();
    for (const TImage& image : data.images)
        m_imageTabIcons.append(image.tabIcon);
    Q_EMIT dataChanged();
}

/// CIP: the index never passes the image count.
void TShowImageDialogController::setCurrentIndex(const uint index)
{
    const uint clamped = std::min(index, imageCount());
    if (clamped == m_currentIndex)
        return;
    m_currentIndex = clamped;
    Q_EMIT dataChanged();
}

QString TShowImageDialogController::currentImagePath() const
{
    return m_currentIndex < imageCount() ? m_data.images[m_currentIndex].path : QString();
}

void TShowImageDialogController::requestClose()
{
    closeDialog();
}

void TShowImageDialogController::nextClicked()
{
    setCurrentIndex(m_currentIndex + 1);
}

void TShowImageDialogController::backClicked()
{
    if (m_currentIndex != 0)
        setCurrentIndex(m_currentIndex - 1);
}

/// CIP: closing drops the data.
void TShowImageDialogController::closeDialog()
{
    dialog()->close();
    setData({});
    Q_EMIT dialogClosed();
}

TShowSpellBookDialogController::TShowSpellBookDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ShowSpellBookDialog.qml"), parent)
{
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TShowSpellBookDialogController::onGameSessionChanged);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameSessionChanged, this,
            &TShowSpellBookDialogController::onGameSessionChanged);
    onGameSessionChanged();
}

void TShowSpellBookDialogController::onGameSessionChanged()
{
    dialog()->close();
    if (auto* handler = tibia::game::TSpellBookProtocolMessageHandler::liveInstance())
        connect(handler, &tibia::game::TSpellBookProtocolMessageHandler::requestShowSpellBookDialog, this,
                &TShowSpellBookDialogController::showSpellBookDialog, Qt::UniqueConnection);
}

void TShowSpellBookDialogController::showSpellBookDialog(const QString& Text, const int AppearanceID)
{
    m_spellBookText = Text;
    m_spellBookID = static_cast<uint16_t>(AppearanceID);
    // An open dialog keeps showing the book it was opened with.
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

void TShowSpellBookDialogController::openCyclopedia()
{
    dialog()->close();
    // The magical archive tab of the cyclopedia.
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 8;
    input::publish(action);
}

void TShowSpellBookDialogController::requestClose() { dialog()->close(); }

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

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSoulsealsData", [this](GameObject*, const Args& args) {
        std::vector<TSoulSealsMonsterListModel::Entry> entries;
        if (const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<TaskBoardSoulsealEntryData>>(&args[0])) {
            entries.reserve(list->size());
            for (const TaskBoardSoulsealEntryData& row : *list) {
                TSoulSealsMonsterListModel::Entry entry;
                entry.raceId = static_cast<int>(row.raceId);
                entry.name = QString::fromStdString(row.name);
                if (entry.name.isEmpty()) {
                    const RaceType& race = tibia::game::raceData(row.raceId);
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
        tibia::game::soulsealFightAction(static_cast<uint16_t>(m_selectedMonsterRaceID));
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
