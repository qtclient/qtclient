#include "npctradecontroller.h"
#include "game/gameactionrouter.h"
#include "game/gamesessionbase.h"
#include "config/characterconfigurationmanager.h"
#include "config/clientoptions.h"
#include "config/sidebaroptions.h"
#include "input/gameaction.h"
#include "input/gameactioncontext.h"

#include "trade/tradeciptypes.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/messagedialogcontroller.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QJsonObject>
#include <QLocale>

#include <algorithm>
#include <array>
#include <functional>
#include <set>
#include <tibia/appearances/appearanceinstance.h>

namespace tibia::gamewindow {

namespace {

/// The name with each space-separated word capitalised.
QString capitalizedWords(const std::string& name)
{
    QString text = QString::fromStdString(name);
    bool upNext = true;
    for (QChar& ch : text) {
        if (upNext)
            ch = ch.toUpper();
        upNext = ch == u' ';
    }
    return text;
}

using trade::ENPCTradeDirection;
using trade::TNPCTradeGood;

constexpr int LargeTradeAmount = 100;
constexpr int MaxTradeAmount = 10000;
constexpr uint16_t GoldCoinTypeId = 3031;
const QLatin1StringView OptionsKey("npcTradeOptions");

QString formatNumber(const uint64_t value)
{
    return QLocale(QLocale::English).toString(static_cast<qulonglong>(value));
}

/// CIP / : the number and its shorter " k", "k",
/// " kk", "kk", ... forms as Qt length variants (U+009C separated) for
/// elided Text.
QString formatNumberVariants(const uint64_t value)
{
    QStringList variants{ formatNumber(value) };
    static constexpr std::array<std::pair<uint64_t, const char*>, 3> Units{ { { 1000, "k" }, { 1000000, "kk" }, { 1000000000, "kkk" } } };
    for (const auto& [divisor, unit] : Units) {
        if (value < divisor)
            break;
        const QString shortened = formatNumber(value / divisor);
        variants << shortened + QLatin1Char(' ') + QLatin1StringView(unit) << shortened + QLatin1StringView(unit);
    }
    return variants.join(QChar(0x9c));
}

}

TNPCTradeOptions TNPCTradeOptions::load()
{
    const QJsonObject o = app::ClientOptions::load().options().value(OptionsKey).toObject();
    TNPCTradeOptions options;
    options.sellEquipped = o.value(QLatin1String("sellEquipped")).toBool();
    options.ignoreCapacity = o.value(QLatin1String("ignoreCapacity")).toBool();
    options.buyWithBackpacks = o.value(QLatin1String("buyWithBackpacks")).toBool();
    options.showNameFilter = o.value(QLatin1String("showNameFilter")).toBool();
    options.allowLargeTradesWithoutWarning = o.value(QLatin1String("allowLargeTradesWithoutWarning")).toBool();
    options.buySortOrder = o.value(QLatin1String("buySortOrder")).toInt();
    options.sellSortOrder = o.value(QLatin1String("sellSortOrder")).toInt();
    return options;
}

void TNPCTradeOptions::store() const
{
    app::ClientOptions::store(OptionsKey, QJsonObject{
                                              { QLatin1String("sellEquipped"), sellEquipped },
                                              { QLatin1String("ignoreCapacity"), ignoreCapacity },
                                              { QLatin1String("buyWithBackpacks"), buyWithBackpacks },
                                              { QLatin1String("showNameFilter"), showNameFilter },
                                              { QLatin1String("allowLargeTradesWithoutWarning"), allowLargeTradesWithoutWarning },
                                              { QLatin1String("buySortOrder"), buySortOrder },
                                              { QLatin1String("sellSortOrder"), sellSortOrder },
                                          });
}

int TTraderInventoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TTraderInventoryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const TTraderInventoryEntry& entry = m_entries[index.row()];
    switch (role) {
    case AppearanceID:
        return entry.typeId;
    case LiquidType:
        return entry.liquidType;
    case HookDirection:
        return entry.hookDirection;
    case CanBeTraded:
        return entry.maximumTradeAmount != 0;
    case MaximumTradeAmount:
        return entry.maximumTradeAmount;
    case Name:
        return entry.name;
    case PriceAndWeight:
        if (entry.weight == 0)
            return qtTrId("npc_trade_item_price").arg(entry.price);
        return qtTrId("npc_trade_item_price_and_weight").arg(entry.price).arg(QString::number(entry.weight / 100.0, 'f', 2));
    case TradePrice:
        return entry.price;
    case Weight:
        return entry.weight;
    default:
        return {};
    }
}

QHash<int, QByteArray> TTraderInventoryModel::roleNames() const
{
    return {
        { AppearanceID, "appearanceID" },
        { LiquidType, "liquidType" },
        { HookDirection, "hookDirection" },
        { CanBeTraded, "canBeTraded" },
        { MaximumTradeAmount, "maximumTradeAmount" },
        { Name, "name" },
        { PriceAndWeight, "priceAndWeight" },
        { TradePrice, "tradePrice" },
        { Weight, "weight" },
    };
}

const std::vector<TTraderInventoryEntry>& TTraderInventoryModel::entries() const
{
    return m_entries;
}

void TTraderInventoryModel::reset(std::vector<TTraderInventoryEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TTraderInventorySortFilterProxyModel::TTraderInventorySortFilterProxyModel(const TNPCTradeOptions* options, QObject* parent)
    : cip::TTraderInventorySortFilterProxyModel(parent)
    , m_options(options)
{
    m_appearanceIDEnumValue = TTraderInventoryModel::AppearanceID;
    m_liquidTypeEnumValue = TTraderInventoryModel::LiquidType;
    setDynamicSortFilter(true);
    sort(0);
}

int TTraderInventorySortFilterProxyModel::appearanceIDEnumValue() const
{
    return m_appearanceIDEnumValue;
}

int TTraderInventorySortFilterProxyModel::liquidTypeEnumValue() const
{
    return m_liquidTypeEnumValue;
}

void TTraderInventorySortFilterProxyModel::setDirection(const ENPCTradeDirection direction)
{
    if (direction == m_direction)
        return;
    m_direction = direction;
    invalidate();
    Q_EMIT filterOrSortingChanged();
}

void TTraderInventorySortFilterProxyModel::setNameFilter(const QString& filter)
{
    if (filter == m_nameFilter)
        return;
    beginFilterChange();
    m_nameFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterOrSortingChanged();
}

void TTraderInventorySortFilterProxyModel::onNPCTradeOptionsChanged()
{
    invalidate();
    Q_EMIT filterOrSortingChanged();
}

bool TTraderInventorySortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QString filter = m_nameFilter.trimmed();
    if (filter.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(TTraderInventoryModel::Name).toString().contains(filter, Qt::CaseInsensitive);
}

bool TTraderInventorySortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const int order = m_direction == ENPCTradeDirection::SELL ? m_options->sellSortOrder : m_options->buySortOrder;
    const auto value = [](const QModelIndex& index, const int role) { return index.data(role).toUInt(); };
    uint a = 0;
    uint b = 0;
    switch (order) {
    case 1:
        a = value(left, TTraderInventoryModel::TradePrice);
        b = value(right, TTraderInventoryModel::TradePrice);
        break;
    case 2:
        a = value(left, TTraderInventoryModel::Weight);
        b = value(right, TTraderInventoryModel::Weight);
        break;
    case 3:
        a = value(left, TTraderInventoryModel::MaximumTradeAmount);
        b = value(right, TTraderInventoryModel::MaximumTradeAmount);
        break;
    default:
        break;
    }
    if (a != b)
        return a < b;
    const QString leftName = left.data(TTraderInventoryModel::Name).toString().toLower();
    const QString rightName = right.data(TTraderInventoryModel::Name).toString().toLower();
    if (const int c = QString::compare(leftName, rightName, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return value(left, TTraderInventoryModel::AppearanceID) < value(right, TTraderInventoryModel::AppearanceID);
}

TNPCTradeController::TNPCTradeController(QQuickItem* DialogParentItem, QQmlEngine* Engine, QObject* parent)
    : cip::tibia::gamewindow::TNPCTradeController(parent)
    , m_options(TNPCTradeOptions::load())
    , m_proxy(&m_options)
{
    m_proxy.setSourceModel(&m_model);
    m_traderInventoryModel = &m_proxy;
    m_showNameFilter = m_options.showNameFilter;

    connect(&m_proxy, &TTraderInventorySortFilterProxyModel::filterOrSortingChanged, this, &TNPCTradeController::selectedTypeIdChanged);
    m_refreshTimer.setInterval(0);
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &TNPCTradeController::refresh);
    connect(this, &TNPCTradeController::selectedTypeIdChanged, this, &TNPCTradeController::buyOrSellPriceChanged);
    connect(this, &TNPCTradeController::selectedLiquidTypeChanged, this, &TNPCTradeController::buyOrSellPriceChanged);
    connect(this, &TNPCTradeController::amountPreselectedChanged, this, &TNPCTradeController::buyOrSellPriceChanged);

    m_confirmation = new TMessageDialogController(Engine, DialogParentItem, this);
    m_confirmation->setYesNoButtons();
    m_confirmation->setCloseOnButtonClicked(true);
    connect(m_confirmation, &TMessageDialogController::acceptClicked, this, &TNPCTradeController::onLargeTradeConfirmed);

}

void TNPCTradeController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    disconnect(m_openTradeConnection);
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_storage = session ? session->npcTradeStorage().get() : nullptr;
    // CIP attaches the trade session each time the NPC opens a trade.
    if (session)
        m_openTradeConnection = connect(session->npcTradeProtocolMessageHandler().get(), &trade::TNPCTradeProtocolMessageHandler::requestOpenNpcTrade,
                                        this, &TNPCTradeController::setTradeStorage);
    setTradeStorage();
}

void TNPCTradeController::setTradeStorage()
{
    for (const QMetaObject::Connection& connection : m_storageConnections)
        disconnect(connection);
    m_storageConnections.clear();
    if (m_storage) {
        trade::TNPCTradeStorage* storage = m_storage;
        const auto startRefresh = [this] { m_refreshTimer.start(); };
        m_storageConnections.push_back(connect(storage, &trade::TNPCTradeStorage::playerInventoryChanged, this, startRefresh));
        m_storageConnections.push_back(connect(storage, &trade::TNPCTradeStorage::traderInventoryChanged, this, startRefresh));
        m_storageConnections.push_back(connect(storage, &trade::TNPCTradeStorage::tradeCurrencyChanged, this, &TNPCTradeController::currencyChanged));
        // The player's balance (gold, currency objects, resources).
        m_storageConnections.push_back(connect(storage, &trade::TNPCTradeStorage::playerInventoryChanged, this, &TNPCTradeController::currencyBalanceChanged));
        m_storageConnections.push_back(connect(storage, &trade::TNPCTradeStorage::playerInventoryChanged, this,
                                               &TNPCTradeController::onAvailableCurrencyBalanceMightHaveChagned));
        // CIP: the sidebar options' npcTradeOptionsChanged refreshes the trade options.
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        if (config::TSidebarOptions* sidebarOptions =
                session && session->characterConfigurationManager() ? session->characterConfigurationManager()->sidebarOptions() : nullptr)
            m_storageConnections.push_back(connect(sidebarOptions, &config::TSidebarOptions::npcTradeOptionsChanged, this,
                                                   &TNPCTradeController::onNpcTradeOptionsChanged));
        Q_EMIT showNameFitlerChanged();
        m_refreshTimer.start();
        Q_EMIT currencyChanged();
        Q_EMIT currencyBalanceChanged();
    }
    m_proxy.onNPCTradeOptionsChanged();
    m_proxy.setNameFilter({});
}

TNPCTradeController::~TNPCTradeController()
{
    if (m_selectedTypeId != 0 || m_selectedLiquidType != 0) {
        m_selectedTypeId = 0;
        m_selectedLiquidType = 0;
        Q_EMIT selectedTypeIdChanged();
        Q_EMIT selectedLiquidTypeChanged();
        updateAmount(true);
    }
    if (m_confirmation && m_confirmation->dialog())
        m_confirmation->dialog()->close();
}

QString TNPCTradeController::bankBalance() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session && session->hasLocalPlayer() && m_storage ? formatNumber(m_storage->bankBalance()) : QStringLiteral("-");
}

QString TNPCTradeController::inventoryBalance() const
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    return session && session->hasLocalPlayer() && m_storage ? formatNumber(m_storage->inventoryBalance()) : QStringLiteral("-");
}

QString TNPCTradeController::totalBalance() const
{
    return formatNumberVariants(availableCurrencyAmount());
}

bool TNPCTradeController::npcIsBuying() const
{
    return !m_storage || m_storage->direction() == ENPCTradeDirection::BUY;
}

QString TNPCTradeController::buyOrSellPrice() const
{
    const TNPCTradeGood* good = selectedGood();
    const int amount = m_amountPreselected;
    if (!good || amount < 1 || amount > MaxTradeAmount)
        return formatNumberVariants(0);
    if (m_storage->direction() == ENPCTradeDirection::SELL)
        return formatNumberVariants(trade::sellPrice(*good, amount));
    return formatNumberVariants(trade::buyPrice(*good, amount, m_options.buyWithBackpacks && !m_storage->hasCurrency()));
}

int TNPCTradeController::currencyTypeId() const
{
    if (!m_storage)
        return 0;
    if (!m_storage->hasCurrency())
        return GoldCoinTypeId;
    return m_storage->currencyIsItem() ? m_storage->currencyTypeId() : 0;
}

QString TNPCTradeController::currencyName() const
{
    if (!m_storage || m_storage->currencyIsName())
        return m_storage ? m_storage->currencyName() : QString();
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, static_cast<uint16_t>(currencyTypeId()));
    if (!type)
        return {};
    // Gold is shown with every word capitalised.
    if (!m_storage->hasCurrency())
        return capitalizedWords(type->getName());
    return QString::fromStdString(type->getName());
}

bool TNPCTradeController::currencyIsGold() const
{
    return m_storage && !m_storage->hasCurrency();
}

bool TNPCTradeController::showNameFilter() const
{
    return m_options.showNameFilter;
}

QObject* TNPCTradeController::traderInventoryModel() const
{
    return m_traderInventoryModel;
}

int TNPCTradeController::selectedTypeId() const
{
    return m_selectedTypeId;
}

uint TNPCTradeController::selectedLiquidType() const
{
    return m_selectedLiquidType;
}

int TNPCTradeController::amountPreselected() const
{
    return m_amountPreselected;
}

const TNPCTradeGood* TNPCTradeController::good(const int typeId, const uint liquidType) const
{
    for (const TNPCTradeGood& good : m_goods) {
        if (good.typeId() != typeId)
            continue;
        if (good.appearance.fluidOrSplash && good.liquidType() != liquidType)
            continue;
        return &good;
    }
    return nullptr;
}

const TNPCTradeGood* TNPCTradeController::selectedGood() const
{
    return good(m_selectedTypeId, m_selectedLiquidType);
}

int TNPCTradeController::maximumAmount(const TNPCTradeGood* good) const
{
    if (!good || !m_storage)
        return 0;
    if (m_storage->direction() == ENPCTradeDirection::SELL)
        return trade::maximumSellAmount(*good, m_storage->ownedCount(*good, m_options.sellEquipped));
    return trade::maximumBuyAmount(*good, availableCurrencyAmount(), m_storage->freeCapacity(), m_options.ignoreCapacity,
                                   m_options.buyWithBackpacks && !m_storage->hasCurrency());
}

void TNPCTradeController::refresh()
{
    if (!m_storage)
        return;
    const ENPCTradeDirection direction = m_storage->direction();
    m_proxy.setDirection(direction);

    // The new rows, sorted by object type, and the object types they hold.
    m_goods = m_storage->goods(direction);
    std::vector<TTraderInventoryEntry> entries;
    entries.reserve(m_goods.size());
    std::set<uint16_t> types;
    for (const TNPCTradeGood& good : m_goods) {
        entries.push_back({ good.name, direction == ENPCTradeDirection::SELL ? good.sellPrice : good.buyPrice, good.weight, maximumAmount(&good),
                            good.typeId(), good.liquidType(), good.hookDirection() });
        types.insert(good.typeId());
    }
    std::sort(entries.begin(), entries.end(),
              [](const TTraderInventoryEntry& left, const TTraderInventoryEntry& right) { return left.typeId < right.typeId; });

    std::set<uint16_t> shownTypes;
    for (const TTraderInventoryEntry& entry : m_model.entries())
        shownTypes.insert(entry.typeId);

    if (m_model.entries() != entries) {
        // Selling the same objects (amounts or prices changed): keep the
        // list where the player scrolled it.
        if (direction == ENPCTradeDirection::SELL && shownTypes == types)
            Q_EMIT requestSaveScrollPositionAndRestoreAfterTradableGoodsListUpdate();
        m_model.reset(std::move(entries));
        if (shownTypes != types)
            Q_EMIT requestPositionViewAtBeginning();
    }

    const bool selectionAvailable = good(m_selectedTypeId, m_selectedLiquidType) != nullptr;
    if (selectionAvailable || m_selectedTypeId != 0 || m_selectedLiquidType != 0) {
        if (!selectionAvailable) {
            m_selectedTypeId = 0;
            m_selectedLiquidType = 0;
        }
        Q_EMIT selectedTypeIdChanged();
        Q_EMIT selectedLiquidTypeChanged();
        updateAmount(!selectionAvailable);
    }
    Q_EMIT tradedirectionChanged();
}

uint64_t TNPCTradeController::availableCurrencyAmount() const
{
    if (!m_storage)
        return 0;
    if (m_storage->currencyIsItem()) {
        // A liquid currency object is never counted.
        const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, m_storage->currencyTypeId());
        if (type && (type->isFluidContainer() || type->isSplash()))
            return 0;
    }
    // The currency objects the player carries, the quest flag balance of a
    // named currency (never below 0), or the player's gold.
    return m_storage->money();
}

void TNPCTradeController::onAvailableCurrencyBalanceMightHaveChagned()
{
    const uint64_t money = availableCurrencyAmount();
    if (money == m_money)
        return;
    m_money = money;
    m_refreshTimer.start();
}

void TNPCTradeController::onNpcTradeOptionsChanged()
{
    // CIP: showNameFitlerChanged. Host: qtclient keeps a copy of the trade
    // options, so the copy, the proxy's filter and the list follow here.
    m_options = TNPCTradeOptions::load();
    m_showNameFilter = m_options.showNameFilter;
    Q_EMIT showNameFitlerChanged();
    m_proxy.onNPCTradeOptionsChanged();
    m_refreshTimer.start();
}

void TNPCTradeController::setOptions(const TNPCTradeOptions& options)
{
    m_options = options;
    options.store();
    onNpcTradeOptionsChanged();
}

void TNPCTradeController::updateAmount(const bool force)
{
    const TNPCTradeGood* selected = selectedGood();
    const int maximum = maximumAmount(selected);
    int amount = selected ? std::min(m_amountPreselected, maximum) : 0;
    if (m_storage && m_storage->direction() != ENPCTradeDirection::SELL) {
        if (maximum != 0 && (force || amount == 0))
            amount = 1;
    } else if (force || m_amountPreselected > maximum || (maximum != 0 && amount == 0)) {
        amount = maximum;
    }
    setAmountPreselected(amount);
}

void TNPCTradeController::switchDirection(const ENPCTradeDirection direction)
{
    if (!m_storage)
        return;
    if (m_selectedTypeId != 0 || m_selectedLiquidType != 0) {
        m_selectedTypeId = 0;
        m_selectedLiquidType = 0;
        Q_EMIT selectedTypeIdChanged();
        Q_EMIT selectedLiquidTypeChanged();
        updateAmount(true);
    }
    m_storage->setDirection(direction);
}

void TNPCTradeController::switchToBuyItems()
{
    switchDirection(ENPCTradeDirection::BUY);
}

void TNPCTradeController::switchToSellItems()
{
    switchDirection(ENPCTradeDirection::SELL);
}

void TNPCTradeController::traderObjectSelected(const int TypeId, const uint LiquidType)
{
    if (TypeId == m_selectedTypeId && LiquidType == m_selectedLiquidType)
        return;
    m_selectedTypeId = TypeId;
    m_selectedLiquidType = LiquidType;
    Q_EMIT selectedTypeIdChanged();
    Q_EMIT selectedLiquidTypeChanged();
    updateAmount(true);
}

void TNPCTradeController::contextMenuForObject(const int TypeId, const uint LiquidType)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    buildContextMenu(menu, TypeId, LiquidType);
    menu->showCustomMenu();
}

void TNPCTradeController::buildContextMenu(TContextMenuController* menu, const int typeId, const uint liquidType)
{
    if (!m_storage)
        return;
    if (const TNPCTradeGood* selected = good(typeId, liquidType)) {
        const TNPCTradeGood copy = *selected;
        connect(menu->addCustomEntry(qtTrId("context_look")), &TContextMenuEntryItem::triggered, this, [this, copy] {
            if (m_storage)
                m_storage->look(copy);
        });
        if (copy.isPickupable()) {
            connect(menu->addCustomEntry(qtTrId("context_inspect_object")), &TContextMenuEntryItem::triggered, this, [this, copy] {
                const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
                if (!session || !session->gameActionRouter())
                    return;
                session->gameActionRouter()->internalRouter()->handleGameActionInternal(std::make_shared<input::TGameActionInspectObject>(
                    std::make_shared<input::TLocationNpcTradeGameActionContext>(std::make_shared<const TNPCTradeGood>(copy))));
            });
        }
        menu->addCustomSeparator();
    }

    const bool selling = m_storage->direction() == ENPCTradeDirection::SELL;
    const int sortOrder = selling ? m_options.sellSortOrder : m_options.buySortOrder;
    const auto addOption = [&](const char* label, const bool checked, std::function<void(TNPCTradeOptions&)> change) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(label));
        entry->setCheckable(true);
        entry->setChecked(checked);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, change] {
            TNPCTradeOptions options = m_options;
            change(options);
            setOptions(options);
        });
    };
    const auto addSort = [&](const char* label, const int order) {
        addOption(label, sortOrder == order, [selling, order](TNPCTradeOptions& o) { (selling ? o.sellSortOrder : o.buySortOrder) = order; });
    };
    addSort("npc_trade_sort_name", 0);
    addSort("npc_trade_sort_price", 1);
    if (selling)
        addSort("npc_trade_sort_amount", 3);
    else
        addSort("npc_trade_sort_weight", 2);
    menu->addCustomSeparator();
    if (!selling) {
        if (!m_storage->hasCurrency())
            addOption("npc_trade_buy_backpacks", m_options.buyWithBackpacks, [](TNPCTradeOptions& o) { o.buyWithBackpacks = !o.buyWithBackpacks; });
        addOption("npc_trade_ignore_capacity", m_options.ignoreCapacity, [](TNPCTradeOptions& o) { o.ignoreCapacity = !o.ignoreCapacity; });
    } else {
        addOption("npc_trade_sell_equipped", m_options.sellEquipped, [](TNPCTradeOptions& o) { o.sellEquipped = !o.sellEquipped; });
    }
    addOption("npc_trade_show_name_filter", m_options.showNameFilter, [](TNPCTradeOptions& o) { o.showNameFilter = !o.showNameFilter; });
    addOption("npc_trade_allow_large_trades_without_warning_context_menue", m_options.allowLargeTradesWithoutWarning,
              [](TNPCTradeOptions& o) { o.allowLargeTradesWithoutWarning = !o.allowLargeTradesWithoutWarning; });
}

void TNPCTradeController::lookSelectedGoods()
{
    if (const TNPCTradeGood* selected = selectedGood(); selected && m_storage)
        m_storage->look(*selected);
}

void TNPCTradeController::buyOrSellSelectedGoods(const int Amount)
{
    if (Amount < 1 || Amount > MaxTradeAmount)
        return;
    setAmountPreselected(Amount);
    if (Amount <= LargeTradeAmount || m_options.allowLargeTradesWithoutWarning) {
        trade();
        return;
    }
    if (!m_confirmation)
        return;
    const bool selling = m_storage && m_storage->direction() == ENPCTradeDirection::SELL;
    const QString verb = qtTrId(selling ? "npc_trade_sell" : "buy").toLower();
    m_confirmation->showMessageWithCheckBox(qtTrId("warning"), qtTrId("npc_trade_allow_large_trades_without_warning").arg(verb).arg(m_amountPreselected),
                                            qtTrId("confirmation_do_not_show_again"), m_options.allowLargeTradesWithoutWarning);
}

void TNPCTradeController::onLargeTradeConfirmed(const bool DoNotShowDialogAgain)
{
    trade();
    TNPCTradeOptions options = m_options;
    options.allowLargeTradesWithoutWarning = DoNotShowDialogAgain;
    setOptions(options);
}

void TNPCTradeController::trade()
{
    const TNPCTradeGood* selected = selectedGood();
    if (!selected || !m_storage)
        return;
    const int amount = m_amountPreselected;
    if (m_storage->direction() == ENPCTradeDirection::SELL)
        m_storage->sell(*selected, amount, !m_options.sellEquipped);
    else
        m_storage->buy(*selected, amount, m_options.ignoreCapacity, m_options.buyWithBackpacks && !m_storage->hasCurrency());
}

void TNPCTradeController::onRequestCloseNpcTrade()
{
    if (m_storage)
        m_storage->close();
}

void TNPCTradeController::setAmountPreselected(const int Count)
{
    const int amount = std::min(Count, maximumAmount(selectedGood()));
    if (amount == m_amountPreselected)
        return;
    m_amountPreselected = amount;
    Q_EMIT amountPreselectedChanged();
}

void TNPCTradeController::requestFilterByName(const QString& NameFilter)
{
    m_proxy.setNameFilter(NameFilter);
}

}
