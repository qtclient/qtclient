#include "depotsearchwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/sidebarmanager.h"

#include <client/item.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <tibia/world/game.h>

#include <QJsonObject>

namespace tibia::gamewindow {

namespace {

QString nameOf(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return QString::number(typeId);
    const MarketData& market = type->getMarketData();
    return QString::fromStdString(type->isMarketable() && !market.name.empty() ? market.name : type->getName());
}

int marketCategoryOf(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type || type->getId() == 0)
        return 0;
    return static_cast<int>(type->getMarketData().category);
}

TAppearanceTypeEntry entryOf(const uint16_t typeId, const uint8_t tier, const bool hasTier)
{
    TAppearanceTypeEntry entry;
    entry.typeId = typeId;
    entry.tier = tier;
    entry.hasTier = hasTier || tier > 0;
    entry.name = nameOf(typeId);
    entry.marketCategory = marketCategoryOf(typeId);
    return entry;
}

/// CipSoft retrieve type: 0 depot, 1 inbox, 2 stash (matches Crystal sendRetrieveDepotSearch).
constexpr uint8_t RetrieveDepot = 0;
constexpr uint8_t RetrieveInbox = 1;
constexpr uint8_t RetrieveStash = 2;

}

TDepotSearchWidgetController::TDepotSearchWidgetController(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_sidebars(sidebars)
{
    auto counts = [this](uint16_t typeId, uint8_t tier) { return countOf(typeId, tier); };
    m_resultsModel = new TAppearanceTypeListModel(counts, this);
    m_resultsProxy = new TAppearanceTypeListModelSortFilterProxy(m_resultsModel, this);
    m_stashModel = new TAppearanceTypeListModel(counts, this);
    m_depotModel = new TAppearanceTypeListModel(counts, this);
    m_inboxModel = new TAppearanceTypeListModel(counts, this);

    m_appearanceTypeListModel = QVariant::fromValue(static_cast<QObject*>(m_resultsProxy));
    m_stashAppearanceTypeListModel = QVariant::fromValue(static_cast<QObject*>(m_stashModel));
    m_depotAppearanceTypeListModel = QVariant::fromValue(static_cast<QObject*>(m_depotModel));
    m_inboxAppearanceTypeListModel = QVariant::fromValue(static_cast<QObject*>(m_inboxModel));

    m_hooks.emplace_back("g_game", "onDepotSearchResults", [this](LuaObject*, const std::vector<std::any>& args) {
        if (const auto* items = args.empty() ? nullptr : std::any_cast<std::vector<DepotSearchResultItem>>(&args[0]))
            applyResults(*items);
        placeInSidebar();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onDepotSearchDetail", [this](LuaObject*, const std::vector<std::any>& args) {
        if (const auto* detail = args.empty() ? nullptr : std::any_cast<DepotSearchDetail>(&args[0]))
            applyDetail(*detail);
        placeInSidebar();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCloseDepotSearch", [this](LuaObject*, const std::vector<std::any>&) {
        clearDetail();
        m_resultsModel->reset({});
        m_counts.clear();
        widget()->hide();
        return std::any{};
    });

    widget()->createItem(QStringLiteral("DepotSearchWidget.qml"));
}

void TDepotSearchWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String("depotSearch")).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

QVariant TDepotSearchWidgetController::appearanceTypeListModel() const { return m_appearanceTypeListModel; }
QVariant TDepotSearchWidgetController::stashAppearanceTypeListModel() const { return m_stashAppearanceTypeListModel; }
QVariant TDepotSearchWidgetController::depotAppearanceTypeListModel() const { return m_depotAppearanceTypeListModel; }
QVariant TDepotSearchWidgetController::inboxAppearanceTypeListModel() const { return m_inboxAppearanceTypeListModel; }
QList<QObject*> TDepotSearchWidgetController::categories() const { return m_categories; }

void TDepotSearchWidgetController::saveWidgetOptions() { persistOptions(); }

void TDepotSearchWidgetController::persistOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String("depotSearch")).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String("depotSearch"), json);
}

void TDepotSearchWidgetController::placeInSidebar()
{
    if (!m_sidebars || !widget()->quickItem())
        return;
    if (widget()->parentSidebarId() == TSidebarWidget::NoSidebar)
        m_sidebars->addWidget(widget());
    widget()->show();
}

void TDepotSearchWidgetController::applyResults(const std::vector<DepotSearchResultItem>& items)
{
    m_counts.clear();
    std::vector<TAppearanceTypeEntry> entries;
    entries.reserve(items.size());
    for (const DepotSearchResultItem& item : items) {
        m_counts.insert({ item.itemId, item.tier }, item.count);
        entries.push_back(entryOf(item.itemId, item.tier, item.tier > 0));
    }
    clearDetail();
    m_resultsModel->reset(std::move(entries));
    m_resultsModel->onObjectCountsChanged();
}

void TDepotSearchWidgetController::applyDetail(const DepotSearchDetail& detail)
{
    m_detailTypeId = detail.itemId;
    m_detailTier = detail.tier;
    m_detailListObjectName = nameOf(detail.itemId);
    m_showDetailList = true;
    m_totalCountDepot = detail.depotCount;
    m_totalCountInbox = detail.inboxCount;
    m_totalCountStash = detail.hasStash ? detail.stashCount : 0;
    m_totalCountDepotString = QString::number(m_totalCountDepot);
    m_totalCountInboxString = QString::number(m_totalCountInbox);
    m_totalCountStashString = QString::number(m_totalCountStash);
    m_isAppearanceAllowedToRetrieve = m_totalCountDepot > 0 || m_totalCountInbox > 0 || m_totalCountStash > 0;

    m_counts[{ detail.itemId, detail.tier }] =
        static_cast<int>(detail.depotCount + detail.inboxCount + (detail.hasStash ? detail.stashCount : 0));

    std::vector<TAppearanceTypeEntry> depot;
    depot.reserve(detail.depotItems.size());
    for (const ItemPtr& item : detail.depotItems) {
        if (!item)
            continue;
        depot.push_back(entryOf(item->getId(), item->getTier(), item->getTier() > 0));
    }
    m_depotModel->reset(std::move(depot));

    std::vector<TAppearanceTypeEntry> inbox;
    inbox.reserve(detail.inboxItems.size());
    for (const ItemPtr& item : detail.inboxItems) {
        if (!item)
            continue;
        inbox.push_back(entryOf(item->getId(), item->getTier(), item->getTier() > 0));
    }
    m_inboxModel->reset(std::move(inbox));

    std::vector<TAppearanceTypeEntry> stash;
    if (detail.hasStash) {
        stash.push_back(entryOf(detail.stashItemId ? detail.stashItemId : detail.itemId, detail.tier, detail.tier > 0));
        m_counts[{ detail.stashItemId ? detail.stashItemId : detail.itemId, detail.tier }] = static_cast<int>(detail.stashCount);
    }
    m_stashModel->reset(std::move(stash));
    Q_EMIT detailListDataChanged();
}

void TDepotSearchWidgetController::clearDetail()
{
    m_showDetailList = false;
    m_detailListObjectName.clear();
    m_totalCountDepot = m_totalCountStash = m_totalCountInbox = 0;
    m_totalCountDepotString.clear();
    m_totalCountStashString.clear();
    m_totalCountInboxString.clear();
    m_isAppearanceAllowedToRetrieve = false;
    m_detailTypeId = 0;
    m_detailTier = 0;
    m_depotModel->reset({});
    m_inboxModel->reset({});
    m_stashModel->reset({});
    Q_EMIT detailListDataChanged();
}

int TDepotSearchWidgetController::countOf(const uint16_t typeId, const uint8_t tier) const
{
    return m_counts.value({ typeId, tier }, 0);
}

void TDepotSearchWidgetController::requestSearch() { g_game.openDepotSearch(); }
void TDepotSearchWidgetController::requestRefresh() { g_game.openDepotSearch(); }

void TDepotSearchWidgetController::requestCategorySelect(const int ClassificationIdentifier, const bool FilterByLevel,
                                                        const bool FilterByVocation, const bool OnlyOneHanWeapons,
                                                        const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    m_showLockerOnly = ShowLockerOnly;
    Q_EMIT showLockerOnlyChanged();
    TAppearanceTypeFilter filter;
    m_resultsProxy->setCategoryFilter(ClassificationIdentifier, FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons,
                                      filter, ShowLockerOnly);
}

void TDepotSearchWidgetController::requestNameSelect(const QString& NameFilter, const bool FilterByLevel, const bool FilterByVocation,
                                                    const bool OnlyOneHanWeapons, const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    m_showLockerOnly = ShowLockerOnly;
    Q_EMIT showLockerOnlyChanged();
    TAppearanceTypeFilter filter;
    m_resultsProxy->setNameFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons, filter,
                                  ShowLockerOnly);
}

void TDepotSearchWidgetController::requestItemSelect(const int AppearanceId, const int UpgradeTier)
{
    g_game.requestDepotSearchItem(static_cast<uint16_t>(AppearanceId), static_cast<uint8_t>(UpgradeTier));
}

void TDepotSearchWidgetController::requestItemChosen(const int AppearanceId, const int UpgradeTier)
{
    requestItemSelect(AppearanceId, UpgradeTier);
}

void TDepotSearchWidgetController::onSearchedItemClicked(const int AppearanceId, const int UpgradeTier, int, int)
{
    requestItemSelect(AppearanceId, UpgradeTier);
}

void TDepotSearchWidgetController::requestRetrieveFromStash(int)
{
    if (m_detailTypeId)
        g_game.retrieveDepotSearch(m_detailTypeId, m_detailTier, RetrieveStash);
}

void TDepotSearchWidgetController::requestRetrieveFromDepotInbox(const int StorageLocation)
{
    if (!m_detailTypeId)
        return;
    const uint8_t type = StorageLocation == 1 ? RetrieveInbox : RetrieveDepot;
    g_game.retrieveDepotSearch(m_detailTypeId, m_detailTier, type);
}

void TDepotSearchWidgetController::onRequestClose()
{
    g_game.closeDepotSearchRequest();
    widget()->hide();
}

void TDepotSearchWidgetController::onCloseClicked() { onRequestClose(); }

TDepotSearchWidgetManager::TDepotSearchWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                     QObject* parent)
    : QObject(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    // Eager: CipSoft hooks must listen for onDepotSearch* as soon as the session starts.
    m_controller = new TDepotSearchWidgetController(m_sidebars, m_gameWindowItem, m_engine);
}

TDepotSearchWidgetManager::~TDepotSearchWidgetManager() { delete m_controller; }

void TDepotSearchWidgetManager::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_controller)
        m_controller->setSidebarOptions(options);
}

TSidebarWidget* TDepotSearchWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller)
        m_controller = new TDepotSearchWidgetController(m_sidebars, m_gameWindowItem, m_engine);
    m_controller->setSidebarOptions(options);
    g_game.openDepotSearch();
    return m_controller->widget();
}

void TDepotSearchWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    g_game.closeDepotSearchRequest();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TDepotSearchWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

} // namespace tibia::gamewindow
