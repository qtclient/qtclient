#include "playertradecontroller.h"
#include "game/currentgamesession.h"
#include "protocol/clientmessages.h"
#include "game/gamesessionbase.h"

#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/gameactionhandlers.h"

#include <creatures/creaturestorage.h>
#include <tibia/game/tgamenotify.h>

#include <algorithm>
#include <utility>

namespace tibia::gamewindow {

namespace {

using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

}

TPlayerTradeObject::TPlayerTradeObject(QObject* parent)
    : cip::tibia::trade::TPlayerTradeObject(parent)
{
    m_objectCount = 1;
}

uint TPlayerTradeObject::slotID() const { return m_slotID; }
uint TPlayerTradeObject::appearanceID() const { return m_appearanceID; }
uint TPlayerTradeObject::objectCount() const { return m_objectCount; }
uint TPlayerTradeObject::objectUpgradeTier() const { return m_objectUpgradeTier; }
Info::ELiquidType TPlayerTradeObject::liquidType() const { return m_liquidType; }
Info::EHookDirection TPlayerTradeObject::hookDirection() const { return m_hookDirection; }

void TPlayerTradeObject::setSlotID(const int slotId)
{
    if (m_slotID == static_cast<uint>(slotId))
        return;
    m_slotID = static_cast<uint>(slotId);
    Q_EMIT itemChanged();
}

void TPlayerTradeObject::setObject(const container::TInventorySlot& object)
{
    // A stack shows its count, any other object 1; inventory objects have no
    // hook direction.
    const auto appearanceId = static_cast<uint>(object.typeId);
    const auto count = static_cast<uint>(static_cast<uint8_t>(object.stackable ? object.count : 1));
    const auto liquidType = static_cast<Info::ELiquidType>(object.fluidOrSplash ? object.fluid : 0);
    const auto hookDirection = Info::EHookDirection::NONE;
    const auto tier = static_cast<uint>(object.tier);
    const bool changed = appearanceId != m_appearanceID || count != m_objectCount || liquidType != m_liquidType
                         || hookDirection != m_hookDirection || tier != m_objectUpgradeTier;
    m_appearanceID = appearanceId;
    m_objectCount = count;
    m_liquidType = liquidType;
    m_hookDirection = hookDirection;
    m_objectUpgradeTier = tier;
    if (changed)
        Q_EMIT itemChanged();
}

TPlayerTradeController::TPlayerTradeController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
{
    widget()->createItem(QStringLiteral("playertrade.qml"));
}

TPlayerTradeController::~TPlayerTradeController()
{
    clearOffers();
}

void TPlayerTradeController::setGameSession(trade::TPlayerTradeStorage* storage, trade::TPlayerTradeProtocolMessageHandler* handler)
{
    for (const QMetaObject::Connection& connection : std::exchange(m_sessionConnections, {}))
        disconnect(connection);
    m_storage = storage;
    m_handler = handler;
    clearOffers();
    if (!m_storage)
        return;
    m_sessionConnections.push_back(
        connect(m_storage, &trade::TPlayerTradeStorage::ownOfferChanged, this, &TPlayerTradeController::onOwnOfferChanged));
    m_sessionConnections.push_back(
        connect(m_storage, &trade::TPlayerTradeStorage::counterOfferChanged, this, &TPlayerTradeController::onCounterOfferChanged));
    onOwnOfferChanged();
    onCounterOfferChanged();
}

void TPlayerTradeController::clearOffers()
{
    for (QObject* object : std::exchange(m_ownOffer, {}))
        object->deleteLater();
    Q_EMIT ownOfferChanged();
    for (QObject* object : std::exchange(m_counterOffer, {}))
        object->deleteLater();
    Q_EMIT counterOfferChanged();
}

void TPlayerTradeController::rebuildOffer(QList<QObject*>& objects, const trade::TPlayerTradeOffer& offer)
{
    for (QObject* object : std::exchange(objects, {}))
        object->deleteLater();
    int slotId = 0;
    for (const container::TInventorySlot& slot : offer) {
        auto* object = new TPlayerTradeObject(this);
        object->setSlotID(slotId);
        object->setObject(slot);
        objects.append(object);
        ++slotId;
    }
}

QString TPlayerTradeController::ownName() const
{
    return m_storage ? m_storage->ownName() : qtTrId("dummy_unknown");
}

QString TPlayerTradeController::tradePartnerName() const
{
    return m_storage ? m_storage->counterName() : qtTrId("dummy_unknown");
}

QList<QObject*> TPlayerTradeController::ownOffer() const { return m_ownOffer; }

QList<QObject*> TPlayerTradeController::counterOffer() const { return m_counterOffer; }

QQuickItem* TPlayerTradeController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TPlayerTradeController::onOwnOfferChanged()
{
    if (!m_storage)
        return;
    rebuildOffer(m_ownOffer, m_storage->ownOffer());
    Q_EMIT ownOfferChanged();
}

void TPlayerTradeController::onCounterOfferChanged()
{
    if (!m_storage)
        return;
    rebuildOffer(m_counterOffer, m_storage->counterOffer());
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

void TPlayerTradeController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}

void TPlayerTradeController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}

void TPlayerTradeController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }

void TPlayerTradeController::onLevelUpClicked() {}

void TPlayerTradeController::saveWidgetOptions() {}

void TPlayerTradeController::onWidgetOptionsChanged() {}

void TPlayerTradeController::onWidgetQuickItemDestroyed() {}

void TPlayerTradeController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}

void TPlayerTradeController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

void TPlayerTradeController::rejectTrade()
{
    if (m_handler)
        m_handler->rejectTrade();
    Q_EMIT widgetClosed();
}

void TPlayerTradeController::acceptTrade()
{
    if (m_handler)
        m_handler->acceptTrade();
}

void TPlayerTradeController::lookOwnTradeSide(const int SlotIndex)
{
    if (SlotIndex < m_ownOffer.size() && m_handler)
        m_handler->lookTrade(SlotIndex, 0);
}

void TPlayerTradeController::lookCounterOfferTradeSide(const int SlotIndex)
{
    if (SlotIndex < m_counterOffer.size() && m_handler)
        m_handler->lookTrade(SlotIndex, 1);
}

void TPlayerTradeController::showContextMenuOwnTradeSide(const int SlotIndex)
{
    showContextMenu(SlotIndex, 0);
}

void TPlayerTradeController::showContextMenuCounterOfferTradeSide(const int SlotIndex)
{
    showContextMenu(SlotIndex, 1);
}

void TPlayerTradeController::lookTradeSide(const int slotIndex, const int side)
{
    const QList<QObject*>& offer = side == 0 ? m_ownOffer : m_counterOffer;
    if (m_handler && slotIndex < offer.size())
        m_handler->lookTrade(slotIndex, side);
}

void TPlayerTradeController::inspectTradeSide(const int slotIndex, const int side)
{
    const QList<QObject*>& offer = side == 0 ? m_ownOffer : m_counterOffer;
    if (slotIndex < 0 || slotIndex >= offer.size())
        return;
    // CipSoft inspects the trade slot itself (side and index), which the
    // protocol here cannot carry; the object type inspection is the closest.
    auto* object = qobject_cast<TPlayerTradeObject*>(offer[slotIndex]);
    if (!object || object->appearanceID() == 0)
        return;
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession(); session)
        tibia::protocol::gameclient::sendInspectionObject(tibia::client::INSPECT_NPCTRADE, static_cast<uint16_t>(object->appearanceID()),
                                                          static_cast<uint8_t>(std::clamp<int>(static_cast<int>(object->objectCount()), 1, 255)));
}

/// Look and Inspect for one slot of an offer.
void TPlayerTradeController::showContextMenu(const int slotIndex, const int side)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("context_look")), &TContextMenuEntryItem::triggered, this,
            [this, slotIndex, side] { lookTradeSide(slotIndex, side); });
    connect(menu->addCustomEntry(qtTrId("context_inspect_object")), &TContextMenuEntryItem::triggered, this,
            [this, slotIndex, side] { inspectTradeSide(slotIndex, side); });
    menu->showCustomMenu();
}

}
