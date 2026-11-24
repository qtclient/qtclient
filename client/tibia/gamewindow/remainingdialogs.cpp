#include "remainingdialogs.h"
#include "protocol/clientmessages.h"
#include "shared/experiencegainchangetracker.h"
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
#include <protocol.pb.h>
#include "game/gamesessionbase.h"
#include "game/offlineskilltrainingdata.h"
#include "game/tgamenotify.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "game/gamesessionbase.h"
#include "taskboard/taskboardciptypes.h"
#include "taskboard/taskboardcurrenciesstorage.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "taskboard/monsterlistfiltermodel.h"

#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/assets/things.h>
#include <tibia/appearances/appearancetype.h>

#include <QColor>

#include <algorithm>
#include <limits>
#include <tuple>
#include <unordered_set>
#include <tibia/game/gamenotify.h>
#include <tibia/appearances/appearanceinstance.h>

namespace tibia::gamewindow {

namespace {
using A = input::EGameAction;
using Args = std::vector<std::any>;
constexpr auto AskBeforeStowOption = "askBeforeStowContainerContent";


QString itemNameOf(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, typeId);
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

TCharacterTradeDialogController::TCharacterTradeDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("charactertrade/CharacterTradeDialog.qml"), Engine, DialogParentItem, parent)
    , m_auction(new TCharacterAuctionConfiguration(this))
    , m_conditions(new TCharacterTradeNotificationsAbstractListModel(this))
    , m_highlights(new TTextualHighlightsSortProxyModel(this))
    , m_items(new TItemHighlightsSortProxyModel(this))
    , m_confirmations(new TCharacterTradeNotificationsAbstractListModel(this))
    , m_overviewHighlights(new TTextualHighlightsAbstractListModel(this))
    , m_overviewItems(new TItemHighlightsAbstractListModel(this))
{
    m_auctionConfiguration = m_auction;
    m_characterData = new TQMLCharacterData(this);
    m_conditionsModel = m_conditions;
    m_highlightsModel = m_highlights;
    m_itemsModel = m_items;
    m_confirmationsModel = m_confirmations;
    m_characterOverviewHighlightsModel = m_overviewHighlights;
    m_characterOverviewItemsModel = m_overviewItems;
}

void TCharacterTradeDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    closeDialog();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_storage = session->characterTradeStorage().get();
    m_gameSessionConnections.push_back(
        connect(m_storage, &trade::TCharacterTradeStorage::infoUpdated, this, &TCharacterTradeDialogController::onInfoUpdated));
}

TCharacterTradeDialogController::~TCharacterTradeDialogController() = default;

void TCharacterTradeDialogController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (pGameAction && pGameAction->gameActionType() == input::EGameAction::OPEN_TRADE_CHARACTER_DIALOG)
        showDialog();
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
    if (m_gameSession) {
        tibia::protocol::gameclient::sendCharacterTradeConfigurationAction(0);
        tibia::protocol::gameclient::sendCharacterTradeConfigurationAction(1);
    }
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
    Q_EMIT dialogClosed();
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
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        session)
        tibia::protocol::gameclient::sendCharacterTradeConfigurationAction(0);
    m_hasUnfulfilledConditions = true;
    Q_EMIT hasUnfulfilledConditionsChanged();
}

void TCharacterTradeDialogController::requestCheckConfiguration()
{
    const TConfiguration config = configuration();
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        session)
        tibia::protocol::gameclient::sendCharacterTradeConfigurationAction(2, config.minimumBid, config.auctionEnd, config.items, config.storeItems, config.highlights);
    m_hasConfirmationError = true;
    Q_EMIT hasConfirmationErrorChanged();
}

void TCharacterTradeDialogController::requestConfirmConfiguration()
{
    const TConfiguration config = configuration();
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        session)
        tibia::protocol::gameclient::sendCharacterTradeConfigurationAction(3, config.minimumBid, config.auctionEnd, config.items, config.storeItems, config.highlights);
}

void TCharacterTradeDialogController::onInfoUpdated(const trade::TCharacterTradeStorage::DataType dataType)
{
    trade::TCharacterTradeStorage* storage = m_storage;
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

TCharacterWorldTransferDialogController::~TCharacterWorldTransferDialogController()
{
    closeDialog();
}

bool TCharacterWorldTransferDialogController::isExpressWorldTransfer() const
{
    return m_isExpressWorldTransfer;
}

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

TConfirmMoveObjectToStashDialogController::TConfirmMoveObjectToStashDialogController(config::TClientOptions* ClientOptions,
                                                                                     QQmlEngine* Engine,
                                                                                     QQuickItem* DialogParentItem,
                                                                                     QObject* parent)
    : TDialogController(QStringLiteral("ConfirmMoveObjectToStashDialog.qml"), Engine, DialogParentItem, parent)
    , m_clientOptions(ClientOptions)
{}

void TConfirmMoveObjectToStashDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    const auto* show = input::gameActionAs<input::TGameActionShowConfirmMoveObjectToStashDialog>(pGameAction.get());
    if (!show)
        return;
    m_gameAction = show->gameAction();
    dialog()->create();
    dialog()->show();
}

bool TConfirmMoveObjectToStashDialogController::showThisDialogAgain() const
{
    return app::ClientOptions::load().options().value(QLatin1String(AskBeforeStowOption)).toBool(true);
}

void TConfirmMoveObjectToStashDialogController::closeDialog()
{
    m_gameAction.reset();
    dialog()->close();
}

void TConfirmMoveObjectToStashDialogController::showDialog()
{
    dialog()->create();
    dialog()->show();
}

void TConfirmMoveObjectToStashDialogController::onYesClicked(const bool ShowDialogAgain)
{
    app::ClientOptions::store(QLatin1String(AskBeforeStowOption), ShowDialogAgain);
    if (m_gameAction && m_gameAction->gameActionType() == input::EGameAction::MOVE_OBJECT) {
        if (const auto move = std::dynamic_pointer_cast<const input::TGameActionMoveObject>(m_gameAction))
            move->setStashConfirmed(true);
    }
    if (m_gameAction && m_gameAction->gameActionType() == input::EGameAction::STASH_STOW_CONTAINER_CONTENT) {
        if (const auto content = std::dynamic_pointer_cast<const input::TGameActionStashStowContainerContent>(m_gameAction))
            content->setStashConfirmed(true);
    }
    Q_EMIT publishGameAction(m_gameAction);
    closeDialog();
}

void TConfirmMoveObjectToStashDialogController::onNoClicked()
{
    closeDialog();
}

TCrashReportDialogController::TCrashReportDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CrashReportDialog.qml"), parent)
{}

TCrashReportDialogController::TCrashReportDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("CrashReportDialog.qml"), Engine, DialogParentItem, parent)
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
                                                      std::vector<store::TPossibleMainCharacterInformation> PossibleMainCharInfo)
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
    for (const store::TPossibleMainCharacterInformation& info : m_possibleMainChars)
        m_mainCharInfos.append(new TQmlPossibleMainChar(info.name, info.world, info.vocation, info.level));
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

TMultiOfflineTrainingDialogController::TMultiOfflineTrainingDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("MultiOfflineTrainingDialog.qml"), Engine, DialogParentItem, parent)
    , m_skillsWidgetEntryUpdater(std::make_shared<shared::TExperienceGainChangeTracker>())
{
    m_skillsWidgetEntryUpdater.setPrecisePercent(true);
}

void TMultiOfflineTrainingDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeDialog();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->gameProtocolMessageHandler().get(),
                                               &game::TGameProtocolMessageHandler::requestShowMultiOfflineTrainingDialog,
                                               this, &TMultiOfflineTrainingDialogController::showSkillsDialog));
}

TMultiOfflineTrainingDialogController::~TMultiOfflineTrainingDialogController()
{
    clearSkillsModel();
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
        data->assign(game::buildSkillsWidgetEntry(game::skillTypeForOfflineTraining(skill), m_skillsWidgetEntryUpdater.precisePercent()));
        m_skillsModel.append(data);
    }
    Q_EMIT skillsModelChanged();
    qDeleteAll(old);
}

void TMultiOfflineTrainingDialogController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() == input::EGameAction::SHOW_OFFLINE_TRAINING_SKILLS)
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
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        ::tibia::protobuf::protocol::GameclientMessageStartOfflineTraining message;
        message.set_skill(static_cast<uint32_t>(Skill));
        Q_EMIT session->gameProtocolMessageHandler()->sendStartOfflineTraining(message);
    }
    closeDialog();
}

TOutfitMemorialDialogController::TOutfitMemorialDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("OutfitMemorialDialog.qml"), Engine, DialogParentItem, parent)
{
}

void TOutfitMemorialDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    dialog()->close();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->gameProtocolMessageHandler().get(),
                                               &game::TGameProtocolMessageHandler::requestOpenOutfitMemorialDialog,
                                               this, &TOutfitMemorialDialogController::showMemorial));
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

TShowImageDialogController::TShowImageDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("ShowImageDialog.qml"), Engine, DialogParentItem, parent)
{
}

TShowImageDialogController::~TShowImageDialogController()
{
    setData({});
}

void TShowImageDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != input::EGameAction::OPEN_SHOW_IMAGE_DIALOG)
        return;
    if (const auto* action = input::gameActionAs<input::TGameActionOpenShowImageDialog>(pGameAction))
        handleShowImageDialogGameAction(*action);
}

void TShowImageDialogController::handleShowImageDialogGameAction(const input::TGameActionOpenShowImageDialog& pGameAction)
{
    setData(pGameAction.data());
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

TShowSpellBookDialogController::TShowSpellBookDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("ShowSpellBookDialog.qml"), Engine, DialogParentItem, parent)
{
}

void TShowSpellBookDialogController::connectToGameSession(const std::shared_ptr<tibia::game::IGameSession>& GameSession)
{
    dialog()->close();
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const tibia::game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.push_back(connect(session->spellBookProtocolMessageHandler().get(),
                                               &tibia::game::TSpellBookProtocolMessageHandler::requestShowSpellBookDialog, this,
                                               &TShowSpellBookDialogController::showSpellBookDialog));
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
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(8);
    Q_EMIT publishGameAction(action);
}

void TShowSpellBookDialogController::requestClose() { dialog()->close(); }

namespace {

/// The bestiary monsters (no bosses) of the given occurrences that carry the
/// static data's sixth flag, in static data order.
std::vector<uint16_t> monsterRaceIdsOfOccurrences(const std::set<int>& occurrences)
{
    std::vector<uint16_t> raceIds;
    for (const RaceType& race : tibia::appearances::TAppearancesManager::instance().getAllRaces()) {
        if (race.boss)
            continue;
        if (occurrences.count(static_cast<int>(race.occurrence)) != 0 && race.category != 0)
            raceIds.push_back(static_cast<uint16_t>(race.raceId));
    }
    return raceIds;
}

/// The soul pit races: common and uncommon monsters.
std::vector<uint16_t> soulSealsMonsterRaceIds()
{
    return monsterRaceIdsOfOccurrences({ 0, 1 });
}

/// The name of a bestiary difficulty (harmless .. challenging).
QString monsterDifficultyName(const int difficulty)
{
    static const char* const Names[] = {
        "monsters_difficulty_harmless", "monsters_difficulty_trivial", "monsters_difficulty_easy",
        "monsters_difficulty_medium",   "monsters_difficulty_hard",    "monsters_difficulty_challenging",
    };
    if (static_cast<unsigned>(difficulty) < 6)
        return qtTrId(Names[difficulty]);
    return qtTrId("dummy_unknown");
}

/// The soul seals dialog's name of a difficulty filter.
struct TDifficultyFilterName
{
    QString operator()(taskboard::ShowAll) const { return qtTrId("soulseals_dialog_filter_all"); }
    QString operator()(const int difficulty) const { return monsterDifficultyName(difficulty); }
};

}

TSoulSealsDialogController::TSoulSealsDialogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("taskboard/SoulSealsDialog.qml"), Engine, DialogParentItem, parent)
    , m_monsterListProxy(soulSealsMonsterRaceIds())
{
    m_selectedMonsterRaceID = 0;
    m_indexOfSelectedDifficulty = -1;
    m_difficultyFilters = { taskboard::ShowAll{}, 0, 1, 2, 3, 4, 5 };
}

void TSoulSealsDialogController::connectToGameSession(std::shared_ptr<game::IGameSession> GameSession)
{
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_gameSession = std::move(GameSession);
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_gameSessionConnections.add(connect(session->taskboardCurrenciesStorage().get(),
                                         &taskboard::TTaskboardCurrenciesStorage::soulSealsAmountChanged, this,
                                         &TSoulSealsDialogController::soulSealsChanged));
    m_gameSessionConnections.add(connect(session->taskboardProtocolMessageHandler(),
                                         &taskboard::TTaskboardProtocolMessageHandler::openSoulSealsDialog, this,
                                         &TSoulSealsDialogController::showDialog));
    m_gameSessionConnections.add(connect(&m_fightConfirmation, &TMessageDialogController::acceptClicked, this,
                                         &TSoulSealsDialogController::onFightConfirmed));
    m_gameSessionConnections.add(connect(&m_fightConfirmation, &TMessageDialogController::discardClicked, this,
                                         &TSoulSealsDialogController::onFightDiscarded));
}

void TSoulSealsDialogController::showDialog(const std::vector<uint16_t>& AnimusMasteryRaceIds)
{
    m_monsterListProxy.monsterListFilterModel()->setCompletedAnimusMasteryRaceIds(AnimusMasteryRaceIds);
    m_monsterListProxy.invalidate();
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
    if (m_indexOfSelectedDifficulty != 0) {
        m_indexOfSelectedDifficulty = 0;
        if (!m_difficultyFilters.empty()) {
            m_monsterListProxy.setDifficultyFilter(m_difficultyFilters.front());
            Q_EMIT selectedDifficultyChanged();
        }
    }
    setMonstersNameFilter(QString());
}

void TSoulSealsDialogController::onFightConfirmed()
{
    if (!m_gameSession)
        return;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        session && static_cast<uint16_t>(m_selectedMonsterRaceID) != 0)
        tibia::protocol::gameclient::sendSoulSealsAction(static_cast<uint16_t>(m_selectedMonsterRaceID));
    closeDialog();
}

void TSoulSealsDialogController::onFightDiscarded()
{
    m_fightConfirmation.dialog()->close();
}

void TSoulSealsDialogController::setIndexOfSelectedDifficulty(const int v)
{
    if (m_indexOfSelectedDifficulty == v)
        return;
    m_indexOfSelectedDifficulty = v;
    // Host: CipSoft only checks the upper bound.
    if (v >= 0 && v < static_cast<int>(m_difficultyFilters.size())) {
        m_monsterListProxy.setDifficultyFilter(m_difficultyFilters[static_cast<size_t>(v)]);
        Q_EMIT selectedDifficultyChanged();
    }
}

void TSoulSealsDialogController::setMonstersNameFilter(const QString& v)
{
    if (m_monstersNameFilter == v)
        return;
    m_monstersNameFilter = v;
    m_monsterListProxy.setNameFilter(m_monstersNameFilter);
    Q_EMIT monstersNameFilterChanged();
}

void TSoulSealsDialogController::closeDialog()
{
    m_fightConfirmation.dialog()->close();
    dialog()->close();
}

QString TSoulSealsDialogController::selectedMonstername() const
{
    return cyclopedia::capitalizeWords(QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(static_cast<uint16_t>(m_selectedMonsterRaceID)).name));
}

int TSoulSealsDialogController::selectedFightPrice() const
{
    return taskboard::soulPitFightPrice(tibia::appearances::TAppearancesManager::instance().getRaceData(static_cast<uint16_t>(m_selectedMonsterRaceID)).difficulty);
}

QString TSoulSealsDialogController::soulSealsString() const
{
    if (!m_gameSession)
        return QStringLiteral("0");
    return QLocale(QLocale::English).toString(availableSoulSeals());
}

QStringList TSoulSealsDialogController::availableMonsterDifficulties() const
{
    QStringList names;
    if (!m_gameSession)
        return names;
    for (const taskboard::TMonsterDifficultyFilter& filter : m_difficultyFilters)
        names.append(std::visit(TDifficultyFilterName{}, filter));
    return names;
}

int TSoulSealsDialogController::availableSoulSeals() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session ? session->taskboardCurrenciesStorage()->soulSealsAmount() : 0;
}

QObject* TSoulSealsDialogController::monsterList() const
{
    return const_cast<taskboard::TMonsterListModelSortFilterProxy*>(&m_monsterListProxy);
}

void TSoulSealsDialogController::setSelectedMonsterRaceID(const int v)
{
    if (m_selectedMonsterRaceID == v)
        return;
    m_selectedMonsterRaceID = v;
    Q_EMIT selectedMonsterRaceIDChanged();
}

void TSoulSealsDialogController::fightselectedMonster()
{
    m_fightConfirmation.setMessageDialogTextFormat(cip::tibia::gamewindow::TMessageDialogController::RICH_TEXT);
    const QString caption = qtTrId("soulseals_fight_confirmation_caption");
    const QString text = qtTrId("soulseals_fight_confirmation_text").arg(selectedMonstername()).arg(selectedFightPrice());
    m_fightConfirmation.setButtons(okCancelDialogButtons());
    m_fightConfirmation.showMessage(caption, text);
}

void TSoulSealsDialogController::requestClose()
{
    closeDialog();
}

TSurveyInvitationDialogController::TSurveyInvitationDialogController(QObject* parent)
    : TDialogController(QStringLiteral("SurveyInvitatioDialog.qml"), parent)
{
    m_showAskLater = true;
}

void TSurveyInvitationDialogController::setSurveyData(const authentication::TSurveyData& Survey)
{
    m_surveyId = static_cast<int>(Survey.id);
    m_invitationText = Survey.invitationText;
    m_token = Survey.invitationToken;
    m_date = Survey.endTimestamp;
    config::TClientOptions* options = config::TClientOptions::instance();
    if (!options)
        return;
    if (options->surveyInvitationOptions().id != Survey.id) {
        config::TSurveyInvitationOptions survey = options->surveyInvitationOptions();
        survey.id = Survey.id;
        survey.status = 0;
        options->setSurveyInvitationOptions(survey);
        Q_EMIT contentChanged();
    }
    m_showAskLater = options->surveyInvitationOptions().status == 0;
    Q_EMIT contentChanged();
}

bool TSurveyInvitationDialogController::showSurveyInvitation(const authentication::TSurveyData& Survey)
{
    setSurveyData(Survey);
    config::TClientOptions* options = config::TClientOptions::instance();
    if (!options || options->surveyInvitationOptions().status == 2)
        return false;
    setSurveyStatus(1);
    Q_EMIT contentChanged();
    dialog()->create();
    dialog()->show();
    return true;
}

QString TSurveyInvitationDialogController::dateString() const
{
    const QDateTime date = QDateTime::fromSecsSinceEpoch(m_date);
    return QLocale(QLocale::English, QLocale::UnitedKingdom).toString(date, QStringLiteral("MMMM d, yyyy"));
}

void TSurveyInvitationDialogController::setSurveyStatus(const int status)
{
    config::TClientOptions* options = config::TClientOptions::instance();
    if (!options)
        return;
    config::TSurveyInvitationOptions survey = options->surveyInvitationOptions();
    survey.status = status;
    options->setSurveyInvitationOptions(survey);
}

void TSurveyInvitationDialogController::onYesClicked()
{
    dialog()->close();
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenSurveyUrl>(m_surveyId, m_token));
    setSurveyStatus(2);
    Q_EMIT contentChanged();
    Q_EMIT dialogClosed();
}

void TSurveyInvitationDialogController::onNoClicked()
{
    dialog()->close();
    setSurveyStatus(2);
    Q_EMIT contentChanged();
    Q_EMIT dialogClosed();
    Q_EMIT abort();
}

void TSurveyInvitationDialogController::onAskLaterClicked()
{
    dialog()->close();
    setSurveyStatus(1);
    Q_EMIT contentChanged();
    Q_EMIT dialogClosed();
}

TTutorialOverlayItemController::TTutorialOverlayItemController(QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayItemController(parent)
{}

TTutorialOverlayItemController::TTutorialOverlayItemController(QQuickItem* item)
    : cip::tibia::gamewindow::TTutorialOverlayItemController(nullptr)
    , m_item(item)
{}

TTutorialOverlayItemController::~TTutorialOverlayItemController()
{
    if (!m_item)
        return;
    m_item->setParent(nullptr);
    m_item->deleteLater();
    m_item = nullptr;
}

TTutorialOverlayMouseFilterController::TTutorialOverlayMouseFilterController(QObject* parent)
    : cip::tibia::gamewindow::TTutorialOverlayMouseFilterController(parent)
{}

TTutorialOverlayMouseFilterController::TTutorialOverlayMouseFilterController(QQuickItem* item)
    : cip::tibia::gamewindow::TTutorialOverlayMouseFilterController(nullptr)
    , m_item(item)
{}

TTutorialOverlayMouseFilterController::~TTutorialOverlayMouseFilterController()
{
    if (!m_item)
        return;
    m_item->setParent(nullptr);
    m_item->deleteLater();
    m_item = nullptr;
}


} // namespace tibia::gamewindow
