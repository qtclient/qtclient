#pragma once

#include "client/gamecore.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QCollator>
#include <QSortFilterProxyModel>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace gamewindow {

class TConfirmMoveObjectToStashDialogController;
class TMessageDialogController;

/// The stash's name, category and trader filters and its sort order (CIP:
/// the TAppearanceTypeListModelSortFilterProxy at TStashDialogController
/// +0x138, setters /).
class TStashProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TStashProxyModel(TAppearanceTypeListModel* model, std::function<int(uint16_t)> counts, QObject* parent = nullptr);

    void setNameFilter(const QString& name);
    /// A TQmlObjectCategory identifier; AllCategories shows every type.
    void setCategory(int category);
    /// The NPC buying the type, empty for any.
    void setTrader(const QString& trader);
    /// cip::tibia::qmlcomponents::enums::ESortOrder.
    void setSortOrder(int order);
    int category() const { return m_category; }
    int sortOrder() const { return m_sortOrder; }
    /// No name, category -1 (nothing passes) and no trader.
    void resetFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TAppearanceTypeListModel* m_model;
    std::function<int(uint16_t)> m_counts;
    QString m_name;
    int m_category = TAppearanceTypeListModelSortFilterProxy::AllCategories;
    QString m_trader;
    int m_sortOrder = 1;
    QCollator m_collator;
};

/// StashDialog.qml: the supply stash's types (CIP: TStashDialogController,
/// qt_static_metacall). The server's Stash message (CIP
/// TContainerProtocolMessageHandler::handleStashMessage) shows or
/// refreshes it; SpecialContainersAvailable without the stash 
/// closes it. Also runs the stash game actions.
/// The NPCs the stash and depot search trader filters offer.
std::set<QString> traderNames();

class TStashDialogController final : public TDialogController<cip::tibia::gamewindow::TStashDialogController>
{
    Q_OBJECT
public:
    explicit TStashDialogController(QObject* parent = nullptr);
    ~TStashDialogController() override;

    void setNameFilter(const QString& name) override;
    void setCategoryFilterIndex(int index) override;
    void setTraderFilterIndex(int index) override;
    void setSortOrder(int order) override;
    int sortOrder() const override { return m_proxy.sortOrder(); }

public Q_SLOTS:
    /// CIP .
    void showDialog() override;
    void closeDialog() override;
    void resetCurrentFilters() override;
    void resetAllFilters() override;
    /// CIP .
    void closeButtonClicked() override;
    /// CIP .
    void slotClicked(int TypeID, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void requestOpenManageLootContainerDialog() override;
    /// CIP .
    void onRequestShowStashMessageDialog(const QString& Message) override;
    /// CIP .
    void onStashContentChanged() override;
    void onStashOptionsChanged() override;
    void onMarketDialogDialogClosed() override;
    void onManageLootContainerDialogClosed() override;

private:
    /// The name and the two filter indices, applied and remembered together.
    struct Filter
    {
        QString name;
        int categoryIndex = 0;
        int traderIndex = 0;
    };

    /// CIP TContainerGameActionProvider::v4 for a stash type.
    input::TGameActionList typeActions(uint16_t typeId) const;
    void setContent(std::map<uint16_t, uint32_t> content);
    int count(uint16_t typeId) const;
    void persistSortOrder(int order);
    void applyFilter(const Filter& filter);
    void saveFilter();
    /// The categories of the stored types, "all" first.
    void setCategories(const std::set<int>& categories);
    /// The NPCs the trader filter offers, "all" first.
    void setTraders(const std::set<QString>& traders);
    /// The market or loot container dialog closed after the stash made way for it.
    void reopenStash();
    void handleGameAction(const input::IGameAction& action);

    std::map<uint16_t, uint32_t> m_content;
    std::vector<int> m_categories;
    QStringList m_traders;
    Filter m_savedFilter;
    TAppearanceTypeListModel m_model;
    TStashProxyModel m_proxy;
    TMessageDialogController* m_info;
    TConfirmMoveObjectToStashDialogController* m_confirmStow;
    input::TGameActionMapper m_mapper;
    input::TMoveObjectPtr m_pendingStow;
    std::vector<client::gamecore::Hook> m_hooks;
    std::vector<int> m_actionListeners;
    bool m_reopenAfterMarket = false;
    bool m_reopenAfterManageLoot = false;
};

}
}
