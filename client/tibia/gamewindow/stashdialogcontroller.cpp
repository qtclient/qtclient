#include "stashdialogcontroller.h"

#include "platform/platform.h"

#include "app/clientoptions.h"
#include "container/containerciptypes.h"
#include "container/containergameactionprovider.h"
#include "game/gamesessionbase.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "protocol/clientmessages.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <set>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Proxy = TAppearanceTypeListModelSortFilterProxy;
using Order = cip::tibia::qmlcomponents::enums::ESortOrder;

/// CIP: the mouse mapping context of stash slots ((.., 8, ..)).
constexpr int StashContext = 8;
/// CIP DAT_102793cf0 (filled from): a stash slot's context menu.
constexpr A ContextMenuOrder[] = {
    A::STASH_OPEN_RETRIEVE_DIALOG, A::CONTEXT_MENU_SEPARATOR, A::OPEN_CYCLOPEDIA, A::CONTEXT_MENU_SEPARATOR,
    A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST,
};

constexpr auto ControlSchemeOption = "controlSchemeIndex";
constexpr auto StashSortOrderOption = "stashSortOrder";

QList<TMessageDialogButton*> okButton()
{
    return { new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK, QKeySequence(QStringLiteral("Return"))) };
}

/// CIP : "Show <category>".
QString categoryText(const int category)
{
    return QStringLiteral("%1 %2").arg(qtTrId("show"), TQmlObjectCategory::translate(category));
}

TAppearanceType* typeOf(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type && type->getId() != 0 ? type : nullptr;
}

QString typeName(TAppearanceType* type)
{
    const auto* market = type->marketProperties();
    return market && !market->name.isEmpty() ? market->name : type->name();
}


/// What the NPCs pay for one, the best offer (CIP: the sell value).
uint32_t sellValue(TAppearanceType* type)
{
    uint32_t best = 0;
    for (const auto& npc : type->npcTradeData())
        best = std::max(best, npc.buyPrice);
    return best;
}

}

/// The NPCs the stash and depot search trader filters offer.
std::set<QString> traderNames()
{
    std::set<QString> names = tibia::appearances::TAppearancesManager::instance().creatureProductBuyerNames();
    names.insert({ QStringLiteral("Alesar"), QStringLiteral("Alexander"), QStringLiteral("Avan"), QStringLiteral("Brengus"),
             QStringLiteral("Chondur"), QStringLiteral("Flint"), QStringLiteral("Gnomission"), QStringLiteral("Haroun"),
             QStringLiteral("Kiru"), QStringLiteral("Lailene"), QStringLiteral("Mugruu"), QStringLiteral("Nah'bob"),
             QStringLiteral("Rafzan"), QStringLiteral("Rashid"), QStringLiteral("Talila"), QStringLiteral("Tamoril"),
             QStringLiteral("Tamru"), QStringLiteral("Valindara"), QStringLiteral("Yaman"), QStringLiteral("Tom") });
    return names;
}

TStashProxyModel::TStashProxyModel(TAppearanceTypeListModel* model, std::function<int(uint16_t)> counts, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_model(model)
    , m_counts(std::move(counts))
{
    setSourceModel(model);
    tibia::platform::configureNaturalCollator(m_collator);
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
    if (m_sortOrder == order)
        return;
    m_sortOrder = order;
    invalidate();
}

void TStashProxyModel::resetFilters()
{
    beginFilterChange();
    m_category = -1;
    m_name.clear();
    m_trader.clear();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
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
        TAppearanceType* type = typeOf(entry.typeId);
        if (!type)
            return false;
        const auto& npcs = type->npcTradeData();
        if (std::ranges::none_of(npcs, [this](const auto& npc) { return npc.buyPrice > 0 && npc.name == m_trader; }))
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
        TAppearanceType* type = typeOf(entry.typeId);
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
    m_info->setCloseOnButtonClicked(true);
    setCategories({});
    setTraders(traderNames());

    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(ControlSchemeOption))
            m_mapper.setControlScheme(app::ClientOptions::load().options().value(name).toInt(0));
        onStashOptionsChanged();
    });

    m_gameActions.on(A::STASH_RETRIEVE, [](const input::TGameActionStashRetrieve& action) {
        if (action.typeId() == 0 || action.amount() <= 0)
            return false;
        tibia::game::stashWithdraw(action.typeId(), static_cast<uint32_t>(action.amount()), 0);
        return true;
    });
    // The market or loot container dialog the stash made way for closed: the
    // server sends the stash again, which shows the dialog.
    m_gameActions.on(A::STASH_REQUEST_REOPEN, [](const input::IGameAction&) {
        protocol::gameclient::sendStashReopen();
        return true;
    });
    // CIP / / : the object's field, type
    // and stack position; Crystal stows the whole type for STOW_STACK and
    // the given count for STOW_ITEM.
    const auto stow = [](const input::TObjectGameActionContext& object, const uint8_t mode, const uint32_t amount) {
        if (object.objectClientId() == 0)
            return false;
        tibia::game::stashStowItem(object.position(), object.objectClientId(), amount,
                                   static_cast<uint8_t>(std::max(object.stackPosition(), 0)), mode);
        return true;
    };
    m_gameActions.on(A::STASH_STOW_TYPE, [stow](const input::TGameActionStashStowType& action) {
        return stow(action.context(), tibia::client::SUPPLY_STASH_ACTION_STOW_STACK, 0);
    });
    m_gameActions.on(A::STASH_STOW_CONTAINER_CONTENT, [stow](const input::TGameActionStashStowContainerContent& action) {
        return stow(action.context(), tibia::client::SUPPLY_STASH_ACTION_STOW_CONTAINER, 0);
    });
    m_gameActions.on(A::STASH_STOW_STACK, [stow](const input::TGameActionStashStowStack& action) {
        return action.amount() > 0
            && stow(action.context(), tibia::client::SUPPLY_STASH_ACTION_STOW_ITEM, static_cast<uint32_t>(action.amount()));
    });
    // CIP SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG → TConfirmMoveObjectToStashDialogController .
    m_confirmStow->setConfirmHandler([this, stow](bool) {
        if (m_pendingStow)
            stow(m_pendingStow->context(), tibia::client::SUPPLY_STASH_ACTION_STOW_STACK, 0);
        m_pendingStow.reset();
    });
    m_gameActions.on(A::SHOW_CONFIRM_MOVE_OBJECT_TO_STASH_DIALOG, [this, stow](const input::TGameActionShowConfirmMoveObjectToStashDialog& action) {
        if (!action.moveObject() || action.moveObject()->objectClientId() == 0)
            return true;
        m_pendingStow = action.moveObject();
        const bool ask = app::ClientOptions::load().options().value(QLatin1String("askBeforeStowContainerContent")).toBool(true);
        if (!ask) {
            stow(m_pendingStow->context(), tibia::client::SUPPLY_STASH_ACTION_STOW_STACK, 0);
            m_pendingStow.reset();
            return true;
        }
        m_confirmStow->showDialog();
        return true;
    });

    m_gameActions.on(A::MARKET_BROWSE, [this](const input::IGameAction& action) { handleGameAction(action); });
}

TStashDialogController::~TStashDialogController()
{
}

int TStashDialogController::count(const uint16_t typeId) const
{
    const uint32_t amount = m_stashStorage ? m_stashStorage->count(typeId) : 0;
    return static_cast<int>(std::min<uint32_t>(amount, INT32_MAX));
}

void TStashDialogController::onStashContentChanged()
{
    // "All weapons" is always offered; the rest are the stored types' categories.
    std::set<int> categories{ Proxy::AllWeaponsCategory };
    std::vector<TAppearanceTypeEntry> entries;
    const container::TObjectCountStorage::TCounts noCounts;
    const container::TObjectCountStorage::TCounts& counts = m_stashStorage ? m_stashStorage->counts() : noCounts;
    entries.reserve(counts.size());
    for (const auto& [key, amount] : counts) {
        const uint16_t typeId = key.first;
        TAppearanceType* type = typeOf(typeId);
        TAppearanceTypeEntry entry;
        entry.typeId = typeId;
        if (type) {
            entry.name = typeName(type);
            entry.marketCategory = type->marketCategory();
        }
        categories.insert(entry.marketCategory);
        entries.push_back(std::move(entry));
    }
    m_model.reset(std::move(entries));
    setCategories(categories);
}

void TStashDialogController::setCategories(const std::set<int>& categories)
{
    const int selected = m_categoryFilterIndex < static_cast<int>(m_categories.size()) ? m_categories[m_categoryFilterIndex] : Proxy::AllCategories;
    std::vector<int> sorted(categories.begin(), categories.end());
    std::ranges::sort(sorted, [](const int a, const int b) {
        return TQmlObjectCategory::translate(a).compare(TQmlObjectCategory::translate(b), Qt::CaseInsensitive) < 0;
    });
    sorted.insert(sorted.begin(), Proxy::AllCategories);
    m_categories = std::move(sorted);

    m_categoryFilterModel.clear();
    for (const int category : m_categories)
        m_categoryFilterModel << categoryText(category);
    Q_EMIT categoryFilterModelChanged();

    const auto it = std::ranges::find(m_categories, selected);
    m_categoryFilterIndex = it == m_categories.end() ? 0 : static_cast<int>(it - m_categories.begin());
    Q_EMIT categoryFilterIndexChanged();
}

void TStashDialogController::setTraders(const std::set<QString>& traders)
{
    const QString selected = m_traderFilterIndex < m_traders.size() ? m_traders[m_traderFilterIndex] : qtTrId("all_trader");
    QStringList sorted(traders.begin(), traders.end());
    std::sort(sorted.begin(), sorted.end());
    sorted.prepend(qtTrId("all_trader"));
    m_traders = sorted;

    m_traderFilterModel.clear();
    m_traderFilterModel << qtTrId("stash_no_trader_filter");
    for (qsizetype i = 1; i < m_traders.size(); ++i)
        m_traderFilterModel << QStringLiteral("%1 %2").arg(qtTrId("bought_by"), m_traders[i]);
    Q_EMIT traderFilterModelChanged();

    const qsizetype index = m_traders.indexOf(selected);
    m_traderFilterIndex = index < 0 ? 0 : static_cast<int>(index);
    Q_EMIT traderFilterIndexChanged();
}

void TStashDialogController::setNameFilter(const QString& name)
{
    if (m_nameFilter == name)
        return;
    m_nameFilter = name;
    m_proxy.setNameFilter(m_nameFilter);
    Q_EMIT nameFilterChanged();
}

void TStashDialogController::setCategoryFilterIndex(const int index)
{
    if (m_categoryFilterIndex == index)
        return;
    m_categoryFilterIndex = index;
    m_proxy.setCategory(index < static_cast<int>(m_categories.size()) ? m_categories[index] : Proxy::AllCategories);
    Q_EMIT categoryFilterIndexChanged();
}

void TStashDialogController::setTraderFilterIndex(const int index)
{
    if (m_traderFilterIndex == index)
        return;
    m_traderFilterIndex = index;
    // Index 0 is no trader; past the list it falls back to "all traders".
    QString trader;
    if (index > 0)
        trader = index < m_traders.size() ? m_traders[index] : qtTrId("all_trader");
    m_proxy.setTrader(trader);
    Q_EMIT traderFilterIndexChanged();
}

void TStashDialogController::setSortOrder(const int order)
{
    if (m_proxy.sortOrder() == order)
        return;
    m_proxy.setSortOrder(order);
    Q_EMIT sortOderChanged();
    persistSortOrder(order);
}

void TStashDialogController::persistSortOrder(const int order)
{
    app::ClientOptions::store(QLatin1String(StashSortOrderOption), order);
}

void TStashDialogController::applyFilter(const Filter& filter)
{
    setNameFilter(filter.name);
    if (m_categoryFilterIndex != filter.categoryIndex) {
        m_categoryFilterIndex = filter.categoryIndex;
        m_proxy.setCategory(filter.categoryIndex < static_cast<int>(m_categories.size()) ? m_categories[filter.categoryIndex]
                                                                                            : Proxy::AllCategories);
        Q_EMIT categoryFilterIndexChanged();
    }
    setTraderFilterIndex(filter.traderIndex);
    m_nameFilter = filter.name;
    m_categoryFilterIndex = filter.categoryIndex;
    m_traderFilterIndex = filter.traderIndex;
}

void TStashDialogController::saveFilter()
{
    m_savedFilter = { m_nameFilter, m_categoryFilterIndex, m_traderFilterIndex };
}

void TStashDialogController::showDialog()
{
    if (m_proxy.category() == -1)
        m_proxy.setCategory(Proxy::AllCategories);
    onStashContentChanged();
    if (dialog()->item())
        return;
    dialog()->create();
    dialog()->show();
    if (dialog()->isInStack())
        onStashOptionsChanged();
    applyFilter(m_savedFilter);
}

void TStashDialogController::closeDialog()
{
    m_info->dialog()->close();
    saveFilter();
    applyFilter({});
    if (m_stashStorage)
        m_stashStorage->clear();
    dialog()->close();
}

void TStashDialogController::resetCurrentFilters()
{
    applyFilter({});
}

void TStashDialogController::resetAllFilters()
{
    applyFilter({});
    saveFilter();
    m_proxy.resetFilters();
}

void TStashDialogController::closeButtonClicked()
{
    applyFilter({});
    closeDialog();
}

input::TGameActionList TStashDialogController::typeActions(const uint16_t typeId) const
{
    if (!m_stashStorage)
        return {};
    const auto& counts = m_stashStorage->counts();
    const auto stored = counts.find({ typeId, 0 });
    if (stored == counts.end())
        return {};
    return container::TContainerGameActionProvider::stashTypeActions(typeId, stored->second);
}

void TStashDialogController::slotClicked(const int TypeID, const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    const input::TGameActionList candidates = typeActions(static_cast<uint16_t>(TypeID));
    const input::IGameActionPtr action = m_mapper.map(candidates, StashContext, static_cast<Qt::MouseButton>(MouseButtonRaw), KeyboardModifiersRaw);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); !entries.empty())
        menu->showGameActions(entries, StashContext, m_mapper);
}

void TStashDialogController::requestOpenManageLootContainerDialog()
{
    if (dialog()->isInStack()) {
        m_reopenAfterManageLoot = true;
        closeDialog();
    }
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_MANAGE_LOOT_CONTAINER_STASH));
}

void TStashDialogController::handleGameAction(const input::IGameAction& action)
{
    // The stash makes way for the market and comes back when it closes.
    if (action.gameActionType() == A::MARKET_BROWSE && dialog()->isInStack()) {
        m_reopenAfterMarket = true;
        closeDialog();
    }
}

void TStashDialogController::onStashOptionsChanged()
{
    if (!dialog()->isInStack())
        return;
    const int order = app::ClientOptions::load().options().value(QLatin1String(StashSortOrderOption)).toInt(static_cast<int>(Order::AtoZ));
    if (m_proxy.sortOrder() != order) {
        m_proxy.setSortOrder(order);
        Q_EMIT sortOderChanged();
    }
}

void TStashDialogController::reopenStash()
{
    if (!m_reopenAfterMarket && !m_reopenAfterManageLoot)
        return;
    m_reopenAfterMarket = false;
    m_reopenAfterManageLoot = false;
    Q_EMIT publishGameAction(input::makeGameAction(A::STASH_REQUEST_REOPEN));
}

void TStashDialogController::onMarketDialogDialogClosed()
{
    reopenStash();
}

void TStashDialogController::onManageLootContainerDialogClosed()
{
    reopenStash();
}

void TStashDialogController::onRequestShowStashMessageDialog(const QString& Message)
{
    m_info->setButtons(okButton());
    m_info->showMessage(qtTrId("info"), Message);
}

}

void tibia::gamewindow::TStashDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    resetAllFilters();
    m_model.reset({});
    m_reopenAfterMarket = false;
    m_reopenAfterManageLoot = false;
    closeDialog();
    m_gameSessionConnections.disconnectAll();
    m_stashStorage.reset();
    m_containerProtocolMessageHandler.reset();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_stashStorage = session->stashStorage();
    m_containerProtocolMessageHandler = session->containerProtocolMessageHandler();
    using Handler = container::TContainerProtocolMessageHandler;
    Handler* handler = m_containerProtocolMessageHandler.get();
    m_gameSessionConnections.add(connect(handler, &Handler::requestShowOrRefreshStashDialog, this, &TStashDialogController::showDialog));
    m_gameSessionConnections.add(connect(handler, &Handler::requestCloseStashDialog, this, &TStashDialogController::closeButtonClicked));
    m_gameSessionConnections.add(
        connect(handler, &Handler::requestShowStashMessageDialog, this, &TStashDialogController::onRequestShowStashMessageDialog));
    m_gameSessionConnections.add(connect(m_stashStorage.get(), &cip::tibia::container::TObjectCountStorage::objectCountsChanged, this,
                                         &TStashDialogController::onStashContentChanged));
}
