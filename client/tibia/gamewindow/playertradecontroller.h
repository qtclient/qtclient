#pragma once

#include "container/containerciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "input/gameaction.h"
#include "trade/playertradestorage.h"

#include <cip_gen.h>

#include <QPointer>

namespace tibia {
namespace gamewindow {

class TSidebarManager;

/// One object of a trade offer (CIP: TPlayerTradeObject, ctor).
/// Appearance fields only — offers are CIP `TInventorySlot` lists in storage.
class TPlayerTradeObject final : public cip::tibia::trade::TPlayerTradeObject
{
    Q_OBJECT
public:
    TPlayerTradeObject(int slot, const container::TInventorySlot& appearance, QObject* parent = nullptr);

    /// CIP moc property getters (appearance fields filled in the ctor).
    uint slotID() const override;
    uint appearanceID() const override;
    uint objectCount() const override;
    uint objectUpgradeTier() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override;
};

/// The trade window, playertrade.qml (CIP: TPlayerTradeController, ctor
/// , vtable).
class TPlayerTradeController final : public TSidebarWidgetController<cip::tibia::gamewindow::TPlayerTradeController>
{
    Q_OBJECT

public:
    TPlayerTradeController(trade::TPlayerTradeStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    /// CIP moc property getters.
    QString ownName() const override;
    QString tradePartnerName() const override;
    QList<QObject*> ownOffer() const override;
    QList<QObject*> counterOffer() const override;
    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// CIP / : closing the window rejects the trade.
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    /// CIP v37 — empty; trade widget is ephemeral.
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
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — tip NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP sidebar chrome (drag/move) — tip redeclare; forwarded from chrome slots.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP moc signal (shared_ptr arg skipped from cip_gen); tip name parity.
    void publishGameAction();

private Q_SLOTS:
    /// CIP.
    void onOwnOfferChanged();
    void onCounterOfferChanged();
    /// CIP v38 — empty companion to saveWidgetOptions.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void showContextMenu(bool counterOffer, int slotIndex);

    QPointer<trade::TPlayerTradeStorage> m_storage;
};

/// Opens the trade window when a trade starts and closes it when it ends
/// (CIP: the sidebar manager's handling of
/// TPlayerTradeProtocolMessageHandler's requests), and handles TRADE_ITEM
/// (CIP: TPlayerTradeGameActionHandler, ctor , with
/// TGameActionPlayerTradeItem): the item is offered to the
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
}
