#include "playertradecontroller.h"

#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/map.h>

#include <algorithm>
#include <utility>

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

QList<QObject*> tradeObjects(const std::vector<ItemPtr>& items, QObject* parent)
{
    QList<QObject*> objects;
    objects.reserve(static_cast<qsizetype>(items.size()));
    for (size_t slot = 0; slot < items.size(); ++slot)
        objects.append(new TPlayerTradeObject(static_cast<int>(slot), items[slot], parent));
    return objects;
}

void replaceObjects(QList<QObject*>& objects, QList<QObject*> replacement)
{
    for (QObject* object : std::exchange(objects, std::move(replacement)))
        object->deleteLater();
}

}

TPlayerTradeObject::TPlayerTradeObject(const int slot, const ItemPtr& item, QObject* parent)
    : cip::tibia::trade::TPlayerTradeObject(parent)
{
    m_slotID = static_cast<uint>(slot);
    if (!item)
        return;
    m_appearanceID = item->getId();
    m_objectCount = item->isStackable() ? std::max<uint>(1, item->getCountOrSubType()) : 1;
    m_objectUpgradeTier = item->getTier();
    if (item->isFluidContainer() || item->isSplash())
        m_liquidType = static_cast<Info::ELiquidType>(item->getCountOrSubType());
    if (item->isHookSouth())
        m_hookDirection = static_cast<Info::EHookDirection>(1);
    else if (item->isHookEast())
        m_hookDirection = static_cast<Info::EHookDirection>(2);
}

TPlayerTradeController::TPlayerTradeController(trade::TPlayerTradeStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    // CIP 0x100c09900.
    connect(storage, &trade::TPlayerTradeStorage::ownOfferChanged, this, &TPlayerTradeController::updateOwnOffer);
    connect(storage, &trade::TPlayerTradeStorage::counterOfferChanged, this, &TPlayerTradeController::updateCounterOffer);
    updateOwnOffer();
    updateCounterOffer();

    widget()->createItem(QStringLiteral("playertrade.qml"));
}

void TPlayerTradeController::updateOwnOffer()
{
    if (!m_storage)
        return;
    m_ownName = m_storage->ownName();
    replaceObjects(m_ownOffer, tradeObjects(m_storage->ownOffer(), this));
    Q_EMIT ownOfferChanged();
}

void TPlayerTradeController::updateCounterOffer()
{
    if (!m_storage)
        return;
    m_tradePartnerName = m_storage->counterName();
    replaceObjects(m_counterOffer, tradeObjects(m_storage->counterOffer(), this));
    Q_EMIT counterOfferChanged();
}

void TPlayerTradeController::onCloseClicked()
{
    rejectTrade();
}

void TPlayerTradeController::onRequestClose()
{
    rejectTrade();
}

/// CIP 0x100c09dd0.
void TPlayerTradeController::rejectTrade()
{
    if (m_storage)
        m_storage->reject();
    Q_EMIT widgetClosed();
}

/// CIP 0x100c0a290.
void TPlayerTradeController::acceptTrade()
{
    if (m_storage)
        m_storage->accept();
}

/// CIP 0x100c0a310.
void TPlayerTradeController::lookOwnTradeSide(const int SlotIndex)
{
    if (m_storage && SlotIndex >= 0 && SlotIndex < m_ownOffer.size())
        m_storage->look(false, SlotIndex);
}

/// CIP 0x100c0a440.
void TPlayerTradeController::lookCounterOfferTradeSide(const int SlotIndex)
{
    if (m_storage && SlotIndex >= 0 && SlotIndex < m_counterOffer.size())
        m_storage->look(true, SlotIndex);
}

void TPlayerTradeController::showContextMenuOwnTradeSide(const int SlotIndex)
{
    showContextMenu(false, SlotIndex);
}

void TPlayerTradeController::showContextMenuCounterOfferTradeSide(const int SlotIndex)
{
    showContextMenu(true, SlotIndex);
}

/// CIP 0x100c0a4e0: Look and Inspect.
void TPlayerTradeController::showContextMenu(const bool counterOffer, const int slotIndex)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("context_look")), &TContextMenuEntryItem::triggered, this, [this, counterOffer, slotIndex] {
        if (counterOffer)
            lookCounterOfferTradeSide(slotIndex);
        else
            lookOwnTradeSide(slotIndex);
    });
    // CIP 0x10103f800 sends the player trade inspection (type 2, side and
    // index), which Crystal does not parse; its item type inspection is the
    // closest it answers.
    connect(menu->addCustomEntry(qtTrId("context_inspect_object")), &TContextMenuEntryItem::triggered, this, [this, counterOffer, slotIndex] {
        if (!m_storage)
            return;
        const std::vector<ItemPtr>& items = counterOffer ? m_storage->counterOffer() : m_storage->ownOffer();
        if (slotIndex < 0 || slotIndex >= static_cast<int>(items.size()) || !items[slotIndex])
            return;
        const ItemPtr& item = items[slotIndex];
        g_game.inspectionObject(Otc::INSPECT_NPCTRADE, static_cast<uint16_t>(item->getId()),
                                static_cast<uint8_t>(std::clamp<int>(item->getCount(), 1, 255)));
    });
    menu->showCustomMenu();
}

TPlayerTradeWidgetManager::TPlayerTradeWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    connect(&m_storage, &trade::TPlayerTradeStorage::requestOpenPlayerTradeWidget, this, &TPlayerTradeWidgetManager::open);
    connect(&m_storage, &trade::TPlayerTradeStorage::requestClosePlayerTradeWidget, this, &TPlayerTradeWidgetManager::close);
    input::setActionHandler(input::EGameAction::TRADE_ITEM, [this](const input::TGameAction& action) { return tradeItem(action); });
}

TPlayerTradeWidgetManager::~TPlayerTradeWidgetManager()
{
    input::setActionHandler(input::EGameAction::TRADE_ITEM, nullptr);
    if (input::isSelectingTarget())
        input::finishTargetSelection(nullptr);
}

void TPlayerTradeWidgetManager::open()
{
    if (m_controller || !m_sidebars)
        return;
    m_controller = new TPlayerTradeController(&m_storage, m_gameWindowItem, m_engine, this);
    connect(m_controller, &TPlayerTradeController::widgetClosed, this, &TPlayerTradeWidgetManager::close);
    m_sidebars->addWidget(m_controller->widget());
    m_controller->widget()->show();
}

void TPlayerTradeWidgetManager::close()
{
    if (!m_controller)
        return;
    TPlayerTradeController* controller = m_controller;
    m_controller = nullptr;
    controller->widget()->hide();
    controller->deleteLater();
}

/// CIP 0x10103e9c0 / 0x100e5a1f0: TRADE_ITEM starts a target selection; a
/// player picked on the map gets the trade request.
bool TPlayerTradeWidgetManager::tradeItem(const input::TGameAction& action)
{
    if (!action.object || !action.object->isItem())
        return true;
    const ItemPtr item = action.object->static_self_cast<Item>();
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(true);
    input::startTargetSelection([item](const input::TGameActionPtr& target) {
        if (TMouseCursorController* cursor = TMouseCursorController::instance())
            cursor->setTargetSelection(false);
        if (!target || !target->creatureId)
            return;
        const CreaturePtr creature = g_map.getCreatureById(target->creatureId);
        if (creature && creature->isPlayer() && !creature->isLocalPlayer())
            g_game.requestTrade(item, creature);
    });
    return true;
}

}
