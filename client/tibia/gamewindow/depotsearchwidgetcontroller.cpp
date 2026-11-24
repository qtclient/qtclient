#include "depotsearchwidgetcontroller.h"

#include "app/clientoptions.h"
#include "container/containergameactionprovider.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "exaltation/exaltationstorage.h"
#include "game/gamesessionbase.h"
#include "game/currentgamesession.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/iteminfodialogcontroller.h"
#include "gamewindow/marketcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/stashdialogcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <QGuiApplication>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;

/// Mouse mapping contexts (CIP): depot and inbox slots, the stash entry,
/// the search list.
constexpr int ContainerContext = 3;
constexpr int StashContext = 8;
constexpr int DepotSearchContext = 10;
/// The "all" category in front of the list (CIP: TQmlObjectCategory 0xff).
constexpr int AllCategories = TAppearanceTypeListModelSortFilterProxy::AllCategories;
/// STASH_RETRIEVE offers at most this many of a type (CIP).
constexpr uint32_t MaximumRetrieveCount = 100;
constexpr uint32_t MaximumCumulativeRetrieveCount = 10000;
/// DEPOT_SERACH_RETRIEVE's source (CIP: 1 depot, 2 inbox).
constexpr int RetrieveFromDepot = 1;
constexpr int RetrieveFromInbox = 2;

/// The context menu of a depot or inbox slot (CIP).
constexpr A ObjectContextMenuOrder[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::OPEN_PARENT_CONTAINER, A::QUICK_LOOT, A::WRAP, A::UNWRAP, A::CONTEXT_MENU_SEPARATOR, A::MOVE_UP, A::ROTATE_OBJECT,
    A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::OPEN_MANAGE_LOOT_CONTAINER,
    A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::CONTEXT_MENU_SEPARATOR, A::STASH_OPEN_STOW_STACK_DIALOG,
    A::STASH_STOW_TYPE, A::STASH_STOW_CONTAINER_CONTENT, A::CONTEXT_MENU_SEPARATOR, A::GET_TEXT_FOR_REPORT,
};
/// The context menu of the stash entry and of a search list row (CIP).
constexpr A TypeContextMenuOrder[] = {
    A::STASH_OPEN_RETRIEVE_DIALOG, A::CONTEXT_MENU_SEPARATOR, A::OPEN_CYCLOPEDIA,          A::CONTEXT_MENU_SEPARATOR,
    A::MARKET_BROWSE,              A::CONTEXT_MENU_SEPARATOR, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST,
};

constexpr auto ControlSchemeOption = "controlSchemeIndex";

TAppearanceType* typeOf(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type && type->getId() != 0 ? type : nullptr;
}

/// The current session's forge tiers.
exaltation::TExaltationStorage* exaltationStorage()
{
    const game::TGameSessionBase* session = game::currentGameSession();
    return session ? session->exaltationStorage().get() : nullptr;
}

/// The type list rows of the cyclopedia types: one per type, or one per
/// tier the forge knows for a classified type (none when it knows none).
std::vector<TAppearanceTypeEntry> searchEntries()
{
    std::vector<TAppearanceTypeEntry> entries;
    exaltation::TExaltationStorage* exaltation = exaltationStorage();
    for (const TAppearanceTypePtr& type : tibia::appearances::TAppearancesManager::instance().getCyclopediaObjectTypes()) {
        TAppearanceTypeEntry entry = TItemInfoDialogController::cyclopediaEntry(type);
        if (entry.classification == 0) {
            entries.push_back(std::move(entry));
            continue;
        }
        if (!exaltation)
            continue;
        for (const uint8_t tier : exaltation->tiers(static_cast<uint8_t>(entry.classification))) {
            TAppearanceTypeEntry tiered = entry;
            tiered.hasTier = true;
            tiered.tier = tier;
            entries.push_back(std::move(tiered));
        }
    }
    return entries;
}

/// The classification drop-down's selection (CIP): 0 off, 1 "none", 2
/// "all", 3 the classification of row `index` - 2.
std::pair<int, int> classificationSelection(const bool enabled, const int index)
{
    if (!enabled)
        return { 0, 0 };
    int mode = 0;
    if (index == 1)
        mode = 1;
    if (index == 0)
        mode = 2;
    if (exaltation::TExaltationStorage* exaltation = exaltationStorage(); exaltation && index > 1) {
        const std::vector<uint8_t> classes = exaltation->classifications();
        if (index - 2 < static_cast<int>(classes.size()))
            return { 3, classes[static_cast<size_t>(index - 2)] };
    }
    return { mode, 0 };
}

/// The tiers the tier drop-down offers for a classification selection.
std::vector<uint8_t> selectableTiers(const std::pair<int, int>& classification)
{
    exaltation::TExaltationStorage* exaltation = exaltationStorage();
    if (!exaltation)
        return {};
    if (classification.first == 3)
        return exaltation->tiers(static_cast<uint8_t>(classification.second));
    if (classification.first == 2)
        return exaltation->allTiers();
    return {};
}

/// The tier drop-down's selection (CIP): 3 with the tier of row `index`, else 0.
std::pair<int, int> tierSelection(const bool enabled, const std::pair<int, int>& classification, const int index)
{
    if (!enabled || (classification.first != 2 && classification.first != 3))
        return { 0, 0 };
    const std::vector<uint8_t> tiers = selectableTiers(classification);
    if (index < 0 || index >= static_cast<int>(tiers.size()))
        return { 0, 0 };
    return { 3, tiers[static_cast<size_t>(index)] };
}

}

TDepotSearchWidgetController::TDepotSearchWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_searchModel([](const uint16_t typeId, const uint8_t tier) {
        const game::TGameSessionBase* session = game::currentGameSession();
        return session ? static_cast<int>(session->objectCountStorage()->count(typeId, tier)) : 0;
    })
    , m_searchProxy(&m_searchModel)
    , m_stashModel([this](const uint16_t typeId, uint8_t) {
        if (!m_detailList)
            return 0;
        const auto& counts = m_detailList->stashCounts();
        const auto it = counts.find({ typeId, 0 });
        return it == counts.end() ? 0 : static_cast<int>(it->second);
    })
    , m_stashProxy(&m_stashModel)
    , m_mapper(app::ClientOptions::load().options().value(QLatin1String(ControlSchemeOption)).toInt(0))
{
    m_categories << new TQmlObjectCategory(AllCategories, this);
    for (const int identifier : TItemInfoDialogController::categoryIdentifiers())
        m_categories << new TQmlObjectCategory(identifier, this);
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_searchProxy);
    m_stashAppearanceTypeListModel = QVariant::fromValue<QObject*>(&m_stashProxy);
    m_depotAppearanceTypeListModel = QVariant::fromValue<QObject*>(&m_depotModel);
    m_inboxAppearanceTypeListModel = QVariant::fromValue<QObject*>(&m_inboxModel);
    m_indexOfSelectedClassification = -1;
    m_indexOfSelectedTier = -1;
    m_indexOfSelectedNPC = 0;
    m_enableTierAndClassificationSelection = false;
    m_enableTierSelection = false;
    m_stashProxy.setAcceptWithoutFilter(true);

    connect(&m_searchProxy, &cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy::showLockerOnlyChanged, this,
            &TDepotSearchWidgetController::showLockerOnlyChanged);
    connect(&m_searchProxy, &cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy::showLockerOnlyChanged, this,
            &TDepotSearchWidgetController::saveWidgetOptions);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(ControlSchemeOption))
            m_mapper.setControlScheme(app::ClientOptions::load().options().value(name).toInt(0));
    });

    widget()->createItem(QStringLiteral("DepotSearchWidget.qml"));
    if (QQuickItem* item = widget()->quickItem())
        m_focus.setClickArea(item);
    connect(&m_focus, &TUiFocusManager::focusChanged, this, &TDepotSearchWidgetController::focusChanged);
}

TDepotSearchWidgetController::~TDepotSearchWidgetController() = default;

void TDepotSearchWidgetController::setGameSession(game::TGameSessionBase* session)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    m_session = session;
    m_detailList.reset();

    // The NPC drop-down: "all", then the traders.
    QStringList npcs{ qtTrId("inventory_store_inbox_filtername_all") };
    if (session) {
        for (const QString& name : traderNames())
            npcs << name;
    }
    m_availableNPCs = npcs;
    Q_EMIT availableNPCsChanged();

    for (TAppearanceInstanceListModel* model : { &m_depotModel, &m_inboxModel })
        model->setObjectCountStorage(session ? session->objectCountStorage().get() : nullptr);
    if (!session) {
        m_searchProxy.resetEntries({});
        m_depotModel.setObjects({});
        m_inboxModel.setObjects({});
    } else {
        applyWidgetOptions();
        m_searchProxy.resetEntries(searchEntries());
        m_detailList = session->depotSearchDetailListStorage();
        m_sessionConnections.push_back(connect(m_detailList.get(), &cip::tibia::container::TDepotSearchDetailListStorage::detailListChanged,
                                               this, &TDepotSearchWidgetController::onDetailListChanged));
        m_sessionConnections.push_back(connect(session->objectCountStorage().get(),
                                               &cip::tibia::container::TObjectCountStorage::objectCountsChanged, &m_searchModel,
                                               &TAppearanceTypeListModel::onObjectCountsChanged));
    }
    m_enableTierAndClassificationSelection = false;
    Q_EMIT enableTierAndClassificationSelectionChanged();
    Q_EMIT availableClassificationsChanged();
    Q_EMIT availableTiersChanged();
    onDetailListChanged();
}

void TDepotSearchWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    onWidgetOptionsChanged();
}

void TDepotSearchWidgetController::applyWidgetOptions()
{
    if (!m_options)
        return;
    const config::TDepotSearchWidgetOptions& options = m_options->depotSearchWidgetOptions();
    widget()->setInitialContentHeightAndMaximized(options.contentHeight, options.contentMaximized);
    m_searchProxy.setShowLockerOnly(options.showLockerOnly);
}

void TDepotSearchWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    config::TDepotSearchWidgetOptions options;
    options.showLockerOnly = m_searchProxy.showLockerOnly();
    options.setContentHeightAndMaximized(widget()->maximizedContentHeight(), widget()->isMaximized());
    m_options->setDepotSearchWidgetOptions(options);
}

/// CIP: the stored widget options are applied (saving is saveWidgetOptions).
void TDepotSearchWidgetController::onWidgetOptionsChanged() { applyWidgetOptions(); }

void TDepotSearchWidgetController::onDetailListChanged()
{
    std::vector<TAppearanceTypeEntry> stash;
    std::vector<container::TInventorySlot> depot;
    std::vector<container::TInventorySlot> inbox;
    if (m_session && m_detailList) {
        for (const auto& [key, count] : m_detailList->stashCounts()) {
            if (const TAppearanceTypePtr& type = tibia::game::thingType(key.first, ThingCategoryItem); type && type->getId() != 0)
                stash.push_back(TItemInfoDialogController::cyclopediaEntry(type));
        }
        depot = m_detailList->depotObjects();
        inbox = m_detailList->inboxObjects();
    }
    m_stashProxy.resetEntries(std::move(stash));
    m_depotModel.setObjects(std::move(depot));
    m_inboxModel.setObjects(std::move(inbox));
    Q_EMIT detailListDataChanged();
    if (!m_detailList || m_detailList->typeId() == 0) {
        takeFocus();
        Q_EMIT requestWidgetControlToTakeFocus();
    } else {
        releaseFocus();
    }
}

bool TDepotSearchWidgetController::showDetailList() const { return m_detailList && m_detailList->typeId() != 0; }

QString TDepotSearchWidgetController::detailListObjectName() const
{
    if (!m_detailList)
        return {};
    const TAppearanceTypePtr& type = tibia::game::thingType(m_detailList->typeId(), ThingCategoryItem);
    return type && type->getId() != 0 ? TItemInfoDialogController::cyclopediaEntry(type).name : QString();
}

uint TDepotSearchWidgetController::totalCountDepot() const { return m_detailList ? m_detailList->depotCount() : 0; }
uint TDepotSearchWidgetController::totalCountStash() const { return m_detailList ? m_detailList->stashCount() : 0; }
uint TDepotSearchWidgetController::totalCountInbox() const { return m_detailList ? m_detailList->inboxCount() : 0; }
QString TDepotSearchWidgetController::totalCountDepotString() const { return QString::number(totalCountDepot()); }
QString TDepotSearchWidgetController::totalCountStashString() const { return QString::number(totalCountStash()); }
QString TDepotSearchWidgetController::totalCountInboxString() const { return QString::number(totalCountInbox()); }

bool TDepotSearchWidgetController::isAppearanceAllowedToRetrieve() const
{
    if (!m_session || !m_detailList || m_detailList->typeId() == 0)
        return false;
    const TAppearanceType* type = typeOf(m_detailList->typeId());
    return type && !type->isNotMoveable();
}

bool TDepotSearchWidgetController::showLockerOnly() const { return m_searchProxy.showLockerOnly(); }

void TDepotSearchWidgetController::setEnableTierSelection(const bool enable)
{
    m_enableTierSelection = enable;
    Q_EMIT enableTierSelectionChanged();
}

void TDepotSearchWidgetController::setIndexOfSelectedTier(const int index)
{
    m_indexOfSelectedTier = index;
    Q_EMIT indexOfSelectedTierChanged();
}

void TDepotSearchWidgetController::setIndexOfSelectedClassification(const int index)
{
    m_indexOfSelectedClassification = index;
    Q_EMIT indexOfSelectedClassificationChanged();
}

void TDepotSearchWidgetController::setIndexOfSelectedNPC(const int index)
{
    m_indexOfSelectedNPC = index;
    Q_EMIT indexOfSelectedNPCChanged();
}

QStringList TDepotSearchWidgetController::availableClassifications() const
{
    QStringList classes;
    exaltation::TExaltationStorage* exaltation = exaltationStorage();
    if (!m_enableTierAndClassificationSelection || !m_session || !exaltation)
        return { QString() };
    classes << qtTrId("market_filter_class_all") << qtTrId("market_filter_class_none");
    for (const uint8_t classification : exaltation->classifications())
        classes << qtTrId("market_filter_class").arg(classification);
    return classes;
}

QStringList TDepotSearchWidgetController::availableTiers() const
{
    if (!m_enableTierSelection || !m_session)
        return { QString() };
    QStringList tiers;
    for (const uint8_t tier :
         selectableTiers(classificationSelection(m_enableTierAndClassificationSelection, m_indexOfSelectedClassification)))
        tiers << qtTrId("market_filter_tier").arg(tier);
    if (tiers.isEmpty())
        tiers << QString();
    return tiers;
}

// CIP TSidebarWidgetControllerWithFocusHandling: the widget's focus is its
// TUiFocusManager's (a click outside the widget gives it back).
void TDepotSearchWidgetController::takeFocus()
{
    m_focus.takeFocus();
}

void TDepotSearchWidgetController::releaseFocus()
{
    m_focus.releaseFocus();
}

void TDepotSearchWidgetController::onRequestClose()
{
    publishSimpleAction(A::DEPOT_SEARCH_CLOSED);
    TSidebarWidgetController::onRequestClose();
    if (m_session)
        m_session->objectCountStorage()->clear();
}

void TDepotSearchWidgetController::onLevelUpClicked()
{
    requestRefresh();
    if (m_detailList)
        m_detailList->clear();
}

void TDepotSearchWidgetController::requestRefresh() { publishSimpleAction(A::REQUEST_DEPOT_SEARCH); }

void TDepotSearchWidgetController::publishSimpleAction(const input::EGameAction type) { Q_EMIT publishGameAction(input::makeGameAction(type)); }

bool TDepotSearchWidgetController::categorySupportsTiers(const int category) const
{
    for (int row = 0; row < m_searchModel.size(); ++row) {
        const TAppearanceTypeEntry& entry = m_searchModel.entry(row);
        if (entry.classification == 0)
            continue;
        if (category == AllCategories || entry.marketCategory == category)
            return true;
        if (category == TAppearanceTypeListModelSortFilterProxy::AllWeaponsCategory
            && TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(entry.marketCategory))
            return true;
    }
    return false;
}

int TDepotSearchWidgetController::categoryIndex(const int identifier) const
{
    for (int index = 0; index < m_categories.size(); ++index) {
        if (const auto* category = qobject_cast<TQmlObjectCategory*>(m_categories.at(index)); category && category->categoryIdentifier() == identifier)
            return index;
    }
    return -1;
}

void TDepotSearchWidgetController::setCategory(const int category)
{
    if (!categorySupportsTiers(category)) {
        m_enableTierAndClassificationSelection = false;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        setEnableTierSelection(false);
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        setIndexOfSelectedClassification(-1);
        setIndexOfSelectedTier(-1);
    } else {
        m_enableTierAndClassificationSelection = true;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        if (m_lastCategory != category) {
            setEnableTierSelection(true);
            Q_EMIT availableClassificationsChanged();
            setIndexOfSelectedClassification(0);
            Q_EMIT availableTiersChanged();
            setIndexOfSelectedTier(0);
        }
    }
    m_lastCategory = category;
}

TAppearanceTypeFilter TDepotSearchWidgetController::currentFilter(const bool filterByLevel, const bool filterByVocation, const bool oneHand,
                                                                  const bool twoHand) const
{
    TAppearanceTypeFilter filter;
    filter.filterByLevel = filterByLevel;
    filter.filterByVocation = filterByVocation;
    filter.onlyOneHandWeapons = oneHand;
    filter.onlyTwoHandWeapons = twoHand;
    const auto classification = classificationSelection(m_enableTierAndClassificationSelection, m_indexOfSelectedClassification);
    filter.classificationMode = classification.first;
    filter.classification = classification.second;
    const auto tier = tierSelection(m_enableTierSelection, classification, m_indexOfSelectedTier);
    filter.tierMode = tier.first;
    filter.tier = tier.second;
    // The NPC drop-down: row 0 is "all", the others the NPCs that buy the type.
    if (m_indexOfSelectedNPC > 0) {
        filter.imbuementMode = 3;
        filter.imbuementValue = m_indexOfSelectedNPC;
        filter.imbuement = m_indexOfSelectedNPC < m_availableNPCs.size() ? m_availableNPCs.at(m_indexOfSelectedNPC)
                                                                          : qtTrId("inventory_store_inbox_filtername_all");
    }
    return filter;
}

void TDepotSearchWidgetController::applyFilter(const QString& nameFilter, const bool filterByLevel, const bool filterByVocation,
                                               const bool oneHand, const bool twoHand, const bool showLockerOnly)
{
    const TAppearanceTypeFilter filter = currentFilter(filterByLevel, filterByVocation, oneHand, twoHand);
    if (nameFilter.isEmpty())
        m_searchProxy.setCategoryFilter(m_lastCategory, filter, showLockerOnly);
    else
        m_searchProxy.setNameFilter(nameFilter, filter, showLockerOnly);
}

void TDepotSearchWidgetController::requestCategorySelect(const int ClassificationIdentifier, const bool FilterByLevel,
                                                        const bool FilterByVocation, const bool OnlyOneHanWeapons,
                                                        const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    setCategory(ClassificationIdentifier);
    m_nameFilter.clear();
    m_searchProxy.setCategoryFilter(ClassificationIdentifier,
                                    currentFilter(FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons), ShowLockerOnly);
}

void TDepotSearchWidgetController::requestNameSelect(const QString& NameFilter, const bool FilterByLevel, const bool FilterByVocation,
                                                    const bool OnlyOneHanWeapons, const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    m_nameFilter = NameFilter;
    m_lastCategory = -1;
    if (m_indexOfSelectedClassification < 0) {
        m_enableTierAndClassificationSelection = true;
        Q_EMIT enableTierAndClassificationSelectionChanged();
        Q_EMIT availableClassificationsChanged();
        Q_EMIT availableTiersChanged();
        setEnableTierSelection(true);
        Q_EMIT availableClassificationsChanged();
        setIndexOfSelectedClassification(0);
        Q_EMIT availableTiersChanged();
        setIndexOfSelectedTier(0);
    }
    m_searchProxy.setNameFilter(NameFilter, currentFilter(FilterByLevel, FilterByVocation, OnlyOneHanWeapons, OnlyTwoHandWeapons),
                                ShowLockerOnly);
}

void TDepotSearchWidgetController::onClassificationFilterWasSelected(const int SelectedIndex, const QString& NameFilter,
                                                                    const bool FilterByLevel, const bool FilterByVocation,
                                                                    const bool OnlyOneHandWeapons, const bool OnlyTwoHandWeapons,
                                                                    const bool ShowLockerOnly)
{
    setIndexOfSelectedClassification(SelectedIndex);
    const int mode = classificationSelection(m_enableTierAndClassificationSelection, m_indexOfSelectedClassification).first;
    const bool tiered = mode == 2 || mode == 3;
    setEnableTierSelection(tiered);
    Q_EMIT availableTiersChanged();
    setIndexOfSelectedTier(tiered ? 0 : -1);
    applyFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, ShowLockerOnly);
}

void TDepotSearchWidgetController::onTierFilterWasSelected(const int SelectedIndex, const QString& NameFilter, const bool FilterByLevel,
                                                           const bool FilterByVocation, const bool OnlyOneHandWeapons,
                                                           const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    setIndexOfSelectedTier(SelectedIndex);
    applyFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, ShowLockerOnly);
}

void TDepotSearchWidgetController::onNpcFilterWasSelected(const int SelectedIndex, const QString& NameFilter, const bool FilterByLevel,
                                                          const bool FilterByVocation, const bool OnlyOneHandWeapons,
                                                          const bool OnlyTwoHandWeapons, const bool ShowLockerOnly)
{
    setIndexOfSelectedNPC(SelectedIndex);
    // An NPC with neither category nor name searches every category.
    if (m_lastCategory == -1 && NameFilter.isEmpty()) {
        setCategory(AllCategories);
        Q_EMIT categoryListSelectionRequest(categoryIndex(AllCategories));
    }
    applyFilter(NameFilter, FilterByLevel, FilterByVocation, OnlyOneHandWeapons, OnlyTwoHandWeapons, ShowLockerOnly);
}

void TDepotSearchWidgetController::requestSearch()
{
    if (m_selectedTypeId == 0)
        return;
    auto action = std::make_shared<input::TGameActionRequestSearchTypeInDepot>(static_cast<uint16_t>(m_selectedTypeId), m_selectedTier);
    Q_EMIT publishGameAction(action);
}

void TDepotSearchWidgetController::requestItemSelect(const int AppearanceId, const int UpgradeTier)
{
    // The tier counts only for a classified type.
    m_selectedTypeId = static_cast<uint16_t>(AppearanceId);
    m_selectedTier.reset();
    if (!m_session)
        return;
    if (const TAppearanceType* type = typeOf(m_selectedTypeId); type && type->getClassification() > 0)
        m_selectedTier = static_cast<uint8_t>(UpgradeTier);
}

void TDepotSearchWidgetController::requestItemChosen(const int AppearanceId, const int UpgradeTier)
{
    requestItemSelect(AppearanceId, UpgradeTier);
    requestSearch();
}

input::TGameActionList TDepotSearchWidgetController::typeActions(const uint16_t typeId) const
{
    // The detail list's type is the searched one, with its tier.
    if (m_detailList && m_detailList->typeId() == typeId)
        return container::TContainerGameActionProvider::depotSearchTypeActions(typeId, m_detailList->tier());
    return container::TContainerGameActionProvider::typeActions(typeId);
}

void TDepotSearchWidgetController::onSearchedItemClicked(const int AppearanceId, const int, const int MouseButtonRaw,
                                                        const int KeyboardModifiersRaw)
{
    if (!m_session)
        return;
    const input::TGameActionList candidates = typeActions(static_cast<uint16_t>(AppearanceId));
    const input::IGameActionPtr action = m_mapper.map(candidates, DepotSearchContext, static_cast<Qt::MouseButton>(MouseButtonRaw), KeyboardModifiersRaw);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        return;
    }
    const input::TGameActionList entries = input::contextMenuActions(candidates, TypeContextMenuOrder);
    if (entries.empty())
        return;
    releaseFocus();
    if (TContextMenuController* menu = TContextMenuController::instance())
        menu->showGameActions(entries, DepotSearchContext, m_mapper);
}

void TDepotSearchWidgetController::requestRetrieveFromStash(const int TypeID)
{
    if (!m_detailList || !m_session)
        return;
    const auto& counts = m_detailList->stashCounts();
    const auto it = counts.find({ static_cast<uint16_t>(TypeID), 0 });
    if (it == counts.end())
        return;
    const TAppearanceType* type = typeOf(static_cast<uint16_t>(TypeID));
    const uint32_t maximum = type && type->isStackable() ? MaximumCumulativeRetrieveCount : MaximumRetrieveCount;
    auto retrieve = std::make_shared<input::TGameActionStashRetrieve>(static_cast<uint16_t>(TypeID),
                                                                      static_cast<int>(std::min(it->second, maximum)));
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionStashOpenRetrieveDialog>(retrieve));
}

void TDepotSearchWidgetController::requestRetrieveFromDepotInbox(const int StorageLocation)
{
    if (StorageLocation < Depot || StorageLocation > Inbox || !m_session || !m_detailList)
        return;
    auto action = std::make_shared<input::TGameActionDepotSearchRetrieve>(static_cast<uint16_t>(m_detailList->typeId()), m_detailList->tier(), StorageLocation == Inbox ? RetrieveFromInbox : RetrieveFromDepot);
    Q_EMIT publishGameAction(action);
}

input::TGameActionList TDepotSearchWidgetController::slotActions(const int storageLocation, const int slotId) const
{
    if (!m_detailList)
        return {};
    if (storageLocation == Stash)
        return typeActions(m_detailList->typeId());
    const std::vector<container::TInventorySlot>& objects =
        storageLocation == Inbox ? m_detailList->inboxObjects() : m_detailList->depotObjects();
    if (slotId < 0 || slotId >= static_cast<int>(objects.size()) || objects[static_cast<size_t>(slotId)].empty())
        return {};
    return container::TContainerGameActionProvider::depotSearchSlotActions(storageLocation == Inbox, slotId,
                                                                           objects[static_cast<size_t>(slotId)]);
}

void TDepotSearchWidgetController::slotClicked(const int StorageLocation, const int SlotIDRaw, const int MouseButtonRaw,
                                              const int KeyboardModifiersRaw)
{
    if (StorageLocation < Depot || StorageLocation > Inbox || !m_session)
        return;
    const int context = StorageLocation == Stash ? StashContext : ContainerContext;
    const input::TGameActionList candidates = slotActions(StorageLocation, SlotIDRaw);
    const input::IGameActionPtr action = m_mapper.map(candidates, context, static_cast<Qt::MouseButton>(MouseButtonRaw), KeyboardModifiersRaw);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        return;
    }
    input::TGameActionList entries = context == StashContext ? input::contextMenuActions(candidates, TypeContextMenuOrder)
                                                             : input::contextMenuActions(candidates, ObjectContextMenuOrder);
    if (context == ContainerContext)
        std::erase_if(entries, [](const input::IGameActionPtr& entry) { return entry && entry->gameActionType() == A::OPEN; });
    if (entries.empty())
        return;
    if (TContextMenuController* menu = TContextMenuController::instance())
        menu->showGameActions(entries, context, m_mapper);
}

void TDepotSearchWidgetController::updateHoverCursor()
{
    Cursor* cursor = Cursor::instance();
    if (m_hoveredSlot == -1 || m_hoveredStorageLocation == -1 || !m_session || !cursor)
        return;
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::TGameActionList candidates =
        m_hoveredStorageLocation == Stash ? input::TGameActionList{} : slotActions(m_hoveredStorageLocation, m_hoveredSlot);
    const input::IGameActionPtr action = m_mapper.map(candidates, ContainerContext, Qt::LeftButton, modifiers);
    cursor->setLayerShape(Cursor::ContainerNoDrop, action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default);
}

void TDepotSearchWidgetController::slotEntered(const int StorageLocation, const int SlotIDRaw)
{
    m_hoveredStorageLocation = StorageLocation;
    m_hoveredSlot = SlotIDRaw;
    if (Cursor* cursor = Cursor::instance())
        cursor->onEnterRegion(Cursor::ContainerNoDrop);
    updateHoverCursor();
}

void TDepotSearchWidgetController::slotExited(int, const int SlotIDRaw)
{
    if (m_hoveredSlot == SlotIDRaw) {
        m_hoveredSlot = -1;
        m_hoveredStorageLocation = -1;
    }
    if (Cursor* cursor = Cursor::instance())
        cursor->onExitRegion(Cursor::ContainerNoDrop);
}

void TDepotSearchWidgetController::emptySlotAreaEntered()
{
    if (Cursor* cursor = Cursor::instance())
        cursor->onEnterRegion(Cursor::ContainerNoDrop);
}

void TDepotSearchWidgetController::emptySlotAreaExited()
{
    if (Cursor* cursor = Cursor::instance())
        cursor->onExitRegion(Cursor::ContainerNoDrop);
}

void TDepotSearchWidgetController::slotStartDrag(const int StorageLocation, const int SlotIDRaw, QObject* pSource)
{
    // A depot or inbox slot drags its MOVE_OBJECT.
    if (!m_session || (StorageLocation != Depot && StorageLocation != Inbox))
        return;
    const input::TGameActionList candidates = input::filterActions(slotActions(StorageLocation, SlotIDRaw), { A::MOVE_OBJECT });
    for (const input::IGameActionPtr& candidate : candidates) {
        if (auto move = std::dynamic_pointer_cast<const input::TGameActionMoveObject>(candidate)) {
            if (TDragAndDropController* dnd = TDragAndDropController::instance())
                dnd->startDrag(move, pSource);
            return;
        }
    }
}

void TDepotSearchWidgetController::slotTargetSelected(const int StorageLocation, const int SlotIDRaw)
{
    if (!m_session || (StorageLocation != Depot && StorageLocation != Inbox))
        return;
    const input::TGameActionList candidates = input::filterActions(slotActions(StorageLocation, SlotIDRaw), { A::TARGET_SELECTED });
    if (!candidates.empty())
        Q_EMIT publishGameAction(candidates.front());
}

} // namespace tibia::gamewindow
