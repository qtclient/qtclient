#include "remainingdialogs.h"
#include "colorhelper.h"

#include "shared/simplevectorabstractlistmodel.h"

#include <QDateTime>
#include <QLocale>
#include <QFile>
#include <QJsonArray>
#include <QLocale>
#include <QJsonDocument>
#include <QJsonObject>

#include <set>

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "game/gameciptypes.h"
#include "game/offlineskilltrainingdata.h"
#include "game/tgamenotify.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "taskboard/taskboardciptypes.h"

#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/gamesession.h>

#include <QColor>

#include <algorithm>
#include <limits>
#include <tuple>
#include <unordered_set>
#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using Args = std::vector<std::any>;
using client::gamecore::argString;
constexpr auto AskBeforeStowOption = "askBeforeStowContainerContent";


QString itemNameOf(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return QString::number(typeId);
    const auto* market = type->marketProperties();
    return market && !market->name.isEmpty() ? market->name : type->name();
}

QString categoryIconFor(const int category)
{
    switch (category) {
        case 1: return QStringLiteral("/images/icon-yes.png");
        case 2: return QStringLiteral("/images/icon-exclamationmark.png");
        default: return QStringLiteral("/images/icon-no.png");
    }
}

std::vector<trade::TCharacterTradeItemEntry> itemEntries(const std::vector<trade::TCharacterTradeItem>& items)
{
    std::vector<trade::TCharacterTradeItemEntry> rows;
    rows.reserve(items.size());
    for (const trade::TCharacterTradeItem& item : items) {
        trade::TCharacterTradeItemEntry entry;
        entry.id = item.objectId;
        entry.upgradeTier = item.tier;
        entry.amount = static_cast<int>(item.count);
        entry.name = itemNameOf(item.objectId);
        rows.push_back(std::move(entry));
    }
    return rows;
}

QObject* tradeOutfitConfiguration(const Outfit& outfit, QObject* parent)
{
    return new TOutfitQMLInformation(static_cast<int>(outfit.getId()), outfit.getHead(), outfit.getBody(), outfit.getLegs(),
                                     outfit.getFeet(), outfit.getAddons(), parent);
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
}

void TCharacterAuctionConfiguration::addHighlight(const int highlightId)
{
    m_highlights.insert(highlightId);
    Q_EMIT selectedHighlightsChanged();
}

void TCharacterAuctionConfiguration::removeHighlight(const int highlightId)
{
    m_highlights.remove(highlightId);
    Q_EMIT selectedHighlightsChanged();
}

void TCharacterAuctionConfiguration::clearHighlights()
{
    m_highlights.clear();
    Q_EMIT selectedHighlightsChanged();
}

void TCharacterAuctionConfiguration::addItem(const int itemId, const int upgradeTier)
{
    m_items.insert({ itemId, upgradeTier });
    Q_EMIT selectedItemsChanged();
}

void TCharacterAuctionConfiguration::removeItem(const int itemId, const int upgradeTier)
{
    m_items.remove({ itemId, upgradeTier });
    Q_EMIT selectedItemsChanged();
}

void TCharacterAuctionConfiguration::clearItems()
{
    m_items.clear();
    Q_EMIT selectedItemsChanged();
}

/// The server's minimum bid and auction end window; a bid below the minimum
/// or an end outside the window moves to the bound.
void TCharacterAuctionConfiguration::setAuctionBounds(const int minimumBid, const int minEnd, const int maxEnd)
{
    m_minimumAuctionEndTimestamp = minEnd;
    m_maximumAuctionEndTimestamp = maxEnd;
    Q_EMIT configurationChanged();
    if (m_auctionEndTimestamp != maxEnd && (maxEnd < m_auctionEndTimestamp || m_auctionEndTimestamp < minEnd)) {
        m_auctionEndTimestamp = maxEnd;
        Q_EMIT configurationChanged();
    }
    m_minimumMinimumBid = minimumBid;
    Q_EMIT configurationChanged();
    if (m_minimumBid < minimumBid) {
        m_minimumBid = minimumBid;
        Q_EMIT configurationChanged();
    }
}

void TCharacterAuctionConfiguration::reset()
{
    m_minimumBid = 0;
    m_minimumMinimumBid = 0;
    m_auctionEndTimestamp = 0;
    m_minimumAuctionEndTimestamp = 0;
    m_maximumAuctionEndTimestamp = 0;
    clearHighlights();
    clearItems();
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

/// Opens on a fresh configuration: the conditions are asked for, the previous
/// setup is cancelled and nothing is selected until the answers arrive.
void TCharacterTradeDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
    tibia::game::requestCharacterTradeConfiguration(0);
    tibia::game::requestCharacterTradeConfiguration(1);
    m_hasUnfulfilledConditions = true;
    m_hasConfirmationError = true;
    m_auction->reset();
    refreshOverviewModels();
    Q_EMIT hasUnfulfilledConditionsChanged();
    Q_EMIT hasConfirmationErrorChanged();
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
    m_auction->addHighlight(HighlightID);
    refreshOverviewModels();
}

void TCharacterTradeDialogController::removeHighlight(const int HighlightID)
{
    m_auction->removeHighlight(HighlightID);
    refreshOverviewModels();
}

void TCharacterTradeDialogController::addItem(const int ItemID, const int UpgradeTier)
{
    m_auction->addItem(ItemID, UpgradeTier);
    refreshOverviewModels();
}

void TCharacterTradeDialogController::removeItem(const int ItemID, const int UpgradeTier)
{
    m_auction->removeItem(ItemID, UpgradeTier);
    refreshOverviewModels();
}

TCharacterTradeDialogController::TConfiguration TCharacterTradeDialogController::configuration() const
{
    TConfiguration configuration;
    configuration.minimumBid = static_cast<uint32_t>(std::max(0, m_auction->minimumBid()));
    configuration.auctionEnd = static_cast<uint32_t>(std::max(0, m_auction->auctionEndTimestamp()));
    // Highlight ids past 0xffff are store items (store item id + 0xffff).
    for (const int id : m_auction->highlights()) {
        const auto value = static_cast<uint32_t>(id);
        if (value < 0x10000)
            configuration.highlights.push_back(static_cast<uint16_t>(value));
        else
            configuration.storeItems.emplace_back(static_cast<uint16_t>((value + 1) & 0xffff), 0);
    }
    for (const QPair<int, int>& item : m_auction->items())
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
            const trade::TCharacterTradeBasicData& data = storage->basicData();
            auto* character = static_cast<TQMLCharacterData*>(m_characterData);
            character->setCharacter(data.name, data.vocation, data.level, data.worldType, tradeOutfitConfiguration(data.outfit, character));
            std::vector<trade::TCharacterTradeConditionEntry> conditions;
            m_hasUnfulfilledConditions = false;
            for (const trade::TCharacterTradeCondition& condition : data.conditions) {
                // TibiaEnums.EYesNoWarning: Yes=0 No=1.
                conditions.push_back({ condition.text, condition.fulfilled ? 0 : 1 });
                if (!condition.fulfilled)
                    m_hasUnfulfilledConditions = true;
            }
            m_conditions->setEntries(std::move(conditions));
            Q_EMIT hasUnfulfilledConditionsChanged();
            m_auction->setAuctionBounds(static_cast<int>(data.minimumBid), static_cast<int>(data.startTime), static_cast<int>(data.endTime));
            break;
        }
        case trade::TCharacterTradeStorage::Items: {
            std::vector<trade::TCharacterTradeItemEntry> items = itemEntries(storage->items());
            m_items->source()->setEntries(items);
            m_overviewItems->setEntries({});
            m_auction->clearItems();
            break;
        }
        case trade::TCharacterTradeStorage::StoreItems:
        case trade::TCharacterTradeStorage::Highlights: {
            // The store items are highlights too, their id past the highlight ids.
            std::vector<trade::TCharacterTradeHighlightEntry> highlights;
            for (const trade::TCharacterTradeHighlightGroup& group : storage->highlights()) {
                for (const trade::TCharacterTradeHighlightGroup::Entry& highlight : group.entries)
                    highlights.push_back({ highlight.id, highlight.text, group.name, categoryIconFor(group.category) });
            }
            const QString storeCategory = qtTrId("charactertrade_store_items_category");
            for (const trade::TCharacterTradeItem& item : storage->storeItems()) {
                QString name = itemNameOf(item.objectId);
                if (name.isEmpty())
                    name = qtTrId("dummy_unknown");
                const QString text = qtTrId("charactertrade_store_items_template")
                                         .arg(QLocale(QLocale::English).toString(static_cast<qulonglong>(item.count)), name);
                highlights.push_back({ item.objectId + 0xffff, text, storeCategory, categoryIconFor(4) });
            }
            m_highlights->source()->setEntries(std::move(highlights));
            m_overviewHighlights->setEntries({});
            m_auction->clearHighlights();
            break;
        }
        case trade::TCharacterTradeStorage::Summary: {
            // A fee means the server accepted the setup: its problems, then
            // what the auction will be.
            if (storage->fee() == 0)
                break;
            std::vector<trade::TCharacterTradeConditionEntry> confirmations;
            m_hasConfirmationError = false;
            for (const QString& problem : storage->problems()) {
                confirmations.push_back({ problem, 1 });
                m_hasConfirmationError = true;
            }
            const auto* character = static_cast<TQMLCharacterData*>(m_characterData);
            confirmations.push_back(
                { qtTrId("charactertrade_confirmation_minimum_bid").arg(character->characterName()).arg(m_auction->minimumBid()), 0 });
            confirmations.push_back({ qtTrId("charactertrade_confirmation_auction_end")
                                          .arg(QDateTime::fromSecsSinceEpoch(m_auction->auctionEndTimestamp())
                                                   .toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"))),
                                      0 });
            confirmations.push_back({ qtTrId("charactertrade_confirmation_auction_fee").arg(storage->fee()), 0 });
            confirmations.push_back({ qtTrId("charactertrade_confirmation_insecure_coins"), 2 });
            confirmations.push_back({ qtTrId("charactertrade_confirmation_no_refund"), 2 });
            confirmations.push_back({ qtTrId("charactertrade_confirmation_privacy_warning"), 2 });
            m_confirmations->setEntries(std::move(confirmations));
            Q_EMIT hasConfirmationErrorChanged();
            break;
        }
    }
}

void TCharacterTradeDialogController::refreshOverviewModels()
{
    std::vector<trade::TCharacterTradeHighlightEntry> highlights;
    for (const trade::TCharacterTradeHighlightEntry& entry : m_highlights->source()->entries()) {
        if (m_auction->highlights().contains(entry.highlightId))
            highlights.push_back(entry);
    }
    m_overviewHighlights->setEntries(std::move(highlights));

    std::vector<trade::TCharacterTradeItemEntry> items;
    for (const trade::TCharacterTradeItemEntry& entry : m_items->source()->entries()) {
        if (m_auction->items().contains({ entry.id, entry.upgradeTier }))
            items.push_back(entry);
    }
    m_overviewItems->setEntries(std::move(items));
}

namespace {

/// The due payments of the character trade: the character, the amount and the due date.
class TDuePaymentModel final : public shared::TSimpleVectorAbstractListModel<authentication::TCharacterTradePayment, cip::TDuePaymentModel>
{
public:
    enum ERole { CharacterName = Qt::UserRole + 1, DueAmountString, DueDateString };

    using TSimpleVectorAbstractListModel::TSimpleVectorAbstractListModel;

    QHash<int, QByteArray> roleNames() const override
    {
        return { { CharacterName, "characterName" }, { DueAmountString, "dueAmountString" }, { DueDateString, "dueDateString" } };
    }

protected:
    QVariant itemData(const authentication::TCharacterTradePayment& item, const int role) const override
    {
        switch (role) {
        case CharacterName: return item.name;
        case DueAmountString: return QLocale(QLocale::English).toString(item.dueAmount);
        case DueDateString:
            return QDateTime::fromSecsSinceEpoch(item.dueTimestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
        default: return qtTrId("dummy_unknown");
        }
    }
};

}

TCharacterTradeDuePaymentDialogController::TCharacterTradeDuePaymentDialogController(QObject* parent)
    : TDialogController(QStringLiteral("charactertrade/CharacterTradeDuePaymentDialog.qml"), parent)
{
    m_duePayments = new TDuePaymentModel(this);
}

bool TCharacterTradeDuePaymentDialogController::showDuePayments(const std::vector<authentication::TCharacterTradePayment>& payments)
{
    static_cast<TDuePaymentModel*>(m_duePayments)->setItems(payments);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 remindAgain = config::TClientOptions::instance()->intOption(config::EClientOption::charTradePaymentRemindAgain);
    if (remindAgain >= now)
        return false;
    dialog()->create();
    dialog()->show();
    return true;
}

void TCharacterTradeDuePaymentDialogController::onClosePressed(const bool DoNotShowAgain)
{
    const int remindAgain = DoNotShowAgain ? static_cast<int>(QDateTime::currentSecsSinceEpoch()) + 86400 : 0;
    config::TClientOptions::instance()->setIntOption(config::EClientOption::charTradePaymentRemindAgain, remindAgain);
    dialog()->close();
    Q_EMIT dialogClosed();
}

TCharacterWorldTransferDialogController::TCharacterWorldTransferDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CharacterWorldTransferDialog.qml"), parent)
{}

bool TCharacterWorldTransferDialogController::canTransfer() const
{
    return std::none_of(std::begin(m_info.flags), std::end(m_info.flags), [](const bool flag) { return flag; });
}

void TCharacterWorldTransferDialogController::showDialog(const int OfferIDForWorldChange, const QString& CharacterName,
                                                         const tibia::store::TWorldTransferInformation& WorldTransferInfo)
{
    closeDialog();
    m_offerId = OfferIDForWorldChange;
    m_info = WorldTransferInfo;
    m_isExpressWorldTransfer = WorldTransferInfo.flag;
    m_characterName = CharacterName;
    m_canTransfer = canTransfer();
    createConditions();
    createWorlds();
    dialog()->show();
}

void TCharacterWorldTransferDialogController::createConditions()
{
    static constexpr std::pair<const char*, const char*> Texts[] = {
        { "characterworldtransfer_redskull", "characterworldtransfer_no_redskull" },
        { "characterworldtransfer_blackskull", "characterworldtransfer_no_blackskull" },
        { "characterworldtransfer_guild_leader", "characterworldtransfer_not_guild_leader" },
        { "characterworldtransfer_house_owner", "characterworldtransfer_not_house_owner" },
        { "characterworldtransfer_coin_offers", "characterworldtransfer_no_coin_offers" },
    };
    for (size_t i = 0; i < std::size(Texts); ++i) {
        const bool affected = m_info.flags[i];
        m_conditions.append(new TQmlWorldTransferCondition(!affected, qtTrId(affected ? Texts[i].first : Texts[i].second)));
    }
}

void TCharacterWorldTransferDialogController::createWorlds()
{
    for (const tibia::store::TTransferWorld& world : m_info.worlds)
        m_worlds.append(new TQmlTargetWorld(world.name, world.firstFlag, world.secondFlag, world.pvpType));
}

void TCharacterWorldTransferDialogController::closeDialog()
{
    dialog()->close();
    qDeleteAll(m_conditions);
    m_conditions.clear();
    qDeleteAll(m_worlds);
    m_worlds.clear();
    Q_EMIT dialogClosed();
}

void TCharacterWorldTransferDialogController::requestClose()
{
    closeDialog();
}

void TCharacterWorldTransferDialogController::requestWorldTransfer(const QString& TargetWorld)
{
    if (TargetWorld.isEmpty())
        return;
    Q_EMIT targetWorldConfirmed(m_offerId, TargetWorld);
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
    QString data;
    QFile file(QStringLiteral(":/data/ImportantBalancingMessageData.json"));
    if (file.open(QIODevice::ReadOnly))
        data = QString::fromUtf8(file.readAll());
    file.close();
    m_data = QJsonDocument::fromJson(data.toUtf8());
    m_id = m_data.object().value(QStringLiteral("id")).toInt();
}

QVariant TEmergencyDialogController::dialogData() const
{
    return QVariant(m_data.object());
}

/// CIP: the dialog closes; with "show again" cleared the message id is
/// stored before dialogClosed goes out.
void TEmergencyDialogController::onOKClicked()
{
    if (!m_showAgain)
        config::TClientOptions::instance()->setIntOption(config::EClientOption::emergencyDialogVersion, m_id);
    dialog()->close();
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

TGenericConfirmWithDontShowAgainDialogController::TGenericConfirmWithDontShowAgainDialogController(
    const QString& caption, const QString& content, const QString& dontShowAgain, const bool showAgain, QObject* parent)
    : TDialogController(QStringLiteral("GenericConfirmWithDontShowAgainDialog.qml"), parent)
{
    m_captionText = caption;
    m_contentText = content;
    m_dontShowAgainText = dontShowAgain;
    m_showThisDialogAgain = showAgain;
}

void TGenericConfirmWithDontShowAgainDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TGenericConfirmWithDontShowAgainDialogController::onYesClicked(const bool ShowDialogAgain)
{
    Q_EMIT yesClicked(ShowDialogAgain);
    dialog()->close();
}

void TGenericConfirmWithDontShowAgainDialogController::onNoClicked()
{
    Q_EMIT noClicked();
    dialog()->close();
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
void TLoginConfirmationCodeEnterTextDialogController::showDialog()
{
    m_enteredText.clear();
    dialog()->create();
    dialog()->show();
}

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

TMainCharacterChangeDialogController::~TMainCharacterChangeDialogController()
{
    closeDialog();
}

void TMainCharacterChangeDialogController::showDialog(const uint32_t OfferIDForMainCharChange,
                                                      std::vector<TPossibleMainCharacterInformation> PossibleMainCharInfo)
{
    closeDialog();
    m_offerId = OfferIDForMainCharChange;
    m_possibleMainChars = std::move(PossibleMainCharInfo);
    createMainCharInfos();
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
}

void TMainCharacterChangeDialogController::createMainCharInfos()
{
    for (const TPossibleMainCharacterInformation& info : m_possibleMainChars)
        m_mainCharInfos.append(new TQmlPossibleMainChar(info.characterName, info.worldName, info.vocation, info.level));
}

void TMainCharacterChangeDialogController::closeDialog()
{
    dialog()->close();
    qDeleteAll(m_mainCharInfos);
    m_mainCharInfos.clear();
}

void TMainCharacterChangeDialogController::requestClose()
{
    closeDialog();
}

void TMainCharacterChangeDialogController::requestMainCharChange(const QString& ChosenCharacter)
{
    if (ChosenCharacter.isEmpty())
        return;
    Q_EMIT mainCharChangeConfirmed(m_offerId, ChosenCharacter);
    closeDialog();
}

TMarketCoinTransactionDetailsDialogController::TMarketCoinTransactionDetailsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MarketCoinTransactionDetails.qml"), parent)
{}

void TMarketCoinTransactionDetailsDialogController::setTransactionDetails(const store::TTransactionDetails& details)
{
    const auto date = [](const uint32_t secs) {
        return QLocale().toString(QDateTime::fromSecsSinceEpoch(secs), QLocale::ShortFormat);
    };
    m_transactionType = details.type == store::TTransactionDetails::TYPE_MULTI ? MultiTransaction : SingleTransaction;
    m_singleTransactionDate = date(details.single.date);
    m_singleTransactionDescription = details.single.description;
    m_singleTransactionCharacterName = details.single.characterName;
    m_singleTransactionCoinBalanceChange = details.single.coinBalanceChange;
    m_singleTransactionPiecePrice = static_cast<qlonglong>(details.single.piecePrice);
    m_singleTransactionGoldBalanceChange = details.single.goldBalanceChange;
    m_multiTransactionCharacterName = details.multi.characterName;
    m_multiTransactionTotalCoins = static_cast<int>(details.multi.totalCoins);
    m_multiTransactionSoldCoins = static_cast<int>(details.multi.soldCoins);
    m_multiTransactionCoinsReainingInMarket = static_cast<int>(details.multi.coinsRemainingInMarket);
    m_multiTransactionPiecePrice = static_cast<qlonglong>(details.multi.piecePrice);
    m_multiTransactionReceivedGold = static_cast<qlonglong>(details.multi.receivedGold);
    Q_EMIT dataChanged();
    if (!m_transactionLoaded) {
        m_transactionLoaded = true;
        Q_EMIT transactionLoadedChanged();
    }
}

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

/// CIP: the skill rows in the dialog's order.
constexpr OfflineTrainingSkill kOfflineTrainingSkills[] = {
    OfflineTrainingSkill::OfflineTrainingSkillTypeMagic,
    OfflineTrainingSkill::OfflineTrainingSkillTypeFist,
    OfflineTrainingSkill::OfflineTrainingSkillTypeClub,
    OfflineTrainingSkill::OfflineTrainingSkillTypeSword,
    OfflineTrainingSkill::OfflineTrainingSkillTypeAxe,
    OfflineTrainingSkill::OfflineTrainingSkillTypeDistance,
};

}

TMultiOfflineTrainingDialogController::TMultiOfflineTrainingDialogController(QObject* parent)
    : TDialogController(QStringLiteral("MultiOfflineTrainingDialog.qml"), parent)
{
    s_instance = this;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMultiOfflineTrainingDialog", [this](GameObject*, const Args&) {
        showSkillsDialog();
        return std::any{};
    });
}

TMultiOfflineTrainingDialogController::~TMultiOfflineTrainingDialogController()
{
    clearSkillsModel();
    if (s_instance == this)
        s_instance = nullptr;
}

TMultiOfflineTrainingDialogController* TMultiOfflineTrainingDialogController::instance()
{
    return s_instance;
}

QList<QObject*> TMultiOfflineTrainingDialogController::skillsModel() const { return m_skillsModel; }

void TMultiOfflineTrainingDialogController::clearSkillsModel()
{
    const QList<QObject*> old = std::exchange(m_skillsModel, {});
    Q_EMIT skillsModelChanged();
    qDeleteAll(old);
}

void TMultiOfflineTrainingDialogController::rebuildSkillsModel()
{
    const QList<QObject*> old = std::exchange(m_skillsModel, {});
    for (const OfflineTrainingSkill skill : kOfflineTrainingSkills) {
        auto* data = new game::TOfflineSkillTrainingData(skill, this);
        data->assign(game::buildOfflineTrainingEntry(skill));
        m_skillsModel.append(data);
    }
    Q_EMIT skillsModelChanged();
    qDeleteAll(old);
}

void TMultiOfflineTrainingDialogController::handleGameAction(const input::TGameAction& action)
{
    if (action.type == input::EGameAction::SHOW_OFFLINE_TRAINING_SKILLS)
        showSkillsDialog();
}

void TMultiOfflineTrainingDialogController::showSkillsDialog()
{
    closeDialog();
    rebuildSkillsModel();
    dialog()->create();
    dialog()->show();
}

void TMultiOfflineTrainingDialogController::closeDialog()
{
    dialog()->close();
    clearSkillsModel();
}

void TMultiOfflineTrainingDialogController::requestClose()
{
    closeDialog();
}

void TMultiOfflineTrainingDialogController::startOfflineTraining(
    const cip::tibia::qmlcomponents::enums::EOfflineTrainingSkillType Skill)
{
    tibia::game::sendStartOfflineTraining(static_cast<uint8_t>(Skill));
    closeDialog();
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
    dialog()->close();
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
}

TReturnerRewardDialogController::TReturnerRewardDialogController(QObject* parent)
    : TDialogController(QStringLiteral("ReturnerRewardNotificationDialog.qml"), parent)
{}

QString TReturnerRewardDialogController::dialogText() const { return qtTrId("returner_reward_notification_text"); }

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

namespace {

/// Every race with a soul pit category, marked done when its animus mastery is
/// complete; mastered races without a category are appended.
std::vector<TSoulSealsMonsterListModel::Entry> soulSealsMonsterEntries(const std::vector<uint16_t>& animusMasteryRaceIds)
{
    std::unordered_set<uint16_t> mastered;
    for (const uint16_t raceId : animusMasteryRaceIds) {
        if (raceId != 0)
            mastered.insert(raceId);
    }
    const auto entryFor = [](const uint16_t raceId, const RaceType& race, const bool done) {
        TSoulSealsMonsterListModel::Entry entry;
        entry.raceId = static_cast<int>(raceId);
        entry.name = race.name.empty() ? QString::number(raceId) : QString::fromStdString(race.name);
        entry.animusMasteryCompleted = done;
        const uint32_t category = race.hasCategory ? race.category : 0;
        entry.soulPitFightPrice = race.hasCategory
            ? static_cast<int>(std::min<uint32_t>((category + 1U) * 10U, std::numeric_limits<uint16_t>::max()))
            : 0;
        entry.category = static_cast<int>(std::min<uint32_t>(category, std::numeric_limits<uint8_t>::max()));
        return entry;
    };
    std::vector<TSoulSealsMonsterListModel::Entry> entries;
    for (const auto& race : tibia::game::getAllRaces()) {
        if (!race.hasCategory || race.boss || race.raceId == 0 || race.raceId > std::numeric_limits<uint16_t>::max())
            continue;
        const auto raceId = static_cast<uint16_t>(race.raceId);
        entries.push_back(entryFor(raceId, race, mastered.erase(raceId) > 0));
    }
    for (const uint16_t raceId : mastered)
        entries.push_back(entryFor(raceId, tibia::game::raceData(raceId), true));
    return entries;
}

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

    connect(taskboard::TTaskboardProtocolMessageHandler::instance(), &taskboard::TTaskboardProtocolMessageHandler::openSoulSealsDialog,
            this, [this](const std::vector<uint16_t>& animusMasteryRaceIds) {
                applyEntries(soulSealsMonsterEntries(animusMasteryRaceIds));
                dialog()->create();
                dialog()->show();
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
