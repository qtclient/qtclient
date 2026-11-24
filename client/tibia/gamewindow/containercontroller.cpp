#include "containercontroller.h"

#include "app/clientoptions.h"
#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "container/containergameactionprovider.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "worldmap/worldmapgameactionprovider.h"

#include "game/gamesessionbase.h"
#include "game/currentgamesession.h"
#include <tibia/game/gamesession.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QGuiApplication>
#include <QQuickItem>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

constexpr int ContainerContext = input::TGameActionMapper::ContainerContext;

/// CIP: a container slot's context menu.
constexpr A ContextMenuOrder[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::OPEN_PARENT_CONTAINER, A::QUICK_LOOT, A::WRAP, A::UNWRAP, A::CONTEXT_MENU_SEPARATOR, A::MOVE_UP, A::ROTATE_OBJECT,
    A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::OPEN_MANAGE_LOOT_CONTAINER,
    A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::CONTEXT_MENU_SEPARATOR, A::STASH_OPEN_STOW_STACK_DIALOG,
    A::STASH_STOW_TYPE, A::STASH_STOW_CONTAINER_CONTENT, A::CONTEXT_MENU_SEPARATOR, A::GET_TEXT_FOR_REPORT,
};

/// A container slot's stack position for MOVE_UP: the server's "parent of
/// this container" slot.
constexpr uint8_t ParentContainerSlot = 254;
/// Drops on the container but on no slot.
constexpr uint8_t AnySlot = 0xff;

constexpr auto ShowExpireOption = "showExpireInContainers";
constexpr auto ShowExpireWhenUnusedOption = "showExpireWhenUnused";
constexpr auto MoveAllOption = "dragAndDropDefaultActionIsMoveAll";
constexpr auto MoveToManagedRecursiveOption = "containerMoveToManagedContainerRecursive";
constexpr auto MoveToManagedRecursiveWarningOption = "containerMoveToManagedContainerRecursiveShowWarningAgain";
constexpr auto SortBackpacksFirstOption = "containerSortBackpacksFirst";
constexpr auto SortRecursiveOption = "containerSortRecursive";
constexpr auto SortRecursiveWarningOption = "containerSortRecursiveShowWarningAgain";

/// The view filter category "others"; 0 shows all.
constexpr uint32_t OtherFilterCategory = 0xff;

/// The sort types and their label suffixes (container_sortoptions_by_%1).
constexpr std::pair<uint32_t, const char*> SortTypes[] = {
    { 0, "name_ascending" },   { 1, "name_descending" },   { 2, "weight_ascending" },     { 3, "weight_descending" },
    { 4, "expiry_ascending" }, { 5, "expiry_descending" }, { 6, "stack_size_ascending" }, { 7, "stack_size_descending" },
};

bool option(const char* name)
{
    return app::ClientOptions::load().options().value(QLatin1String(name)).toBool();
}

void toggleOption(const char* name)
{
    app::ClientOptions::store(QLatin1String(name), !option(name));
}

input::IGameActionPtr firstOfType(const input::TGameActionList& actions, const A type)
{
    for (const input::IGameActionPtr& action : actions) {
        if (action->gameActionType() == type)
            return action;
    }
    return nullptr;
}


}

TContainerController::TContainerController(const int containerId, container::TContainerProtocolMessageHandler* messages,
                                           QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_containerId(containerId)
    , m_messages(messages)
{
    m_appearanceTypeListModel = QVariant::fromValue<QObject*>(&m_model);
    readExpireOptions();

    m_propertyTimer.setSingleShot(true);
    m_propertyTimer.setInterval(0);
    // FINAL cip_gen Q_PROPERTY NOTIFY — emit the base signal (no qtclient redeclare).
    connect(&m_propertyTimer, &QTimer::timeout, this,
            &cip::tibia::gamewindow::TContainerController::containerPropertyChanged);

    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String(ShowExpireOption) || name == QLatin1String(ShowExpireWhenUnusedOption))
            readExpireOptions();
    });

    widget()->createItem(QStringLiteral("container.qml"));
    QQuickItem* item = quickItem();
    if (!item)
        return;
    connect(item, SIGNAL(slotClicked(int, int, int)), this, SLOT(onSlotClicked(int, int, int)));
    connect(item, SIGNAL(slotEntered(int)), this, SLOT(onSlotEntered(int)));
    connect(item, SIGNAL(slotExited(int)), this, SLOT(onSlotExited(int)));
    connect(item, SIGNAL(slotStartDrag(int, QObject*)), this, SLOT(onSlotStartDrag(int, QObject*)));
    connect(item, SIGNAL(slotDragDropped(int)), this, SLOT(onSlotDragDropped(int)));
    connect(item, SIGNAL(slotTargetSelected(int)), this, SLOT(onSlotTargetSelected(int)));
    connect(item, SIGNAL(previousPageClicked()), this, SLOT(onPreviousPageClicked()));
    connect(item, SIGNAL(nextPageClicked()), this, SLOT(onNextPageClicked()));
    connect(item, SIGNAL(searchButtonClicked()), this, SLOT(onSearchButtonClicked()));
}

void TContainerController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_model.setObjectCountStorage(nullptr);
    m_options = nullptr;
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    if (const auto& configuration = session->characterConfigurationManager())
        m_options = configuration->sidebarOptions();
    if (m_options)
        m_gameSessionConnections.add(connect(m_options, &config::TSidebarOptions::optionsForAllContainersChanged, this,
                                             &TContainerController::onWidgetOptionsChanged));
    m_model.setObjectCountStorage(session->objectCountStorage().get());
    onWidgetOptionsChanged();
}

void TContainerController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const config::ContainersOptions& containers = m_options->containersOptions();
    if (const auto it = containers.find(m_containerId); it != containers.end())
        widget()->setInitialContentHeightAndMaximized(it->second.contentHeight, it->second.contentMaximized);
}

QVariant TContainerController::appearanceTypeListModel() const { return m_appearanceTypeListModel; }
int TContainerController::indexOfFirstVisibleObject() const { return m_indexOfFirstVisibleObject; }
QList<int> TContainerController::tutorialMarkerIDs() const { return m_tutorialMarkerIDs; }
QString TContainerController::caption() const { return m_caption; }
uint TContainerController::numberOfPages() const { return m_numberOfPages; }
uint TContainerController::currentPage() const { return m_currentPage; }
uint TContainerController::numberOfContainerSlots() const { return m_numberOfContainerSlots; }
uint TContainerController::numberOfFilledContainerSlots() const { return m_numberOfFilledContainerSlots; }
bool TContainerController::levelUpButtonVisible() const { return m_levelUpButtonVisible; }
bool TContainerController::searchButtonVisible() const { return m_searchButtonVisible; }
bool TContainerController::configButtonVisible() const { return m_configButtonVisible; }
bool TContainerController::isUpdatable() const { return m_isUpdatable; }
bool TContainerController::hasPossiblyMorePages() const { return m_hasPossiblyMorePages; }
bool TContainerController::hasViewFilterOptions() const { return m_hasViewFilterOptions; }
bool TContainerController::isInManualSortMode() const { return m_isInManualSortMode; }
uint TContainerController::objectID() const { return m_objectID; }
uint TContainerController::objectCount() const { return m_objectCount; }
Info::ELiquidType TContainerController::liquidType() const { return m_liquidType; }
Info::EHookDirection TContainerController::hookDirection() const { return m_hookDirection; }
uint TContainerController::decoItemObjectID() const { return m_decoItemObjectID; }

QQuickItem* TContainerController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
void TContainerController::onCloseClicked() { onRequestClose(); }
void TContainerController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TContainerController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TContainerController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TContainerController::setCursorToResizeVertical(const bool Enabled) { widget()->setCursorToResizeVertical(Enabled); }
void TContainerController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

TContainerController::~TContainerController() = default;

void TContainerController::onShowConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    container::TContainerStorage* storage = container::TContainerStorage::instance();
    const container::TStoredContainer* row = stored();
    if (!menu || !storage || !row)
        return;
    menu->beginCustomMenu();
    const auto addEntry = [&](const QString& label, auto&& onTriggered) {
        TContextMenuEntryItem* item = menu->addCustomEntry(label);
        connect(item, &TContextMenuEntryItem::triggered, this, std::forward<decltype(onTriggered)>(onTriggered));
        return item;
    };
    // Picking a category stores it and asks the server for the filtered content.
    const auto setFilter = [this](const uint32_t category) {
        if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
            storage->setFilterCategory(m_containerId, category);
            if (m_messages)
                m_messages->requestUpdateContainer(m_containerId);
        }
    };

    if (row->hasFilterCategories()) {
        const uint32_t current = storage->filterCategory(m_containerId);
        TContextMenuEntryItem* all =
            addEntry(qtTrId("inventory_store_inbox_filtername_all"), [setFilter] { setFilter(0); });
        all->setCheckable(true);
        all->setChecked(current == 0);
        // The server's categories by name; "others" comes last.
        std::vector<std::pair<uint32_t, std::string>> categories(row->filterCategories().begin(),
                                                                 row->filterCategories().end());
        std::sort(categories.begin(), categories.end(),
                  [](const auto& left, const auto& right) { return left.second < right.second; });
        for (const auto& [id, name] : categories) {
            if (id == OtherFilterCategory)
                continue;
            TContextMenuEntryItem* item =
                addEntry(QString::fromStdString(name), [setFilter, id = id] { setFilter(id); });
            item->setCheckable(true);
            item->setChecked(id == current);
        }
        if (row->filterCategories().contains(OtherFilterCategory)) {
            TContextMenuEntryItem* others = addEntry(qtTrId("inventory_store_inbox_filtername_others"),
                                                     [setFilter] { setFilter(OtherFilterCategory); });
            others->setCheckable(true);
            others->setChecked(current == OtherFilterCategory);
        }
    }
    if (row->hasFilterCategories() && (row->isSortable() || row->isContentMovableToManagedContainers()))
        menu->addCustomSeparator();

    if (row->isSortable()) {
        for (const auto& [sortType, name] : SortTypes) {
            const QByteArray label = QStringLiteral("container_sortoptions_by_%1").arg(QLatin1String(name)).toUtf8();
            addEntry(qtTrId(label.constData()), [this, sortType = sortType] { sortContainer(static_cast<int>(sortType)); });
        }
        menu->addCustomSeparator();
        const auto addOption = [&](const char* label, const char* name) {
            TContextMenuEntryItem* item = addEntry(qtTrId(label), [name] { toggleOption(name); });
            item->setCheckable(true);
            item->setChecked(option(name));
        };
        addOption("container_sortoptions_backpacks_first", SortBackpacksFirstOption);
        addOption("container_sortoptions_recursive", SortRecursiveOption);
        TContextMenuEntryItem* manualSort = addEntry(qtTrId("container_sortoptions_manual_sort_mode"), [] {
            if (container::TContainerStorage* storage = container::TContainerStorage::instance())
                storage->setManualSortMode(!container::TContainerStorage::manualSortMode());
        });
        manualSort->setCheckable(true);
        manualSort->setChecked(manualSortModeActive());
    }

    if (row->isContentMovableToManagedContainers()) {
        if (row->isSortable())
            menu->addCustomSeparator();
        addEntry(qtTrId("container_move_content_to_managed_containers"), [this] { moveContentToManagedContainers(); });
        TContextMenuEntryItem* recursive =
            addEntry(qtTrId("container_move_content_to_managed_containers_recursive"),
                     [] { toggleOption(MoveToManagedRecursiveOption); });
        recursive->setCheckable(true);
        recursive->setChecked(option(MoveToManagedRecursiveOption));
    }
    menu->showCustomMenu();
}

void TContainerController::showConfirmDialog(const char* caption, const char* content,
                                             std::function<void(bool showAgain)> onYes)
{
    m_confirmDialog = std::make_unique<TGenericConfirmWithDontShowAgainDialogController>(
        qtTrId(caption), qtTrId(content), qtTrId("confirmation_do_not_show_again"), true);
    connect(m_confirmDialog.get(), &TGenericConfirmWithDontShowAgainDialogController::yesClicked, this, std::move(onYes));
    m_confirmDialog->showDialog();
}

void TContainerController::sortContainer(const int sortType)
{
    const bool backpacksFirst = option(SortBackpacksFirstOption);
    const bool recursive = option(SortRecursiveOption);
    const bool warn = option(SortRecursiveWarningOption);
    if (recursive && warn) {
        showConfirmDialog("container_confirm_sort_recursive_caption", "container_confirm_sort_recursive",
                          [handler = m_messages, id = m_containerId, sortType, backpacksFirst, recursive](const bool showAgain) {
                              if (handler)
                                  handler->requestSortContainer(id, static_cast<uint32_t>(sortType), backpacksFirst, recursive);
                              app::ClientOptions::store(QLatin1String(SortRecursiveWarningOption), showAgain);
                          });
        return;
    }
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestSortContainer(m_containerId, static_cast<uint32_t>(sortType), backpacksFirst, recursive);
    app::ClientOptions::store(QLatin1String(SortRecursiveWarningOption), warn);
}

void TContainerController::moveContentToManagedContainers()
{
    const bool recursive = option(MoveToManagedRecursiveOption);
    const bool warn = option(MoveToManagedRecursiveWarningOption);
    if (recursive && warn) {
        showConfirmDialog("container_confirm_move_content_to_managed_containers_recursive_caption",
                          "container_confirm_move_content_to_managed_containers_recursive",
                          [handler = m_messages, id = m_containerId, recursive](const bool showAgain) {
                              if (handler)
                                  handler->requestMoveContentToManagedContainers(id, recursive);
                              app::ClientOptions::store(QLatin1String(MoveToManagedRecursiveWarningOption), showAgain);
                          });
        return;
    }
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestMoveContentToManagedContainers(m_containerId, recursive);
    app::ClientOptions::store(QLatin1String(MoveToManagedRecursiveWarningOption), warn);
}

bool TContainerController::manualSortModeActive() const
{
    const container::TStoredContainer* row = stored();
    return row && row->isSortable() && container::TContainerStorage::manualSortMode();
}

void TContainerController::refreshViewOptions()
{
    const container::TStoredContainer* row = stored();
    setProperty(m_hasViewFilterOptions, row && row->hasFilterCategories());
    setProperty(m_isInManualSortMode, manualSortModeActive());
}

void TContainerController::onWidgetQuickItemDestroyed() {}

void TContainerController::saveWidgetOptions()
{
    if (!m_options)
        return;
    config::ContainersOptions containers = m_options->containersOptions();
    containers[m_containerId] = { .contentHeight = widget()->maximizedContentHeight(), .contentMaximized = widget()->isMaximized() };
    m_options->setContainersOptions(containers);
}

void TContainerController::removeThisContainersOptions()
{
    if (!m_options)
        return;
    config::ContainersOptions containers = m_options->containersOptions();
    containers.erase(m_containerId);
    m_options->setContainersOptions(containers);
}

template <class T>
void TContainerController::setProperty(T& member, const T& value)
{
    if (member == value)
        return;
    member = value;
    m_propertyTimer.start();
}

void TContainerController::setIcon(const container::TInventorySlot& slot)
{
    const uint objectId = slot.typeId;
    const uint count = slot.stackable ? std::max<uint>(1, slot.count) : 1;
    const auto liquid = static_cast<Info::ELiquidType>(slot.fluidOrSplash ? slot.fluid : 0);
    const auto hook = Info::EHookDirection::NONE;
    const uint deco = slot.decoItemId;
    if (objectId == m_objectID && count == m_objectCount && liquid == m_liquidType && hook == m_hookDirection
        && deco == m_decoItemObjectID)
        return;
    m_objectID = objectId;
    m_objectCount = count;
    m_liquidType = liquid;
    m_hookDirection = hook;
    m_decoItemObjectID = deco;
    Q_EMIT containerIconChanged();
}

void TContainerController::update(const bool resetScrollView)
{
    const game::TGameSessionBase* session = game::currentGameSession();
    const std::shared_ptr<container::TContainerRenderProvider> provider = session ? session->containerRenderProvider() : nullptr;
    const std::optional<container::TContainerProperties> properties =
        provider ? provider->containerProperties(m_containerId, resetScrollView) : std::nullopt;
    if (!properties)
        return;

    setProperty(m_indexOfFirstVisibleObject, static_cast<int>(properties->indexOfFirstVisibleObject));
    setProperty(m_tutorialMarkerIDs, properties->tutorialMarkerIDs);
    setProperty(m_caption, properties->caption);
    setProperty(m_numberOfPages, properties->numberOfPages);
    setProperty(m_currentPage, properties->currentPage);
    setProperty(m_numberOfContainerSlots, properties->numberOfContainerSlots);
    setProperty(m_numberOfFilledContainerSlots, properties->numberOfFilledContainerSlots);
    setProperty(m_levelUpButtonVisible, properties->levelUpButtonVisible);
    setProperty(m_searchButtonVisible, properties->searchButtonVisible);
    setProperty(m_configButtonVisible, properties->configButtonVisible);
    // The hovered slot's cursor region depends on isUpdatable: leave it before switching.
    if (properties->isUpdatable != m_isUpdatable && m_hoveredSlot != -1) {
        m_hoveredSlot = -1;
        if (Cursor* cursor = Cursor::instance()) {
            if (m_isUpdatable)
                cursor->onContainerExited();
            else
                cursor->onContainerNoDropExited();
        }
    }
    setProperty(m_isUpdatable, properties->isUpdatable);
    setProperty(m_hasPossiblyMorePages, properties->hasPossiblyMorePages);
    refreshViewOptions();
    setIcon(properties->containerItem);

    m_model.setMinimumRows(std::max<int>(1, static_cast<int>(properties->numberOfContainerSlots)));
    m_model.setObjects(properties->objects);

    if (properties->resetScrollView && quickItem())
        QMetaObject::invokeMethod(quickItem(), "resetScrollView");
    updateMouseCursorForCurrentSlot();
}

const container::TStoredContainer* TContainerController::stored() const
{
    return container::TContainerStorage::instance()
        ? container::TContainerStorage::instance()->containerAt(m_containerId)
        : nullptr;
}

Position TContainerController::slotPosition(const int pageSlot) const
{
    return { 0xffff, static_cast<uint16_t>(m_containerId | 0x40), static_cast<uint8_t>(pageSlot) };
}

input::TGameActionList TContainerController::slotActions(const int slotId) const
{
    const game::TGameSessionBase* session = game::currentGameSession();
    const std::shared_ptr<container::TContainerGameActionProvider> provider =
        session ? session->containerGameActionProvider() : nullptr;
    return provider ? provider->containerSlotActions(static_cast<uint8_t>(m_containerId), slotId) : input::TGameActionList{};
}

void TContainerController::onSlotClicked(const int slotId, const int mouseButton, const int keyboardModifiers)
{
    const input::TGameActionList candidates = slotActions(slotId);
    const input::IGameActionPtr action = m_mapper.map(candidates, ContainerContext, static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
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
        menu->showGameActions(entries, ContainerContext, m_mapper);
}

void TContainerController::onSlotEntered(const int slotId)
{
    m_hoveredSlot = slotId;
    if (Cursor* cursor = Cursor::instance()) {
        if (m_isUpdatable)
            cursor->onContainerEntered();
        else
            cursor->onContainerNoDropEntered();
    }
    updateMouseCursorForCurrentSlot();
}

void TContainerController::onSlotExited(const int slotId)
{
    if (m_hoveredSlot == slotId)
        m_hoveredSlot = -1;
    if (Cursor* cursor = Cursor::instance()) {
        if (m_isUpdatable)
            cursor->onContainerExited();
        else
            cursor->onContainerNoDropExited();
    }
}

void TContainerController::updateMouseCursorForCurrentSlot()
{
    Cursor* cursor = Cursor::instance();
    if (m_hoveredSlot == -1 || !cursor)
        return;
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::IGameActionPtr action = m_mapper.map(slotActions(m_hoveredSlot), ContainerContext, Qt::LeftButton, modifiers);
    cursor->setLayerShape(m_isUpdatable ? Cursor::Container : Cursor::ContainerNoDrop,
                          action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default);
}

void TContainerController::onSlotStartDrag(const int slotId, QObject* source)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd)
        return;
    if (auto move = std::dynamic_pointer_cast<const input::TGameActionMoveObject>(firstOfType(slotActions(slotId), A::MOVE_OBJECT)))
        dnd->startDrag(move, source);
}

void TContainerController::onSlotDragDropped(const int slotId)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    const container::TStoredContainer* row = stored();
    if (!dnd || !row)
        return;
    const input::TMoveObjectPtr move = dnd->takeAction();
    if (!move)
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String(MoveAllOption)).toBool(false);
    const int pageSlot = slotId >= 0 ? slotId - static_cast<int>(row->firstIndex()) : AnySlot;
    const Position destination = slotPosition(pageSlot);
    Q_EMIT publishGameAction(input::finalizeDrop(move, destination, static_cast<int>(dnd->modifiers()), moveAll));
}

void TContainerController::onSlotTargetSelected(const int slotId)
{
    if (const input::IGameActionPtr target = firstOfType(slotActions(slotId), A::TARGET_SELECTED))
        Q_EMIT publishGameAction(target);
}

void TContainerController::onPreviousPageClicked()
{
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestPreviousPageForContainer(m_containerId);
}

void TContainerController::onNextPageClicked()
{
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestNextPageForContainer(m_containerId);
}

void TContainerController::onSearchButtonClicked()
{
    Q_EMIT publishGameAction(input::makeGameAction(A::REQUEST_DEPOT_SEARCH));
}

void TContainerController::onRequestClose()
{
    removeThisContainersOptions();
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestCloseContainer(m_containerId);
    widget()->hide();
    TSidebarWidgetController::onRequestClose();
}

void TContainerController::onLevelUpClicked()
{
    if (container::TContainerProtocolMessageHandler* handler = m_messages)
        handler->requestUpContainer(m_containerId);
}

void TContainerController::readExpireOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_model.setExpireOptions(options.value(QLatin1String(ShowExpireOption)).toBool(true),
                             options.value(QLatin1String(ShowExpireWhenUnusedOption)).toBool(false));
}

void TContainerController::updateWithTimestamp(const qint64)
{
    m_model.refreshClocks();
}

}
