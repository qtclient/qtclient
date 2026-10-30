#pragma once

#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "trade/playertradestorage.h"

#include <cip_gen.h>

#include <client/declarations.h>

#include <QPointer>

namespace tibia::gamewindow {

class TSidebarManager;

/// One object of a trade offer (CIP: TPlayerTradeObject, ctor 0x10103e600).
class TPlayerTradeObject final : public cip::tibia::trade::TPlayerTradeObject
{
    Q_OBJECT
public:
    TPlayerTradeObject(int slot, const ItemPtr& item, QObject* parent = nullptr);
};

/// The trade window, playertrade.qml (CIP: TPlayerTradeController, ctor
/// 0x100c092d0, vtable 0x102636208).
class TPlayerTradeController final : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerTradeController>
{
    Q_OBJECT
public:
    TPlayerTradeController(trade::TPlayerTradeStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

public Q_SLOTS:
    /// CIP 0x100c09d50 / 0x100c09e50: closing the window rejects the trade.
    void onCloseClicked() override;
    void onRequestClose() override;

public:
    Q_INVOKABLE void rejectTrade() override;
    Q_INVOKABLE void acceptTrade() override;
    Q_INVOKABLE void lookOwnTradeSide(int SlotIndex) override;
    Q_INVOKABLE void lookCounterOfferTradeSide(int SlotIndex) override;
    Q_INVOKABLE void showContextMenuOwnTradeSide(int SlotIndex) override;
    Q_INVOKABLE void showContextMenuCounterOfferTradeSide(int SlotIndex) override;

private:
    /// CIP 0x100c09c20 / 0x100c09cb0.
    void updateOwnOffer();
    void updateCounterOffer();
    void showContextMenu(bool counterOffer, int slotIndex);

    QPointer<trade::TPlayerTradeStorage> m_storage;
};

/// Opens the trade window when a trade starts and closes it when it ends
/// (CIP: the sidebar manager's handling of
/// TPlayerTradeProtocolMessageHandler's requests), and handles TRADE_ITEM
/// (CIP: TPlayerTradeGameActionHandler, ctor 0x10103e9c0, with
/// TGameActionPlayerTradeItem 0x100e5a1f0): the item is offered to the
/// player picked by target selection.
class TPlayerTradeWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TPlayerTradeWidgetManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TPlayerTradeWidgetManager() override;

    trade::TPlayerTradeStorage* storage() { return &m_storage; }

private:
    void open();
    void close();
    bool tradeItem(const input::TGameAction& action);

    QPointer<TSidebarManager> m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    trade::TPlayerTradeStorage m_storage;
    QPointer<TPlayerTradeController> m_controller;
};

}
