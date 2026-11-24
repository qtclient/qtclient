#include "trade/tradeciptypes.h"
#include "chat/chatchannel.h"
#include "creatures/creaturestorage.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"

#include <QPointer>
#include "game/worldtype.h"
#include <stdexcept>
#include <tuple>
#include <client/outfit.h>
#include "creatures/creatureprotocolapply.h"

#include "container/containerciptypes.h"
#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::trade {

namespace {

using Args = std::vector<std::any>;
using client::gamecore::argInt;
using client::gamecore::argString;

/// The vocation's name; promoted vocations have their own names.
QString vocationName(const int vocation, const bool promoted)
{
    static constexpr const char* Base[] = { "knight", "paladin", "sorcerer", "druid", "monk" };
    static constexpr const char* Promoted[] = { "knight_promoted", "paladin_promoted", "sorcerer_promoted", "druid_promoted", "monk_promoted" };
    if (vocation < 1 || vocation > 5)
        return qtTrId("none");
    return qtTrId(promoted ? Promoted[vocation - 1] : Base[vocation - 1]);
}

/// A highlight group's category: the protocol values 0..14 keep their value;
/// any other value throws.
int highlightCategory(const uint32_t protocolValue)
{
    if (protocolValue > 14)
        throw std::runtime_error("unhandled enum value");
    return static_cast<int>(protocolValue);
}

} // namespace

TCharacterTradeStorage::TCharacterTradeStorage(QObject* parent)
    : cip::tibia::trade::TCharacterTradeStorage(parent)
{
}

void TCharacterTradeStorage::setBasicData(TCharacterTradeBasicData data)
{
    m_basicData = std::move(data);
    Q_EMIT infoUpdated(Conditions);
}

void TCharacterTradeStorage::setItems(std::vector<TCharacterTradeItem> items)
{
    m_items = std::move(items);
    Q_EMIT infoUpdated(Items);
}

void TCharacterTradeStorage::setStoreItems(std::vector<TCharacterTradeItem> items)
{
    m_storeItems = std::move(items);
    Q_EMIT infoUpdated(StoreItems);
}

void TCharacterTradeStorage::setHighlights(std::vector<TCharacterTradeHighlightGroup> highlights)
{
    m_highlights = std::move(highlights);
    Q_EMIT infoUpdated(Highlights);
}

void TCharacterTradeStorage::setSummary(std::vector<QString> problems, const quint16 fee)
{
    m_problems = std::move(problems);
    m_fee = fee;
    Q_EMIT infoUpdated(Summary);
}

namespace {

/// The quest flag currency's resource type.
constexpr uint32_t QuestFlagCurrencyResource = 60;

template<typename Message>
std::vector<::tibia::container::TInventorySlot> offerItems(const Message& m)
{
    std::vector<::tibia::container::TInventorySlot> items;
    items.reserve(static_cast<size_t>(m.items_size()));
    for (const auto& item : m.items())
        items.push_back(::tibia::container::TInventorySlot::fromProtocolInventoryItem(item));
    return items;
}

}

TNPCTradeProtocolMessageHandler::TNPCTradeProtocolMessageHandler(TNPCTradeStorage* storage, QObject* parent)
    : cip::tibia::trade::TNPCTradeProtocolMessageHandler(parent)
    , m_storage(storage)
{
    if (m_storage)
        m_storage->setProtocolMessageHandler(this);
}

TNPCTradeProtocolMessageHandler::~TNPCTradeProtocolMessageHandler()
{
    if (m_storage)
        m_storage->setProtocolMessageHandler(nullptr);
}

void TNPCTradeProtocolMessageHandler::sendClose()
{
    Q_EMIT sendCloseNPCTrade(::tibia::protobuf::protocol::GameclientMessageCloseNPCTrade());
    if (m_storage)
        m_storage->setTraderInventory({}, {});
}

void TNPCTradeProtocolMessageHandler::sendLook(const TNPCTradeGood& good)
{
    if (good.typeId() == 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageLookNPCTrade message;
    message.set_object_id(good.typeId());
    message.set_count(static_cast<uint8_t>(good.countOrSubType()));
    Q_EMIT sendLookNPCTrade(message);
}

void TNPCTradeProtocolMessageHandler::sendInspect(const TNPCTradeGood& good)
{
    if (good.typeId() == 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageInspectObject message;
    message.set_inspection_type(tibia::client::INSPECT_NPCTRADE);
    message.mutable_object_identifier()->set_type_id(good.typeId());
    message.mutable_object_identifier()->set_data(static_cast<uint8_t>(good.countOrSubType()));
    Q_EMIT sendInspectObject(message);
}

void TNPCTradeProtocolMessageHandler::sendBuy(const TNPCTradeGood& good, const int amount, const bool ignoreCapacity,
                                              const bool withBackpacks)
{
    if (good.typeId() == 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageBuyObject message;
    message.set_object_id(good.typeId());
    message.set_subtype(static_cast<uint8_t>(good.countOrSubType()));
    message.set_amount(static_cast<uint16_t>(amount));
    message.set_ignore_capacity(ignoreCapacity);
    message.set_in_backpacks(withBackpacks);
    Q_EMIT sendBuyObject(message);
}

void TNPCTradeProtocolMessageHandler::sendSell(const TNPCTradeGood& good, const int amount, const bool ignoreEquipped)
{
    if (good.typeId() == 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageSellObject message;
    message.set_object_id(good.typeId());
    message.set_subtype(static_cast<uint8_t>(good.countOrSubType()));
    message.set_amount(static_cast<uint16_t>(amount));
    message.set_ignore_equipped(ignoreEquipped);
    Q_EMIT sendSellObject(message);
}

void TNPCTradeProtocolMessageHandler::handleNPCOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageNPCOffer& message)
{
    std::vector<TNPCTradeGood> goods;
    for (const auto& offer : message.offers()) {
        goods.push_back({ ::tibia::container::TInventorySlot::fromTypeId(static_cast<uint16_t>(offer.object_id()),
                                                                        static_cast<uint8_t>(offer.data())),
                          QString::fromStdString(offer.name()), offer.sell_price(), offer.buy_price(), offer.weight() });
    }
    if (m_storage)
        m_storage->setTraderInventory(QString::fromStdString(message.npc_name()), std::move(goods));

    // An object currency other than gold, else a named currency, else gold.
    constexpr uint16_t GoldCoinTypeId = 3031;
    std::optional<TNPCTradeStorage::TCurrency> currency;
    const auto currencyObject = static_cast<uint16_t>(message.currency_object_id());
    if (message.has_currency_object_id() && currencyObject != 0) {
        if (currencyObject != GoldCoinTypeId)
            currency = currencyObject;
    } else if (message.has_currency_name() && !message.currency_name().empty()) {
        currency = QString::fromStdString(message.currency_name());
    }
    if (m_storage)
        m_storage->setCurrency(currency);
    Q_EMIT requestOpenNpcTrade();
}

void TNPCTradeProtocolMessageHandler::handlePlayerGoodsMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerGoods& message)
{
    std::map<uint16_t, uint16_t> goods;
    for (const auto& good : message.goods())
        goods[static_cast<uint16_t>(good.object_id())] = static_cast<uint16_t>(good.amount());
    if (m_storage)
        m_storage->setPlayerGoods(goods);
}

void TNPCTradeProtocolMessageHandler::handleResourceBalanceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    if (message.resource_type() == QuestFlagCurrencyResource && m_storage)
        m_storage->setQuestFlagCurrencyBalance(message.small_value());
}

void TNPCTradeProtocolMessageHandler::handleCloseNPCTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseNPCTrade&)
{
    if (m_storage)
        m_storage->setTraderInventory({}, {});
    Q_EMIT requestCloseNpcTrade();
}

void TNPCTradeProtocolMessageHandler::handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead&)
{
    Q_EMIT requestCloseNpcTrade();
}

TPlayerTradeProtocolMessageHandler::TPlayerTradeProtocolMessageHandler(TPlayerTradeStorage& storage, QObject* parent)
    : cip::tibia::trade::TPlayerTradeProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

TPlayerTradeProtocolMessageHandler::~TPlayerTradeProtocolMessageHandler() = default;

void TPlayerTradeProtocolMessageHandler::acceptTrade()
{
    Q_EMIT sendAcceptTrade(::tibia::protobuf::protocol::GameclientMessageAcceptTrade());
}

void TPlayerTradeProtocolMessageHandler::rejectTrade()
{
    Q_EMIT sendRejectTrade(::tibia::protobuf::protocol::GameclientMessageRejectTrade());
    m_storage.reset();
}

void TPlayerTradeProtocolMessageHandler::lookTrade(const int index, const int side)
{
    if (static_cast<unsigned>(side) >= 2)
        throw std::runtime_error("unhandled enum value");
    ::tibia::protobuf::protocol::GameclientMessageLookTrade message;
    message.set_counter_offer(side != 0 ? 1u : 0u);
    message.set_index(static_cast<uint32_t>(index));
    Q_EMIT sendLookTrade(message);
}

void TPlayerTradeProtocolMessageHandler::inspectTrade(const int index, const int side)
{
    if (static_cast<unsigned>(side) >= 2)
        throw std::runtime_error("unhandled enum value");
    ::tibia::protobuf::protocol::GameclientMessageInspectObject message;
    message.set_inspection_type(tibia::client::INSPECT_PLAYERTRADE);
    message.set_trade_side(side != 0 ? 1u : 0u);
    message.set_trade_index(static_cast<uint32_t>(index));
    Q_EMIT sendInspectObject(message);
}

void TPlayerTradeProtocolMessageHandler::handleOwnOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageOwnOffer& OwnOfferMessage)
{
    m_storage.setOwnOffer(QString::fromStdString(OwnOfferMessage.name()), offerItems(OwnOfferMessage));
    Q_EMIT requestOpenPlayerTradeWidget();
    // Host: product packs follow trades on the notify bus.
    tibia::game::processOwnTrade(tibia::game::formatCreatureName(OwnOfferMessage.name()), offerItems(OwnOfferMessage));
}

void TPlayerTradeProtocolMessageHandler::handleCounterOfferMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCounterOffer& CounterOfferMessage)
{
    m_storage.setCounterOffer(QString::fromStdString(CounterOfferMessage.name()), offerItems(CounterOfferMessage));
    tibia::game::processCounterTrade(tibia::game::formatCreatureName(CounterOfferMessage.name()), offerItems(CounterOfferMessage));
}

void TPlayerTradeProtocolMessageHandler::handleCloseTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseTrade&)
{
    Q_EMIT requestClosePlayerTradeWidget();
    m_storage.reset();
}

TCharacterTradeProtocolMessageHandler::TCharacterTradeProtocolMessageHandler(TCharacterTradeStorage* storage, QObject* parent)
    : cip::tibia::trade::TCharacterTradeProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

void TCharacterTradeProtocolMessageHandler::handleCharacterTradeConfigurationMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCharacterTradeConfiguration& m)
{
    if (m.data_type() > TCharacterTradeStorage::Summary)
        throw std::logic_error("unhandled enum value");
    TCharacterTradeStorage* storage = m_storage;
    if (!storage)
        return;
    switch (m.data_type()) {
    case TCharacterTradeStorage::Conditions: {
        TCharacterTradeBasicData data;
        data.name = QString::fromStdString(m.name());
        data.level = static_cast<quint16>(m.level());
        // Promoted vocations come as the base vocation plus ten.
        data.vocation = vocationName(static_cast<int>(m.vocation() % 10), m.vocation() >= 10);
        data.worldType = game::worldTypeName(static_cast<int>(m.world_type()));
        data.startTime = m.start_time();
        data.endTime = m.end_time();
        data.minimumBid = m.minimum_bid();
        for (const auto& condition : m.conditions())
            data.conditions.push_back({ QString::fromStdString(condition.text()), condition.fulfilled() });
        data.outfit = creatures::outfitFromProtocol(m.outfit(), nullptr);
        storage->setBasicData(std::move(data));
        break;
    }
    case TCharacterTradeStorage::Items:
    case TCharacterTradeStorage::StoreItems: {
        std::vector<TCharacterTradeItem> items;
        for (const auto& item : m.items())
            items.push_back({ static_cast<quint16>(item.object().object_id()), static_cast<quint8>(item.object().tier()), item.count() });
        if (m.data_type() == TCharacterTradeStorage::Items)
            storage->setItems(std::move(items));
        else
            storage->setStoreItems(std::move(items));
        break;
    }
    case TCharacterTradeStorage::Highlights: {
        std::vector<TCharacterTradeHighlightGroup> groups;
        for (const auto& group : m.groups()) {
            TCharacterTradeHighlightGroup entry;
            entry.category = highlightCategory(static_cast<uint32_t>(group.category()));
            entry.name = QString::fromStdString(group.name());
            for (const auto& highlight : group.entries())
                entry.entries.push_back({ static_cast<quint16>(highlight.entry_id()), QString::fromStdString(highlight.text()) });
            groups.push_back(std::move(entry));
        }
        storage->setHighlights(std::move(groups));
        break;
    }
    case TCharacterTradeStorage::Summary: {
        std::vector<QString> problems;
        for (const std::string& problem : m.problems())
            problems.push_back(QString::fromStdString(problem));
        storage->setSummary(std::move(problems), static_cast<quint16>(m.fee()));
        break;
    }
    default:
        break;
    }
}

TPlayerTradeGameActionHandler::TPlayerTradeGameActionHandler(creatures::TCreatureStorage* pCreatureStorage, QObject* parent)
    : cip::tibia::trade::TPlayerTradeGameActionHandler(parent)
    , m_pCreatureStorage(pCreatureStorage)
{
}

TPlayerTradeGameActionHandler::~TPlayerTradeGameActionHandler()
{
    // Host: the map's selection callback must not outlive the handler.
    if (m_pPendingAction && input::isSelectingTarget())
        input::finishTargetSelection(nullptr);
}

void TPlayerTradeGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
        case input::EGameAction::TRADE_ITEM:
            handleTradeItemGameAction(std::dynamic_pointer_cast<const input::TGameActionPlayerTradeItem>(pGameAction));
            return;
        case input::EGameAction::TARGET_SELECTED:
            handleTargetSelectedGameAction(std::dynamic_pointer_cast<const input::TGameActionTargetSelected>(pGameAction));
            return;
        default:
            return;
    }
}

void TPlayerTradeGameActionHandler::handleTradeItemGameAction(const std::shared_ptr<const input::TGameActionPlayerTradeItem>& pAction)
{
    if (!pAction)
        return;
    if (pAction->targetCreatureId() == 0) {
        m_pPendingAction = pAction;
        // Host: the world map hands its click to the selection callback; it
        // comes back as the TARGET_SELECTED every window publishes.
        QPointer<TPlayerTradeGameActionHandler> self(this);
        input::startTargetSelection([self](const input::TTargetSelectedPtr& pTarget) {
            if (!self)
                return;
            if (pTarget)
                Q_EMIT self->publishGameAction(pTarget);
            else
                self->m_pPendingAction.reset();
        });
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionStartTargetSelection>());
        return;
    }
    const int stackPosition = pAction->stackPosition();
    ::tibia::protobuf::protocol::GameclientMessageTradeObject message;
    protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(),
                                           pAction->position(), stackPosition < 0 ? 0 : stackPosition, pAction->objectClientId());
    message.set_creature_id(pAction->targetCreatureId());
    Q_EMIT sendTradeObject(message);
}

void TPlayerTradeGameActionHandler::handleTargetSelectedGameAction(const std::shared_ptr<const input::TGameActionTargetSelected>& pTarget)
{
    if (!m_pPendingAction)
        return;
    const std::shared_ptr<const input::TGameActionPlayerTradeItem> pending = std::move(m_pPendingAction);
    m_pPendingAction.reset();
    const input::TGameActionContextPtr& object = pTarget ? pTarget->object() : input::TGameActionContextPtr();
    if (!object || object->gameActionContextType() != input::EGameActionContextType::OBJECT_CREATURE) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(qtTrId("player_trade_select_player_to_trade"), chat::systemWhite()));
        return;
    }
    const uint32_t creatureId = pTarget->creatureId();
    const std::shared_ptr<creatures::TCreature> creature = m_pCreatureStorage ? m_pCreatureStorage->creatureAt(creatureId) : nullptr;
    if (!creature || !creature->isPlayer() || creature->isLocalPlayer()) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(qtTrId("player_trade_select_player_to_trade"), chat::systemWhite()));
        return;
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionPlayerTradeItem>(pending->context(), creatureId));
}

} // namespace tibia::trade
