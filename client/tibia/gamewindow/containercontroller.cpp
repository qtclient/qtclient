#include "containercontroller.h"

#include "app/clientoptions.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <tibia/game/gamesession.h>
#include <client/item.h>
#include <tibia/assets/thingtype.h>
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

/// CIP : the object actions container slots offer.
constexpr A SlotActionTypes[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::MOVE_OBJECT, A::TRADE_ITEM, A::TARGET_SELECTED, A::SMART_CLICK, A::OPEN_MANAGE_LOOT_CONTAINER, A::LOOT_ADD_TO_BLACKWHITELIST,
    A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::STASH_STOW_CONTAINER_CONTENT, A::STASH_OPEN_STOW_STACK_DIALOG, A::MARKET_BROWSE,
    A::GET_TEXT_FOR_REPORT,
};

/// CIP DAT_10278d408: a container slot's context menu.
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

input::TGameActionPtr firstOfType(const input::TGameActionList& actions, const A type)
{
    for (const input::TGameActionPtr& action : actions) {
        if (action->type == type)
            return action;
    }
    return nullptr;
}

/// CIP : the server's name with every word capitalised.
QString captionOf(const QString& name)
{
    QStringList words = name.split(QLatin1Char(' '));
    for (QString& word : words) {
        if (!word.isEmpty())
            word[0] = word[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

}

TContainerController::TContainerController(const int containerId, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_containerId(containerId)
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
    m_clockTimer.setInterval(1000);
    connect(&m_clockTimer, &QTimer::timeout, &m_model, &TAppearanceInstanceListModel::refreshClocks);
    m_clockTimer.start();

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

// b1/3298987ba14ddb3c
void TContainerController::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (!options)
        return;
    connect(options, &config::TSidebarOptions::optionsForAllContainersChanged, this, &TContainerController::onWidgetOptionsChanged);
    onWidgetOptionsChanged();
}

// b1/276e04dab0eadfd4
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

void TContainerController::onShowConfigurationMenu()
{
    // CIP opens view-filter / manual-sort entries; qtclient protocol has none yet.
}

void TContainerController::onWidgetQuickItemDestroyed() {}

// b1/fbca85e805e49399
void TContainerController::saveWidgetOptions()
{
    if (!m_options)
        return;
    config::ContainersOptions containers = m_options->containersOptions();
    containers[m_containerId] = { .contentHeight = widget()->maximizedContentHeight(), .contentMaximized = widget()->isMaximized() };
    m_options->setContainersOptions(containers);
}

// b1/9382eee67e64ef84
void TContainerController::removeThisContainersOptions()
{
    if (!m_options)
        return;
    config::ContainersOptions containers = m_options->containersOptions();
    if (containers.erase(m_containerId))
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

// b1/115145b3e0fe01a2
void TContainerController::update(const bool resetScrollView)
{
    const container::TStoredContainer* row = stored();
    if (!row)
        return;

    const int capacity = std::max(1, static_cast<int>(row->capacity));
    const int size = static_cast<int>(row->size);
    const int firstIndex = static_cast<int>(row->firstIndex);
    const bool hasPages = row->hasPages;
    const bool hasParent = row->hasParent;
    const bool unlocked = row->unlocked;
    const bool showSearch = row->showSearch;
    const int filled = static_cast<int>(row->items.size());
    const QString& name = row->name;
    const container::TInventorySlot& icon = row->containerItem;

    const uint pages = hasPages ? (std::max(1, size) + capacity - 1) / capacity : 0;
    setProperty(m_indexOfFirstVisibleObject, firstIndex);
    setProperty(m_caption, captionOf(name));
    setProperty(m_numberOfPages, pages);
    setProperty(m_currentPage, static_cast<uint>(firstIndex) / static_cast<uint>(capacity) + 1);
    setProperty(m_numberOfContainerSlots, static_cast<uint>(capacity));
    setProperty(m_numberOfFilledContainerSlots, static_cast<uint>(filled));
    setProperty(m_levelUpButtonVisible, hasParent);
    setProperty(m_searchButtonVisible, showSearch);
    setProperty(m_isUpdatable, unlocked);
    setIcon(icon);

    m_model.setMinimumRows(capacity);
    m_model.setObjects(row->items);

    if (resetScrollView && quickItem())
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
    const container::TStoredContainer* row = stored();
    if (!row || slotId < 0)
        return {};
    const container::TInventorySlot slot = container::TContainerStorage::instance()->slotAt(m_containerId, slotId);
    if (slot.empty())
        return {};
    const int pageSlot = slotId - static_cast<int>(row->firstIndex);
    const Position position = slotPosition(pageSlot);
    input::TGameActionList actions;
    if (row->hasParent) {
        auto openParent = input::mutableClone(*input::makeObjectAction(A::OPEN_PARENT_CONTAINER, position, pageSlot));
        openParent->objectClientId = slot.typeId;
        actions.push_back(std::move(openParent));
        ThingType* type = slot.appearance ? slot.appearance->thingType() : tibia::game::rawThingType(slot.typeId, ThingCategoryItem);
        if (!type || !type->isNotMoveable()) {
            auto moveUp = input::mutableClone(*input::makeObjectAction(A::MOVE_UP, position, pageSlot));
            moveUp->objectClientId = slot.typeId;
            moveUp->destination = Position(UINT16_MAX, static_cast<uint16_t>(0x40 | m_containerId), ParentContainerSlot);
            moveUp->moveAll = true;
            actions.push_back(std::move(moveUp));
        }
    }
    actions.push_back(input::makeAction(A::CONTEXT_MENU));
    for (input::TGameActionPtr& action :
         input::filterActions(input::TGenericObjectGameActionProvider::objectActions(slot, position, pageSlot), SlotActionTypes))
        actions.push_back(std::move(action));
    return actions;
}

void TContainerController::onSlotClicked(const int slotId, const int mouseButton, const int keyboardModifiers)
{
    const input::TGameActionList candidates = slotActions(slotId);
    const input::TGameActionPtr action =
        m_mapper.map(candidates, ContainerContext, static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
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

void TContainerController::onSlotExited(int)
{
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
    const input::TGameActionPtr action = m_mapper.map(slotActions(m_hoveredSlot), ContainerContext, Qt::LeftButton, modifiers);
    cursor->setLayerShape(Cursor::Container, action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default);
}

void TContainerController::onSlotStartDrag(const int slotId, QObject* source)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd)
        return;
    if (const input::TGameActionPtr move = firstOfType(slotActions(slotId), A::MOVE_OBJECT))
        dnd->startDrag(move, source);
}

void TContainerController::onSlotDragDropped(const int slotId)
{
    TDragAndDropController* dnd = TDragAndDropController::instance();
    const container::TStoredContainer* row = stored();
    if (!dnd || !row)
        return;
    const input::TGameActionPtr move = dnd->takeAction();
    if (!move)
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String(MoveAllOption)).toBool(false);
    const int pageSlot = slotId >= 0 ? slotId - static_cast<int>(row->firstIndex) : AnySlot;
    const Position destination = slotPosition(pageSlot);
    input::publish(input::finalizeDrop(move, destination, static_cast<int>(dnd->modifiers()), moveAll));
}

void TContainerController::onSlotTargetSelected(const int slotId)
{
    if (const input::TGameActionPtr target = firstOfType(slotActions(slotId), A::TARGET_SELECTED))
        input::publish(target);
}

// b1/f90825e4ec7ade1a
void TContainerController::onPreviousPageClicked()
{
    const container::TStoredContainer* row = stored();
    if (row && row->firstIndex > 0)
        tibia::game::seekInContainer(m_containerId, std::max(0, static_cast<int>(row->firstIndex) - static_cast<int>(row->capacity)));
}

void TContainerController::onNextPageClicked()
{
    const container::TStoredContainer* row = stored();
    if (row && static_cast<int>(row->firstIndex) + static_cast<int>(row->capacity) < static_cast<int>(row->size))
        tibia::game::seekInContainer(m_containerId, static_cast<int>(row->firstIndex) + static_cast<int>(row->capacity));
}

// b1/f5c750388a7baa56
void TContainerController::onSearchButtonClicked()
{
    input::publish(input::makeAction(A::REQUEST_DEPOT_SEARCH));
}

// b1/f8fc2ce8622a50c7
void TContainerController::onRequestClose()
{
    removeThisContainersOptions();
    tibia::game::closeById(m_containerId);
    widget()->hide();
    TSidebarWidgetController::onRequestClose();
}

// b1/a4348d2dee918c06
void TContainerController::onLevelUpClicked()
{
    tibia::game::openParentById(m_containerId);
}

void TContainerController::readExpireOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_model.setExpireOptions(options.value(QLatin1String(ShowExpireOption)).toBool(true),
                             options.value(QLatin1String(ShowExpireWhenUnusedOption)).toBool(false));
}

}
