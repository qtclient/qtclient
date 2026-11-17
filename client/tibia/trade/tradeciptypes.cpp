#include "trade/tradeciptypes.h"
#include <stdexcept>
#include <tuple>
#include <client/outfit.h>
#include "creatures/creatureprotocolapply.h"

#include "container/containerciptypes.h"
#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <tibia/assets/thingtype.h>
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

QString worldTypeName(const int worldType)
{
    static constexpr const char* Names[] = { "world_pvp_open", "world_pvp_optional", "world_pvp_hardcore", "world_pvp_retro_open",
                                             "world_pvp_retro_hardcore" };
    return worldType >= 0 && worldType < 5 ? qtTrId(Names[worldType]) : qtTrId("dummy_unknown");
}

} // namespace

TCharacterTradeStorage* TCharacterTradeStorage::s_instance = nullptr;

TCharacterTradeStorage::TCharacterTradeStorage(QObject* parent)
    : cip::tibia::trade::TCharacterTradeStorage(parent)
{
    s_instance = this;
}

TCharacterTradeStorage* TCharacterTradeStorage::instance()
{
    return s_instance;
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

TNPCTradeProtocolMessageHandler* s_npcTradeLive = nullptr;

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
    s_npcTradeLive = this;
}

TNPCTradeProtocolMessageHandler::~TNPCTradeProtocolMessageHandler()
{
    if (s_npcTradeLive == this)
        s_npcTradeLive = nullptr;
}

TNPCTradeProtocolMessageHandler* TNPCTradeProtocolMessageHandler::instance()
{
    return s_npcTradeLive;
}

void TNPCTradeProtocolMessageHandler::sendClose()
{
    tibia::game::closeNpcTrade();
    if (m_storage)
        m_storage->setTraderInventory({}, {});
}

void TNPCTradeProtocolMessageHandler::sendLook(const TNPCTradeGood& good)
{
    if (good.typeId() != 0)
        tibia::game::inspectNpcTrade(good.typeId(), good.countOrSubType());
}

void TNPCTradeProtocolMessageHandler::sendBuy(const TNPCTradeGood& good, const int amount, const bool ignoreCapacity,
                                              const bool withBackpacks)
{
    if (good.typeId() != 0)
        tibia::game::buyItem(good.typeId(), good.countOrSubType(), static_cast<uint16_t>(amount), ignoreCapacity, withBackpacks);
}

void TNPCTradeProtocolMessageHandler::sendSell(const TNPCTradeGood& good, const int amount, const bool ignoreEquipped)
{
    if (good.typeId() != 0)
        tibia::game::sellItem(good.typeId(), good.countOrSubType(), static_cast<uint16_t>(amount), ignoreEquipped);
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

void TNPCTradeProtocolMessageHandler::handleCloseNPCTradeMessage()
{
    if (m_storage)
        m_storage->setTraderInventory({}, {});
    Q_EMIT requestCloseNpcTrade();
}

void TNPCTradeProtocolMessageHandler::handleDeadMessage()
{
    Q_EMIT requestCloseNpcTrade();
}

void TNPCTradeProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    TNPCTradeProtocolMessageHandler* handler = s_npcTradeLive;
    if (!handler)
        return;
    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;
    switch (opcode) {
    case 122:
        if (shadow->HasExtension(GameserverMessageNPCOffer::gameserver_message_n_p_c_offer))
            handler->handleNPCOfferMessage(shadow->GetExtension(GameserverMessageNPCOffer::gameserver_message_n_p_c_offer));
        break;
    case 123:
        if (shadow->HasExtension(GameserverMessagePlayerGoods::gameserver_message_player_goods))
            handler->handlePlayerGoodsMessage(shadow->GetExtension(GameserverMessagePlayerGoods::gameserver_message_player_goods));
        break;
    case 124:
        handler->handleCloseNPCTradeMessage();
        break;
    case 238:
        if (shadow->HasExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance))
            handler->handleResourceBalanceMessage(
                shadow->GetExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance));
        break;
    case 40:
        handler->handleDeadMessage();
        break;
    default:
        break;
    }
}

TPlayerTradeProtocolMessageHandler* TPlayerTradeProtocolMessageHandler::s_instance = nullptr;

TPlayerTradeProtocolMessageHandler::TPlayerTradeProtocolMessageHandler(TPlayerTradeStorage& storage, QObject* parent)
    : cip::tibia::trade::TPlayerTradeProtocolMessageHandler(parent)
    , m_storage(storage)
{
    s_instance = this;
}

TPlayerTradeProtocolMessageHandler::~TPlayerTradeProtocolMessageHandler()
{
    if (s_instance == this)
        s_instance = nullptr;
}

void TPlayerTradeProtocolMessageHandler::acceptTrade()
{
    tibia::game::acceptTrade();
}

void TPlayerTradeProtocolMessageHandler::rejectTrade()
{
    tibia::game::rejectTrade();
    m_storage.reset();
}

void TPlayerTradeProtocolMessageHandler::lookTrade(const int index, const int side)
{
    if (static_cast<unsigned>(side) >= 2)
        throw std::runtime_error("unhandled enum value");
    tibia::game::inspectTrade(side != 0, static_cast<uint8_t>(index));
}

void TPlayerTradeProtocolMessageHandler::handleOwnOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageOwnOffer& OwnOfferMessage)
{
    m_storage.setOwnOffer(QString::fromStdString(OwnOfferMessage.name()), offerItems(OwnOfferMessage));
    Q_EMIT requestOpenPlayerTradeWidget();
}

void TPlayerTradeProtocolMessageHandler::handleCounterOfferMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCounterOffer& CounterOfferMessage)
{
    m_storage.setCounterOffer(QString::fromStdString(CounterOfferMessage.name()), offerItems(CounterOfferMessage));
}

void TPlayerTradeProtocolMessageHandler::handleCloseTradeMessage()
{
    Q_EMIT requestClosePlayerTradeWidget();
    m_storage.reset();
}

void TPlayerTradeProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;
    switch (opcode) {
    case 125:
        if (shadow->HasExtension(GameserverMessageOwnOffer::gameserver_message_own_offer)) {
            const auto& m = shadow->GetExtension(GameserverMessageOwnOffer::gameserver_message_own_offer);
            if (s_instance)
                s_instance->handleOwnOfferMessage(m);
            tibia::game::processOwnTrade(tibia::game::formatCreatureName(m.name()), offerItems(m));
        }
        break;
    case 126:
        if (shadow->HasExtension(GameserverMessageCounterOffer::gameserver_message_counter_offer)) {
            const auto& m = shadow->GetExtension(GameserverMessageCounterOffer::gameserver_message_counter_offer);
            if (s_instance)
                s_instance->handleCounterOfferMessage(m);
            tibia::game::processCounterTrade(tibia::game::formatCreatureName(m.name()), offerItems(m));
        }
        break;
    case 127:
        if (s_instance)
            s_instance->handleCloseTradeMessage();
        tibia::game::processCloseTrade();
        break;
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    new TCharacterTradeStorage(parent);
}

void TCharacterTradeProtocolMessageHandler::handleCharacterTradeConfigurationMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCharacterTradeConfiguration& m)
{
    if (m.data_type() > TCharacterTradeStorage::Summary)
        throw std::logic_error("unhandled enum value");
    TCharacterTradeStorage* storage = TCharacterTradeStorage::instance();
    if (!storage)
        return;
    switch (m.data_type()) {
    case TCharacterTradeStorage::Conditions: {
        TCharacterTradeBasicData data;
        data.name = QString::fromStdString(m.name());
        data.level = static_cast<quint16>(m.level());
        // Promoted vocations come as the base vocation plus ten.
        data.vocation = vocationName(static_cast<int>(m.vocation() % 10), m.vocation() >= 10);
        data.worldType = worldTypeName(static_cast<int>(m.world_type()));
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
            entry.category = static_cast<int>(group.category());
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

} // namespace tibia::trade
