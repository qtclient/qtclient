#pragma once

#include "trade/npctradestorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

/// The NPC trade options of the context menu (CIP: the options struct read
/// through vtable 0x1d0 of the character options, accessors
/// 0x100a154b0..0x100a15510).
struct TNPCTradeOptions
{
    bool sellEquipped = false;
    bool ignoreCapacity = false;
    bool buyWithBackpacks = false;
    bool showNameFilter = false;
    bool allowLargeTradesWithoutWarning = false;
    /// 0 name, 1 price, 2 weight.
    int buySortOrder = 0;
    /// 0 name, 1 price, 3 amount.
    int sellSortOrder = 0;

    static TNPCTradeOptions load();
    void store() const;
};

/// One row of the trade list (CIP: the entries reset by 0x100bf5c40).
struct TTraderInventoryEntry
{
    QString name;
    uint32_t price = 0;
    uint32_t weight = 0;
    int maximumTradeAmount = 0;
    uint16_t typeId = 0;
    uint8_t liquidType = 0;
    uint8_t hookDirection = 0;
};

/// The goods of the current trade direction (CIP: TTraderInventoryModel,
/// data 0x100bf9e40, roles 0x101..0x109).
class TTraderInventoryModel final : public cip::TTraderInventoryModel
{
    Q_OBJECT
public:
    enum Role {
        AppearanceID = Qt::UserRole + 1,
        LiquidType,
        HookDirection,
        CanBeTraded,
        MaximumTradeAmount,
        Name,
        PriceAndWeight,
        TradePrice,
        Weight,
    };

    using cip::TTraderInventoryModel::TTraderInventoryModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset(std::vector<TTraderInventoryEntry> entries);

private:
    std::vector<TTraderInventoryEntry> m_entries;
};

/// Name filter and sorting of the trade list (CIP:
/// TTraderInventorySortFilterProxyModel, filter 0x100bf9110, lessThan
/// 0x100bf92c0).
class TTraderInventorySortFilterProxyModel final : public cip::TTraderInventorySortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TTraderInventorySortFilterProxyModel(const TNPCTradeOptions* options, QObject* parent = nullptr);

    void setDirection(trade::ENPCTradeDirection direction);
    void setNameFilter(const QString& filter);

public Q_SLOTS:
    void onNPCTradeOptionsChanged() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    const TNPCTradeOptions* m_options;
    trade::ENPCTradeDirection m_direction = trade::ENPCTradeDirection::BUY;
    QString m_nameFilter;
};

/// NpcTrade.qml's trade (CIP: TNPCTradeController, ctor 0x100bed7a0).
class TNPCTradeController final : public cip::tibia::gamewindow::TNPCTradeController
{
    Q_OBJECT
public:
    explicit TNPCTradeController(trade::TNPCTradeStorage* storage, QObject* parent = nullptr);
    ~TNPCTradeController() override;

    /// CIP 0x100befb80 / 0x100befc20 / 0x100befcc0.
    QString bankBalance() const override;
    QString inventoryBalance() const override;
    QString totalBalance() const override;
    /// CIP 0x100befd00.
    bool npcIsBuying() const override;
    /// CIP 0x100bf25b0.
    QString buyOrSellPrice() const override;
    /// CIP 0x100bf2710 / 0x100bf2800 / 0x100bf2260.
    int currencyTypeId() const override;
    QString currencyName() const override;
    bool currencyIsGold() const override;
    /// CIP 0x100bf2b00.
    bool showNameFilter() const override;

public Q_SLOTS:
    /// CIP 0x100bee710.
    void onLargeTradeConfirmed(bool DoNotShowDialogAgain) override;
    /// CIP 0x100bef850.
    void onRequestCloseNpcTrade() override;

public:
    /// CIP 0x100befd20 / 0x100befda0.
    Q_INVOKABLE void switchToBuyItems() override;
    Q_INVOKABLE void switchToSellItems() override;
    /// CIP 0x100bee940.
    Q_INVOKABLE void traderObjectSelected(int TypeId, uint LiquidType) override;
    /// CIP 0x100bf0650 / 0x100bf0770.
    Q_INVOKABLE void contextMenuForObject(int TypeId, uint LiquidType) override;
    /// CIP 0x100beff70.
    Q_INVOKABLE void lookSelectedGoods() override;
    /// CIP 0x100bf1b70.
    Q_INVOKABLE void buyOrSellSelectedGoods(int Amount) override;
    /// CIP 0x100bf1fb0.
    Q_INVOKABLE void setAmountPreselected(int Count) override;
    /// CIP 0x100bf2b30.
    Q_INVOKABLE void requestFilterByName(const QString& NameFilter) override;

private:
    /// CIP 0x100bedce0.
    void refresh();
    /// CIP 0x100bef610.
    void onAvailableCurrencyBalanceMightHaveChanged();
    /// CIP 0x100bef650.
    void onNpcTradeOptionsChanged();
    /// CIP 0x100bf0110: the good of the type (and liquid for fluids).
    const trade::TNPCTradeGood* good(int typeId, uint liquidType) const;
    const trade::TNPCTradeGood* selectedGood() const;
    /// CIP 0x100bf2350.
    int maximumAmount(const trade::TNPCTradeGood* good) const;
    /// CIP 0x100befe20.
    void updateAmount(bool force);
    void switchDirection(trade::ENPCTradeDirection direction);
    /// CIP 0x100bef660.
    void trade(int amount);
    void setOptions(const TNPCTradeOptions& options);

    QPointer<trade::TNPCTradeStorage> m_storage;
    TNPCTradeOptions m_options;
    TTraderInventoryModel m_model;
    TTraderInventorySortFilterProxyModel m_proxy;
    std::vector<trade::TNPCTradeGood> m_goods;
    QTimer m_refreshTimer;
    uint64_t m_money = 0;
    QPointer<TMessageDialogController> m_confirmation;
};

}
