#pragma once

#include "input/gameactiondispatch.h"
#include "container/containerciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "trade/playertradestorage.h"
#include "trade/tradeciptypes.h"

#include <cip_gen.h>

#include <memory>
#include <vector>

#include <QPointer>

#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

class TSidebarManager;

/// One object of a trade offer, as shown by playertrade.qml.
class TPlayerTradeObject final : public cip::tibia::trade::TPlayerTradeObject
{
    Q_OBJECT
public:
    explicit TPlayerTradeObject(QObject* parent = nullptr);

    /// moc property getters.
    uint slotID() const override;
    uint appearanceID() const override;
    uint objectCount() const override;
    uint objectUpgradeTier() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override;

    void setSlotID(int slotId);
    /// Takes the appearance fields of `object`; notifies once when any changed.
    void setObject(const container::TInventorySlot& object);
};

/// The trade window, playertrade.qml.
class TPlayerTradeController final : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerTradeController>
{
    Q_OBJECT

public:
    TPlayerTradeController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerTradeController() override;

    /// Follows the trade of a game session (none: both offers empty).
    void setGameSession(trade::TPlayerTradeStorage* storage, trade::TPlayerTradeProtocolMessageHandler* handler);

    /// moc property getters.
    QString ownName() const override;
    QString tradePartnerName() const override;
    QList<QObject*> ownOffer() const override;
    QList<QObject*> counterOffer() const override;
    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// Closing the window rejects the trade.
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    /// The trade window has no options.
    void saveWidgetOptions() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

public:
    Q_INVOKABLE void rejectTrade() override;
    Q_INVOKABLE void acceptTrade() override;
    Q_INVOKABLE void lookOwnTradeSide(int SlotIndex) override;
    Q_INVOKABLE void lookCounterOfferTradeSide(int SlotIndex) override;
    Q_INVOKABLE void showContextMenuOwnTradeSide(int SlotIndex) override;
    Q_INVOKABLE void showContextMenuCounterOfferTradeSide(int SlotIndex) override;

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — qtclient redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); qtclient name parity.

private Q_SLOTS:
    void onOwnOfferChanged();
    void onCounterOfferChanged();
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    /// Deletes the objects of both offers and notifies both.
    void clearOffers();
    /// Rebuilds `objects` from the offer, one object per slot.
    void rebuildOffer(QList<QObject*>& objects, const trade::TPlayerTradeOffer& offer);
    /// `side` 0 is the own offer, 1 the counter offer.
    void lookTradeSide(int slotIndex, int side);
    void inspectTradeSide(int slotIndex, int side);
    void showContextMenu(int slotIndex, int side);

    QPointer<trade::TPlayerTradeStorage> m_storage;
    QPointer<trade::TPlayerTradeProtocolMessageHandler> m_handler;
    std::vector<QMetaObject::Connection> m_sessionConnections;
};

/// Follows the session's trade storage and protocol handler, opens the trade window when
/// the handler asks for it and closes it when the trade ends, and handles
/// TRADE_ITEM: the item is offered to the player picked by target selection.
class TPlayerTradeWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TPlayerTradeWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerTradeWidgetManager() override;

    trade::TPlayerTradeStorage* storage() const { return m_storage; }
    /// The window closes; the trade follows `GameSession`'s storage and handler.
    /// Host: the sidebar widgets manager's connector reaches the trade window here.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

private:
    void open();
    void close();
    bool tradeItem(const input::TGameActionPlayerTradeItem& action);

    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<trade::TPlayerTradeStorage> m_storage;
    QPointer<trade::TPlayerTradeProtocolMessageHandler> m_handler;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    QPointer<TPlayerTradeController> m_controller;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
