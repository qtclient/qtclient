#include "containercontroller.h"

#include "app/clientoptions.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/draganddropcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <tibia/world/container.h>
#include <tibia/world/game.h>
#include <client/item.h>

#include <QGuiApplication>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Cursor = TMouseCursorController;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

constexpr int ContainerContext = input::TGameActionMapper::ContainerContext;

/// CIP 0x100b0f030: the object actions container slots offer.
constexpr A SlotActionTypes[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::MOVE_OBJECT, A::TRADE_ITEM, A::TARGET_SELECTED, A::SMART_CLICK, A::OPEN_MANGE_LOOT_CONTAINER, A::LOOT_ADD_TO_BLACKWHITELIST,
    A::LOOT_REMOVE_FROM_BLACKWHITELIST, A::STASH_STOW_CONTAINER_CONTENT, A::STASH_OPEN_STOW_STACK_DIALOG, A::MARKET_BROWSE,
    A::GET_TEXT_FOR_REPORT,
};

/// CIP DAT_10278d408: a container slot's context menu.
constexpr A ContextMenuOrder[] = {
    A::LOOK, A::INSPECT_OBJECT, A::OPEN_CYCLOPEDIA, A::OPEN_WEAPON_PROFICIENCY_DIALOG, A::USE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW,
    A::OPEN_PARENT_CONTAINER, A::QUICK_LOOT, A::WRAP, A::UNWRAP, A::CONTEXT_MENU_SEPARATOR, A::MOVE_UP, A::ROTATE_OBJECT,
    A::CONFIGURE_SHOW_OFF_SOCKET, A::TRADE_ITEM, A::MARKET_BROWSE, A::CONTEXT_MENU_SEPARATOR, A::OPEN_MANGE_LOOT_CONTAINER,
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

/// CIP FUN_101018f90: the server's name with every word capitalised.
QString captionOf(const std::string& name)
{
    QStringList words = QString::fromStdString(name).split(QLatin1Char(' '));
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
    connect(&m_propertyTimer, &QTimer::timeout, this, &TContainerController::containerPropertyChanged);

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

void TContainerController::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (!options)
        return;
    connect(options, &config::TSidebarOptions::optionsForAllContainersChanged, this, &TContainerController::applyWidgetOptions);
    applyWidgetOptions();
}

void TContainerController::applyWidgetOptions()
{
    if (!m_options)
        return;
    const config::ContainersOptions& containers = m_options->containersOptions();
    if (const auto it = containers.find(m_containerId); it != containers.end())
        widget()->setInitialContentHeightAndMaximized(it->second.contentHeight, it->second.contentMaximized);
}

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

void TContainerController::setIcon(const ItemPtr& item)
{
    const uint objectId = item ? item->getId() : 0;
    const uint count = item && item->isStackable() ? std::max<uint>(1, item->getCountOrSubType()) : 1;
    const auto liquid = static_cast<Info::ELiquidType>(item && (item->isFluidContainer() || item->isSplash()) ? item->getCountOrSubType() : 0);
    const uint deco = item ? static_cast<uint>(item->getDecoItemId()) : 0;
    if (objectId == m_objectID && count == m_objectCount && liquid == m_liquidType && deco == m_decoItemObjectID)
        return;
    m_objectID = objectId;
    m_objectCount = count;
    m_liquidType = liquid;
    m_decoItemObjectID = deco;
    Q_EMIT containerIconChanged();
}

void TContainerController::update(const ContainerPtr& container, const bool resetScrollView)
{
    if (!container)
        return;
    const uint capacity = std::max(1, container->getCapacity());
    const uint pages = container->hasPages() ? (std::max(1, container->getSize()) + capacity - 1) / capacity : 0;
    setProperty(m_indexOfFirstVisibleObject, static_cast<int>(container->getFirstIndex()));
    setProperty(m_caption, captionOf(container->getName()));
    setProperty(m_numberOfPages, pages);
    setProperty(m_currentPage, static_cast<uint>(container->getFirstIndex()) / capacity + 1);
    setProperty(m_numberOfContainerSlots, static_cast<uint>(container->getCapacity()));
    setProperty(m_numberOfFilledContainerSlots, static_cast<uint>(container->getItemsCount()));
    setProperty(m_levelUpButtonVisible, container->hasParent());
    setProperty(m_isUpdatable, container->isUnlocked());
    setIcon(container->getContainerItem());

    m_model.setMinimumRows(container->getCapacity());
    const auto& items = container->getItems();
    m_model.setObjects(std::vector<ItemPtr>(items.begin(), items.end()));

    if (resetScrollView && quickItem())
        QMetaObject::invokeMethod(quickItem(), "resetScrollView");
    updateMouseCursorForCurrentSlot();
}

ContainerPtr TContainerController::container() const
{
    return g_game.getContainer(m_containerId);
}

ItemPtr TContainerController::slotItem(const int slotId) const
{
    const ContainerPtr c = container();
    return c && slotId >= 0 ? c->getItem(slotId - c->getFirstIndex()) : nullptr;
}

input::TGameActionList TContainerController::slotActions(const int slotId) const
{
    const ContainerPtr c = container();
    const ItemPtr item = slotItem(slotId);
    if (!c || !item)
        return {};
    const int pageSlot = slotId - c->getFirstIndex();
    const Position position = c->getSlotPosition(pageSlot);
    input::TGameActionList actions;
    if (c->hasParent()) {
        actions.push_back(input::makeObjectAction(A::OPEN_PARENT_CONTAINER, item, position, pageSlot));
        if (!item->isNotMoveable()) {
            auto moveUp = input::mutableClone(*input::makeObjectAction(A::MOVE_UP, item, position, pageSlot));
            moveUp->destination = Position(UINT16_MAX, static_cast<uint16_t>(0x40 | m_containerId), ParentContainerSlot);
            moveUp->moveAll = true;
            actions.push_back(std::move(moveUp));
        }
    }
    actions.push_back(input::makeAction(A::CONTEXT_MENU));
    for (input::TGameActionPtr& action :
         input::filterActions(input::TGenericObjectGameActionProvider::objectActions(item, position, pageSlot), SlotActionTypes))
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
    const ContainerPtr c = container();
    if (!dnd || !c)
        return;
    const input::TGameActionPtr move = dnd->takeAction();
    if (!move)
        return;
    const bool moveAll = app::ClientOptions::load().options().value(QLatin1String(MoveAllOption)).toBool(false);
    const Position destination = c->getSlotPosition(slotId >= 0 ? slotId - c->getFirstIndex() : AnySlot);
    input::publish(input::finalizeDrop(move, destination, static_cast<int>(dnd->modifiers()), moveAll));
}

void TContainerController::onSlotTargetSelected(const int slotId)
{
    if (const input::TGameActionPtr target = firstOfType(slotActions(slotId), A::TARGET_SELECTED))
        input::publish(target);
}

void TContainerController::onPreviousPageClicked()
{
    if (const ContainerPtr c = container(); c && c->getFirstIndex() > 0)
        g_game.seekInContainer(m_containerId, std::max(0, c->getFirstIndex() - c->getCapacity()));
}

void TContainerController::onNextPageClicked()
{
    if (const ContainerPtr c = container(); c && c->getFirstIndex() + c->getCapacity() < c->getSize())
        g_game.seekInContainer(m_containerId, c->getFirstIndex() + c->getCapacity());
}

void TContainerController::onSearchButtonClicked()
{
    input::publish(input::makeAction(A::REQUEST_DEPOT_SEARCH));
}

void TContainerController::onRequestClose()
{
    removeThisContainersOptions();
    if (const ContainerPtr c = container())
        g_game.close(c);
    widget()->hide();
    TSidebarWidgetController::onRequestClose();
}

void TContainerController::onLevelUpClicked()
{
    if (const ContainerPtr c = container())
        g_game.openParent(c);
}

void TContainerController::readExpireOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_model.setExpireOptions(options.value(QLatin1String(ShowExpireOption)).toBool(true),
                             options.value(QLatin1String(ShowExpireWhenUnusedOption)).toBool(false));
}

}
