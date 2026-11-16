#include "trade/tradeciptypes.h"
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

QString itemNameOf(const uint16_t typeId)
{
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return QString::number(typeId);
    const MarketData& market = type->getMarketData();
    return QString::fromStdString(type->isMarketable() && !market.name.empty() ? market.name : type->getName());
}

QString categoryIconFor(const uint8_t category)
{
    switch (category) {
        case 1: return QStringLiteral("/images/icon-yes.png");
        case 2: return QStringLiteral("/images/icon-exclamationmark.png");
        default: return QStringLiteral("/images/icon-no.png");
    }
}

} // namespace

TCharacterTradeStorage* TCharacterTradeStorage::s_instance = nullptr;

TCharacterTradeStorage::TCharacterTradeStorage(QObject* parent)
    : cip::tibia::trade::TCharacterTradeStorage(parent)
{
    s_instance = this;

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCharacterTradeConditions", [this](GameObject*, const Args& args) {
        m_characterName = argString(args, 0);
        m_characterLevel = static_cast<int>(argInt(args, 1));
        m_vocationClientId = static_cast<uint8_t>(argInt(args, 2));
        if (const auto* outfit = args.size() > 3 ? std::any_cast<Outfit>(&args[3]) : nullptr)
            m_outfit = *outfit;
        m_worldType = static_cast<uint8_t>(argInt(args, 4));
        m_startTimestamp = static_cast<uint32_t>(argInt(args, 5));
        m_endTimestamp = static_cast<uint32_t>(argInt(args, 6));
        m_minimumBid = static_cast<uint16_t>(argInt(args, 7));
        m_conditions.clear();
        if (const auto* list = args.size() > 8 ? std::any_cast<std::vector<std::tuple<std::string, uint8_t>>>(&args[8]) : nullptr) {
            m_conditions.reserve(list->size());
            for (const auto& [text, status] : *list) {
                TCharacterTradeConditionEntry entry;
                entry.notification = QString::fromStdString(text);
                entry.status = static_cast<int>(status);
                m_conditions.push_back(std::move(entry));
            }
        }
        Q_EMIT infoUpdated(Conditions);
        return std::any{};
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCharacterTradeItems", [this](GameObject*, const Args& args) {
        const int dataType = static_cast<int>(argInt(args, 0));
        std::vector<TCharacterTradeItemEntry> rows;
        if (const auto* list = args.size() > 1 ? std::any_cast<std::vector<std::tuple<uint16_t, uint8_t, uint32_t>>>(&args[1]) : nullptr) {
            rows.reserve(list->size());
            for (const auto& [itemId, tier, count] : *list) {
                TCharacterTradeItemEntry entry;
                entry.id = static_cast<int>(itemId);
                entry.upgradeTier = static_cast<int>(tier);
                entry.amount = static_cast<int>(count);
                entry.name = itemNameOf(itemId);
                rows.push_back(std::move(entry));
            }
        }
        if (dataType == StoreItems) {
            m_storeItems = std::move(rows);
            Q_EMIT infoUpdated(StoreItems);
        } else {
            m_items = std::move(rows);
            Q_EMIT infoUpdated(Items);
        }
        return std::any{};
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCharacterTradeHighlights", [this](GameObject*, const Args& args) {
        m_highlights.clear();
        using Group = std::tuple<uint8_t, std::string, std::vector<std::tuple<uint16_t, std::string>>>;
        if (const auto* groups = args.empty() ? nullptr : std::any_cast<std::vector<Group>>(&args[0])) {
            for (const auto& [category, groupName, entries] : *groups) {
                const QString categoryLabel = QString::fromStdString(groupName);
                const QString icon = categoryIconFor(category);
                for (const auto& [id, text] : entries) {
                    TCharacterTradeHighlightEntry entry;
                    entry.highlightId = static_cast<int>(id);
                    entry.highlight = QString::fromStdString(text);
                    entry.category = categoryLabel;
                    entry.categoryIcon = icon;
                    m_highlights.push_back(std::move(entry));
                }
            }
        }
        Q_EMIT infoUpdated(Highlights);
        return std::any{};
    });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCharacterTradeSummary", [this](GameObject*, const Args& args) {
        m_summaryFee = static_cast<uint16_t>(argInt(args, 0));
        m_confirmations.clear();
        if (const auto* problems = args.size() > 1 ? std::any_cast<std::vector<std::string>>(&args[1]) : nullptr) {
            m_confirmations.reserve(problems->size());
            for (const std::string& problem : *problems) {
                TCharacterTradeConditionEntry entry;
                entry.notification = QString::fromStdString(problem);
                entry.status = 1; // No / error
                m_confirmations.push_back(std::move(entry));
            }
        }
        Q_EMIT infoUpdated(Summary);
        return std::any{};
    });

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_characterName.clear();
        m_characterLevel = 0;
        m_vocationClientId = 0;
        m_outfit = {};
        m_worldType = 0;
        m_startTimestamp = 0;
        m_endTimestamp = 0;
        m_minimumBid = 0;
        m_conditions.clear();
        m_items.clear();
        m_storeItems.clear();
        m_highlights.clear();
        m_summaryFee = 0;
        m_confirmations.clear();
    });
}

TCharacterTradeStorage* TCharacterTradeStorage::instance()
{
    return s_instance;
}

bool TCharacterTradeStorage::hasUnfulfilledConditions() const
{
    for (const TCharacterTradeConditionEntry& entry : m_conditions) {
        if (entry.status != 0)
            return true;
    }
    return false;
}

bool TCharacterTradeStorage::hasConfirmationError() const
{
    return !m_confirmations.empty();
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

void TCharacterTradeProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || opcode != 128
        || !shadow->HasExtension(GameserverMessageCharacterTradeConfiguration::gameserver_message_character_trade_configuration))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageCharacterTradeConfiguration::gameserver_message_character_trade_configuration);
    switch (m.data_type()) {
    case 0: {
        std::vector<std::tuple<std::string, uint8_t>> conditions;
        for (const auto& c : m.conditions())
            conditions.emplace_back(c.text(), static_cast<uint8_t>(c.fulfilled() ? 1 : 0));
        tibia::game::notify("onCharacterTradeConditions", m.name(), static_cast<uint16_t>(m.level()), static_cast<uint8_t>(m.vocation()),
                            creatures::outfitFromProtocol(m.outfit(), nullptr), static_cast<uint8_t>(m.world_type()), m.start_time(),
                            m.end_time(), static_cast<uint16_t>(m.minimum_bid()), conditions);
        break;
    }
    case 1:
    case 2: {
        std::vector<std::tuple<uint16_t, uint8_t, uint32_t>> items;
        for (const auto& i : m.items())
            items.emplace_back(static_cast<uint16_t>(i.object().object_id()), static_cast<uint8_t>(i.object().tier()), i.count());
        tibia::game::notify("onCharacterTradeItems", static_cast<uint8_t>(m.data_type()), items);
        break;
    }
    case 3: {
        std::vector<std::tuple<uint8_t, std::string, std::vector<std::tuple<uint16_t, std::string>>>> groups;
        for (const auto& g : m.groups()) {
            std::vector<std::tuple<uint16_t, std::string>> entries;
            for (const auto& e : g.entries())
                entries.emplace_back(static_cast<uint16_t>(e.entry_id()), e.text());
            groups.emplace_back(static_cast<uint8_t>(g.category()), g.name(), entries);
        }
        tibia::game::notify("onCharacterTradeHighlights", groups);
        break;
    }
    case 4:
        tibia::game::notify("onCharacterTradeSummary", static_cast<uint16_t>(m.fee()),
                            std::vector<std::string>(m.problems().begin(), m.problems().end()));
        break;
    default:
        break;
    }
}

} // namespace tibia::trade
