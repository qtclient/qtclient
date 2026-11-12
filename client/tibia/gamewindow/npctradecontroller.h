#pragma once

#include "trade/npctradestorage.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <vector>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// The NPC trade options of the context menu (CIP: the options struct read
/// through vtable 0x1d0 of the character options, accessors
/// ..).
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

/// One row of the trade list (CIP: the entries reset by).
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
/// data , roles 0x101..0x109).
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
/// TTraderInventorySortFilterProxyModel, filter , lessThan
/// ). Role enum properties feed NpcTrade.qml selection.
class TTraderInventorySortFilterProxyModel final : public cip::TTraderInventorySortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int appearanceIDEnumValue READ appearanceIDEnumValue CONSTANT)
    Q_PROPERTY(int liquidTypeEnumValue READ liquidTypeEnumValue CONSTANT)
public:
    explicit TTraderInventorySortFilterProxyModel(const TNPCTradeOptions* options, QObject* parent = nullptr);

    int appearanceIDEnumValue() const override;
    int liquidTypeEnumValue() const override;

    void setDirection(trade::ENPCTradeDirection direction);
    void setNameFilter(const QString& filter);

public Q_SLOTS:
    void onNPCTradeOptionsChanged() override;

Q_SIGNALS:
    /// CIP signal — tip owns emit (NpcTrade selection + amount refresh).
    void filterOrSortingChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    const TNPCTradeOptions* m_options;
    trade::ENPCTradeDirection m_direction = trade::ENPCTradeDirection::BUY;
    QString m_nameFilter;
};

/// NpcTrade.qml's trade (CIP: TNPCTradeController, ctor).
class TNPCTradeController final : public cip::tibia::gamewindow::TNPCTradeController
{
    Q_OBJECT
public:
    explicit TNPCTradeController(trade::TNPCTradeStorage* storage, QObject* parent = nullptr);
    ~TNPCTradeController() override;

    /// CIP /.
    QString bankBalance() const override;
    QString inventoryBalance() const override;
    QString totalBalance() const override;
    /// CIP .
    bool npcIsBuying() const override;
    /// CIP .
    QString buyOrSellPrice() const override;
    /// CIP /.
    int currencyTypeId() const override;
    QString currencyName() const override;
    bool currencyIsGold() const override;
    /// CIP .
    bool showNameFilter() const override;

public Q_SLOTS:
    /// CIP .
    void onLargeTradeConfirmed(bool DoNotShowDialogAgain) override;
    /// CIP .
    void onRequestCloseNpcTrade() override;

public:
    /// CIP.
    Q_INVOKABLE void switchToBuyItems() override;
    Q_INVOKABLE void switchToSellItems() override;
    /// CIP .
    Q_INVOKABLE void traderObjectSelected(int TypeId, uint LiquidType) override;
    /// CIP.
    Q_INVOKABLE void contextMenuForObject(int TypeId, uint LiquidType) override;
    /// CIP .
    Q_INVOKABLE void lookSelectedGoods() override;
    /// CIP .
    Q_INVOKABLE void buyOrSellSelectedGoods(int Amount) override;
    /// CIP .
    Q_INVOKABLE void setAmountPreselected(int Count) override;
    /// CIP .
    Q_INVOKABLE void requestFilterByName(const QString& NameFilter) override;

private:
    /// CIP .
    void refresh();
    /// CIP .
    void onAvailableCurrencyBalanceMightHaveChanged();
    /// CIP .
    void onNpcTradeOptionsChanged();
    /// CIP : the good of the type (and liquid for fluids).
    const trade::TNPCTradeGood* good(int typeId, uint liquidType) const;
    const trade::TNPCTradeGood* selectedGood() const;
    /// CIP .
    int maximumAmount(const trade::TNPCTradeGood* good) const;
    /// CIP .
    void updateAmount(bool force);
    void switchDirection(trade::ENPCTradeDirection direction);
    /// CIP .
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
}
