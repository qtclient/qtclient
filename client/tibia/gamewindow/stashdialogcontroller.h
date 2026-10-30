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
#include <vector>

namespace tibia::gamewindow {

class TConfirmMoveObjectToStashDialogController;
class TMessageDialogController;

/// The stash's name, category and trader filters and its sort order (CIP:
/// the TAppearanceTypeListModelSortFilterProxy at TStashDialogController
/// +0x138, setters 0x100d6d480 / 0x100d6d250 / 0x100d6d270 / 0x100d6d7e0).
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
/// qt_static_metacall 0x100843610). The server's Stash message (CIP
/// TContainerProtocolMessageHandler::handleStashMessage 0x100a318e0) shows or
/// refreshes it; SpecialContainersAvailable without the stash (0x100a31d30)
/// closes it. Also runs the stash game actions.
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

public Q_SLOTS:
    /// CIP 0x100d435f0.
    void showDialog() override;
    void closeDialog() override;
    /// CIP 0x100d43cd0.
    void resetFilters() override;
    /// CIP 0x100d436f0.
    void closeButtonClicked() override;
    /// CIP 0x100d43e40.
    void slotClicked(int TypeID, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    /// CIP 0x100d441c0.
    void requestOpenManageQuickLootDialog() override;
    /// CIP 0x100d43700.
    void onRequestShowStashMessageDialog(const QString& Message) override;
    /// CIP 0x100d437f0.
    void onStashContentChanged() override;

private:
    /// CIP TContainerGameActionProvider::v4 (0x100b0f910) for a stash type.
    input::TGameActionList typeActions(uint16_t typeId) const;
    void setContent(std::map<uint16_t, uint32_t> content);
    int count(uint16_t typeId) const;
    void applyCategory();
    void applyTrader();

    std::map<uint16_t, uint32_t> m_content;
    std::vector<int> m_categories;
    QStringList m_traders;
    TAppearanceTypeListModel m_model;
    TStashProxyModel m_proxy;
    TMessageDialogController* m_info;
    TConfirmMoveObjectToStashDialogController* m_confirmStow;
    input::TGameActionMapper m_mapper;
    std::shared_ptr<input::TGameAction> m_pendingStow;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
