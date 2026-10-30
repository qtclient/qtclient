#include "stashdialogcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <set>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Proxy = TAppearanceTypeListModelSortFilterProxy;
using Order = cip::tibia::qmlcomponents::enums::ESortOrder;

/// CIP: the mouse mapping context of stash slots (FUN_100e7c040(.., 8, ..)).
constexpr int StashContext = 8;
/// CIP 0x100b0f910: STASH_RETRIEVE offers at most this many of a type.
constexpr uint32_t MaximumRetrieveCount = 100;
constexpr uint32_t MaximumCumulativeRetrieveCount = 10000;

/// CIP DAT_102793cf0 (filled from 0x1022e04a4): a stash slot's context menu.
constexpr A ContextMenuOrder[] = {
    A::STASH_OPEN_RETRIEVE_DIALOG, A::CONTEXT_MENU_SEPARATOR, A::OPEN_CYCLOPEDIA, A::CONTEXT_MENU_SEPARATOR,
    A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST,
};

constexpr auto ControlSchemeOption = "controlSchemeIndex";

QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

/// CIP 0x100d41390: "Show <category>".
QString categoryText(const int category)
{
    return QStringLiteral("%1 %2").arg(qtTrId("show"), TQmlObjectCategory::translate(category));
}

ThingType* typeOf(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    return type && type->getId() != 0 ? type.get() : nullptr;
}

QString typeName(ThingType* type)
{
    const MarketData& market = type->getMarketData();
    return QString::fromStdString(type->isMarketable() && !market.name.empty() ? market.name : type->getName());
}

/// What the NPCs pay for one, the best offer (CIP: the sell value).
uint32_t sellValue(ThingType* type)
{
    uint32_t best = 0;
    for (const NPCData& npc : type->getNpcSaleData())
        best = std::max(best, npc.buyPrice);
    return best;
}

}

TStashProxyModel::TStashProxyModel(TAppearanceTypeListModel* model, std::function<int(uint16_t)> counts, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_model(model)
    , m_counts(std::move(counts))
{
    setSourceModel(model);
#if !defined(Q_OS_WASM)
    // Wasm's POSIX collator logs and ignores both of these.
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
#endif
    sort(0);
}

void TStashProxyModel::setNameFilter(const QString& name)
{
    beginFilterChange();
    m_name = name;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TStashProxyModel::setCategory(const int category)
{
    beginFilterChange();
    m_category = category;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TStashProxyModel::setTrader(const QString& trader)
{
    beginFilterChange();
    m_trader = trader;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TStashProxyModel::setSortOrder(const int order)
{
    m_sortOrder = order;
    invalidate();
}

bool TStashProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow < 0 || sourceRow >= m_model->size())
        return false;
    const TAppearanceTypeEntry& entry = m_model->entry(sourceRow);
    if (!m_name.isEmpty() && !entry.name.contains(m_name, Qt::CaseInsensitive))
        return false;
    if (m_category != Proxy::AllCategories && entry.marketCategory != m_category
        && !(m_category == Proxy::AllWeaponsCategory && Proxy::isHandWeaponCategory(entry.marketCategory)))
        return false;
    if (!m_trader.isEmpty()) {
        ThingType* type = typeOf(entry.typeId);
        if (!type)
            return false;
        const std::string trader = m_trader.toStdString();
        const auto& npcs = type->getNpcSaleData();
        if (std::ranges::none_of(npcs, [&trader](const NPCData& npc) { return npc.buyPrice > 0 && npc.name == trader; }))
            return false;
    }
    return true;
}

bool TStashProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const TAppearanceTypeEntry& a = m_model->entry(left.row());
    const TAppearanceTypeEntry& b = m_model->entry(right.row());
    const int byName = m_collator.compare(a.name, b.name);
    const auto compare = [byName](const uint64_t x, const uint64_t y, const bool largestFirst) {
        if (x != y)
            return largestFirst ? x > y : x < y;
        return byName < 0;
    };
    const auto sell = [](const TAppearanceTypeEntry& entry) -> uint64_t {
        ThingType* type = typeOf(entry.typeId);
        return type ? sellValue(type) : 0;
    };
    const auto total = [&](const TAppearanceTypeEntry& entry) { return sell(entry) * static_cast<uint64_t>(m_counts(entry.typeId)); };
    switch (static_cast<Order>(m_sortOrder)) {
    case Order::ZtoA:
        return byName > 0;
    case Order::SellExpensiveOnTop:
        return compare(sell(a), sell(b), true);
    case Order::SellExpensiveOnBottom:
        return compare(sell(a), sell(b), false);
    case Order::TotalSellExpensiveOnTop:
        return compare(total(a), total(b), true);
    case Order::TotalSellExpensiveOnBottom:
        return compare(total(a), total(b), false);
    case Order::LargestStockOnTop:
        return compare(m_counts(a.typeId), m_counts(b.typeId), true);
    case Order::LargestStockOnBottom:
        return compare(m_counts(a.typeId), m_counts(b.typeId), false);
    default:
        // The market sorts need the market's average prices, which the
        // client only learns while browsing a type in the market.
        return byName < 0;
    }
}

TStashDialogController::TStashDialogController(QObject* parent)
    : TDialogController(QStringLiteral("StashDialog.qml"), parent)
    , m_model([this](const uint16_t typeId, uint8_t) { return count(typeId); })
    , m_proxy(&m_model, [this](const uint16_t typeId) { return count(typeId); })
    , m_info(new TMessageDialogController(this))
    , m_confirmStow(new TConfirmMoveObjectToStashDialogController(this))
    , m_mapper(app::ClientOptions::load().options().value(QLatin1String(ControlSchemeOption)).toInt(0))
{
    m_stashModel = &m_proxy;
    m_sortOrder = static_cast<int>(Order::AtoZ);
    m_info->setCloseOnButtonClicked(true);
    m_categoryFilterModel = { categoryText(Proxy::AllCategories) };
    m_traderFilterModel = { qtTrId("stash_no_trader_filter") };

    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(ControlSchemeOption))
            m_mapper.setControlScheme(app::ClientOptions::load().options().value(name).toInt(0));
    });

    input::setActionHandler(A::STASH_RETRIEVE, [](const input::TGameAction& action) {
        if (action.parameter <= 0 || action.amount <= 0)
            return false;
        g_game.stashWithdraw(static_cast<uint16_t>(action.parameter), static_cast<uint32_t>(action.amount), 0);
        return true;
    });
    // CIP 0x100e64170 / 0x100e64290 / 0x100e643b0: the object's field, type
    // and stack position; Crystal stows the whole type for STOW_STACK and
    // the given count for STOW_ITEM.
    const auto stow = [](const input::TGameAction& action, const uint8_t mode, const uint32_t amount) {
        if (!action.object || !action.object->isItem())
            return false;
        const ItemPtr item = action.object->static_self_cast<Item>();
        g_game.stashStowItem(action.position, static_cast<uint16_t>(item->getId()), amount,
                             static_cast<uint8_t>(std::max(action.stackPosition, 0)), mode);
        return true;
    };
    input::setActionHandler(A::STASH_STOW_TYPE, [stow](const input::TGameAction& action) {
        return stow(action, Otc::SUPPLY_STASH_ACTION_STOW_STACK, 0);
    });
    input::setActionHandler(A::STASH_STOW_CONTAINER_CONTENT, [stow](const input::TGameAction& action) {
        return stow(action, Otc::SUPPLY_STASH_ACTION_STOW_CONTAINER, 0);
    });
    input::setActionHandler(A::STASH_STOW_STACK, [stow](const input::TGameAction& action) {
        return action.amount > 0 && stow(action, Otc::SUPPLY_STASH_ACTION_STOW_ITEM, static_cast<uint32_t>(action.amount));
    });
    // CIP SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG → TConfirmMoveObjectToStashDialogController (0x10087d320).
    m_confirmStow->setConfirmHandler([this, stow](bool) {
        if (m_pendingStow)
            stow(*m_pendingStow, Otc::SUPPLY_STASH_ACTION_STOW_STACK, 0);
        m_pendingStow.reset();
    });
    input::setActionHandler(A::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG, [this, stow](const input::TGameAction& action) {
        if (!action.object || !action.object->isItem())
            return true;
        m_pendingStow = input::mutableClone(action);
        const bool ask = app::ClientOptions::load().options().value(QLatin1String("askBeforeStowContainerContent")).toBool(true);
        if (!ask) {
            stow(*m_pendingStow, Otc::SUPPLY_STASH_ACTION_STOW_STACK, 0);
            m_pendingStow.reset();
            return true;
        }
        m_confirmStow->showDialog();
        return true;
    });

    using Args = std::vector<std::any>;
    m_hooks.emplace_back("g_game", "onSupplyStashEnter", [this](LuaObject*, const Args& args) {
        const auto* items = args.empty() ? nullptr : std::any_cast<std::vector<std::vector<uint32_t>>>(&args[0]);
        std::map<uint16_t, uint32_t> content;
        if (items) {
            for (const std::vector<uint32_t>& item : *items) {
                if (item.size() >= 2 && item[1] > 0)
                    content[static_cast<uint16_t>(item[0])] = item[1];
            }
        }
        setContent(std::move(content));
        showDialog();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onSpecialContainersAvailable", [this](LuaObject*, const Args& args) {
        if (!client::gamecore::argBool(args, 0)) {
            setContent({});
            closeDialog();
        }
        return std::any{};
    });
    // CIP 0x100a31a90: ShowMessageDialog type 0x1E is the stash's.
    m_hooks.emplace_back("g_game", "onServerError", [this](LuaObject*, const Args& args) {
        if (client::gamecore::argInt(args, 0) == 0x1e)
            onRequestShowStashMessageDialog(client::gamecore::argString(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        setContent({});
        closeDialog();
        return std::any{};
    });
}

TStashDialogController::~TStashDialogController()
{
    for (const A type : { A::STASH_RETRIEVE, A::STASH_STOW_TYPE, A::STASH_STOW_CONTAINER_CONTENT, A::STASH_STOW_STACK,
                          A::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG })
        input::setActionHandler(type, nullptr);
}

int TStashDialogController::count(const uint16_t typeId) const
{
    const auto it = m_content.find(typeId);
    return it == m_content.end() ? 0 : static_cast<int>(std::min<uint32_t>(it->second, INT32_MAX));
}

void TStashDialogController::setContent(std::map<uint16_t, uint32_t> content)
{
    m_content = std::move(content);
    onStashContentChanged();
}

void TStashDialogController::onStashContentChanged()
{
    // CIP: the categories of the stored types and "all weapons", "all" first.
    std::set<int> categories{ Proxy::AllWeaponsCategory };
    std::set<QString> traders;
    std::vector<TAppearanceTypeEntry> entries;
    entries.reserve(m_content.size());
    for (const auto& [typeId, amount] : m_content) {
        ThingType* type = typeOf(typeId);
        TAppearanceTypeEntry entry;
        entry.typeId = typeId;
        if (type) {
            entry.name = typeName(type);
            entry.marketCategory = static_cast<int>(type->getMarketData().category);
            for (const NPCData& npc : type->getNpcSaleData()) {
                if (npc.buyPrice > 0)
                    traders.insert(QString::fromStdString(npc.name));
            }
        }
        categories.insert(entry.marketCategory);
        entries.push_back(std::move(entry));
    }
    m_model.reset(std::move(entries));

    const int selectedCategory = m_categoryFilterIndex > 0 && m_categoryFilterIndex <= static_cast<int>(m_categories.size())
                                     ? m_categories[m_categoryFilterIndex - 1]
                                     : Proxy::AllCategories;
    const QString selectedTrader = m_traderFilterIndex > 0 && m_traderFilterIndex <= m_traders.size() ? m_traders[m_traderFilterIndex - 1] : QString();

    m_categories.assign(categories.begin(), categories.end());
    std::ranges::sort(m_categories, [](const int a, const int b) {
        return QString::localeAwareCompare(TQmlObjectCategory::translate(a), TQmlObjectCategory::translate(b)) < 0;
    });
    QStringList categoryNames{ categoryText(Proxy::AllCategories) };
    for (const int category : m_categories)
        categoryNames << categoryText(category);
    m_categoryFilterModel = categoryNames;
    Q_EMIT categoryFilterModelChanged();

    m_traders = QStringList(traders.begin(), traders.end());
    QStringList traderNames{ qtTrId("stash_no_trader_filter") };
    for (const QString& trader : std::as_const(m_traders))
        traderNames << QStringLiteral("%1 %2").arg(qtTrId("bought_by"), trader);
    m_traderFilterModel = traderNames;
    Q_EMIT traderFilterModelChanged();

    const auto category = std::ranges::find(m_categories, selectedCategory);
    TDialogController::setCategoryFilterIndex(category == m_categories.end() ? 0 : static_cast<int>(category - m_categories.begin()) + 1);
    TDialogController::setTraderFilterIndex(selectedTrader.isEmpty() ? 0 : static_cast<int>(m_traders.indexOf(selectedTrader)) + 1);
    applyCategory();
    applyTrader();
    m_model.onObjectCountsChanged();
    m_proxy.invalidate();
}

void TStashDialogController::applyCategory()
{
    const int index = m_categoryFilterIndex;
    m_proxy.setCategory(index > 0 && index <= static_cast<int>(m_categories.size()) ? m_categories[index - 1] : Proxy::AllCategories);
}

void TStashDialogController::applyTrader()
{
    const int index = m_traderFilterIndex;
    m_proxy.setTrader(index > 0 && index <= m_traders.size() ? m_traders[index - 1] : QString());
}

void TStashDialogController::setNameFilter(const QString& name)
{
    TDialogController::setNameFilter(name);
    m_proxy.setNameFilter(name);
}

void TStashDialogController::setCategoryFilterIndex(const int index)
{
    TDialogController::setCategoryFilterIndex(index);
    applyCategory();
}

void TStashDialogController::setTraderFilterIndex(const int index)
{
    TDialogController::setTraderFilterIndex(index);
    applyTrader();
}

void TStashDialogController::setSortOrder(const int order)
{
    TDialogController::setSortOrder(order);
    m_proxy.setSortOrder(order);
}

void TStashDialogController::showDialog()
{
    if (!dialog()->isShown())
        dialog()->show();
}

void TStashDialogController::closeDialog()
{
    dialog()->close();
}

void TStashDialogController::resetFilters()
{
    setCategoryFilterIndex(0);
    setTraderFilterIndex(0);
    setNameFilter(QString());
    setSortOrder(static_cast<int>(Order::AtoZ));
}

void TStashDialogController::closeButtonClicked()
{
    closeDialog();
}

input::TGameActionList TStashDialogController::typeActions(const uint16_t typeId) const
{
    input::TGameActionList actions;
    ThingType* type = typeOf(typeId);
    const auto stored = m_content.find(typeId);
    if (!type || stored == m_content.end())
        return actions;
    const ItemPtr object = Item::create(typeId);
    actions.push_back(input::makeAction(A::CONTEXT_MENU));
    if (type->hasCyclopediaItem())
        actions.push_back(input::makeObjectAction(A::OPEN_CYCLOPEDIA, object, {}, -1));

    auto retrieve = input::mutableClone(*input::makeAction(A::STASH_RETRIEVE));
    retrieve->parameter = typeId;
    const uint32_t maximum = type->isStackable() ? MaximumCumulativeRetrieveCount : MaximumRetrieveCount;
    retrieve->amount = static_cast<int>(std::min(stored->second, maximum));
    auto retrieveDialog = input::mutableClone(*input::makeAction(A::STASH_OPEN_RETRIEVE_DIALOG));
    retrieveDialog->inner = retrieve;
    actions.push_back(retrieveDialog);

    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (player && player->isMarketAvailable() && type->isMarketable())
        actions.push_back(input::makeObjectAction(A::MARKET_BROWSE, object, {}, -1));
    return actions;
}

void TStashDialogController::slotClicked(const int TypeID, const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    const input::TGameActionList candidates = typeActions(static_cast<uint16_t>(TypeID));
    const input::TGameActionPtr action = m_mapper.map(candidates, StashContext, static_cast<Qt::MouseButton>(MouseButtonRaw), KeyboardModifiersRaw);
    if (!action)
        return;
    if (action->type != A::CONTEXT_MENU) {
        input::publish(action);
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); !entries.empty())
        menu->showGameActions(entries, StashContext, m_mapper);
}

void TStashDialogController::requestOpenManageQuickLootDialog()
{
    closeDialog();
    input::publish(input::makeAction(A::OPEN_MANGE_LOOT_CONTAINER_STASH));
}

void TStashDialogController::onRequestShowStashMessageDialog(const QString& Message)
{
    m_info->setButtons(okButton());
    m_info->showMessage(qtTrId("info"), Message);
}

}
