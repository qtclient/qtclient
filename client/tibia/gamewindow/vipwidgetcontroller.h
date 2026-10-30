#pragma once

#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/vipstorage.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QHash>
#include <QPointer>
#include <QSet>

#include <functional>
#include <vector>

class QTimer;

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::exivaoptions {
class TExivaOptionsStorage;
}

namespace tibia::gamewindow {

class TEditVipDialogController;
class TGenericEnterTextDialogController;
class TMessageDialogController;
class TSidebarWidget;

/// One row per (VIP, group) pair; an ungrouped VIP has one row in
/// "vip_ungrouped_group_name" (CIP: TVipWidgetDataModel, rows built by
/// 0x100be9ef0). Rebuilt whenever the storage changes.
class TVipWidgetDataModel final : public cip::tibia::gamewindow::TVipWidgetDataModel
{
    Q_OBJECT
public:
    /// CIP 0x100beafc0.
    enum Role
    {
        VipNameRole = 0x101,
        VipStateRole,
        VipHighlightedRole,
        VipIconIdRole,
        GroupNameRole,
        /// The VIP's first row: shown when groups are hidden.
        PrimaryRole,
    };

    explicit TVipWidgetDataModel(TVipStorage* storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    /// CIP 0x100be9a10.
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /// CIP 0x100beab00.
    void onVipGroupDataChanged() override;

private:
    struct Row
    {
        uint32_t id;
        QString name;
        int state;
        int iconId;
        QString groupName;
        bool primary;
    };

    /// CIP 0x100bea840: a state change stops the VIP's highlight timer; going
    /// from offline (0, 5) to online (1, 4) highlights it again for the
    /// timer's 1000 ms (TVipRenderInfo 0x100be9570, cleared by 0x100be9790).
    void onVipStateChanged(uint32_t id, int previousState, int state);
    void emitHighlightChanged(uint32_t id);

    TVipStorage* m_storage;
    std::vector<Row> m_rows;
    QHash<uint32_t, TVipRenderInfo*> m_renderInfos;
    QSet<uint32_t> m_highlighted;
};

/// Sorting and filtering of the VIP list (CIP:
/// TVipWidgetDataModelSortFilterProxy).
class TVipWidgetDataModelSortFilterProxy final : public cip::tibia::gamewindow::TVipWidgetDataModelSortFilterProxy
{
    Q_OBJECT
public:
    /// CIP EVipSortKey.
    enum SortKey
    {
        ByName = 0,
        ByIcon = 1,
        ByStatus = 2,
    };

    explicit TVipWidgetDataModelSortFilterProxy(QObject* parent = nullptr);

    bool hideOffline() const { return m_hideOffline; }
    /// CIP 0x100bec180 / 0x100bec1d0: configChanged unless silent.
    void setHideOffline(bool hide, bool silent);
    void setShowGroups(bool show, bool silent);
    /// CIP 0x100bec260 / 0x100bec220: the sort keys, most significant first.
    const std::vector<int>& sortOrder() const { return m_sortOrder; }
    void setSortOrder(const std::vector<int>& order);
    /// CIP 0x100bec310 (SORT_VIPS_BY_*): `key` becomes the most significant.
    void sortBy(int key);

protected:
    /// CIP 0x100beb3b0: offline VIPs when hidden; a VIP's other group rows
    /// without groups.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// CIP 0x100beb5e0.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::vector<int> m_sortOrder;
    bool m_hideOffline = false;
};

/// The VIP widget, vipwidget.qml (CIP: TVipWidgetController, ctor
/// 0x100be6550): the list, its add/edit dialogs, context menus and options.
/// Not ported: the add-VIP hotkey from the chat
/// (onPlayerNameToAddToVipListReceived only answers the dialog).
class TVipWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TVipWidgetController>
{
    Q_OBJECT
public:
    TVipWidgetController(TVipStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TVipWidgetController() override;

    /// vipWidgetOptions: size, sort order, hidden offline VIPs, groups.
    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// The exiva whitelist entries' data (CIP TVipWidgetGameActionProvider
    /// +0x18 the exiva options storage, +0x38 the world type).
    void setExivaOptions(exivaoptions::TExivaOptionsStorage* exivaOptions, int worldType);
    /// CIP 0x100be82d0: the widget's dialogs, sorting and filters.
    bool handleAction(const input::TGameAction& action);

public Q_SLOTS:
    /// CIP 0x100be7c20: OPEN_MESSAGE_TO_CHAT_CHANNEL.
    void onVipDoubleClicked(const QString& VipName) override;
    /// CIP 0x100be7d30: the mapped action of the clicked VIP or group header.
    void onVipOrGroupClicked(const QString& VipName, const QString& GroupName, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    /// CIP 0x100be81b0: the widget's own actions (its menu button, empty space).
    void onWidgetClicked(int MouseButtonRaw, int KeyboardModifiersRaw) override;
    /// CIP 0x100be6f20: ADD_NEW_VIP from the add-VIP dialog.
    void onPlayerNameToAddToVipListReceived(const QString& PlayerName) override;
    /// CIP 0x100be7030: ADD_NEW_VIP_GROUP.
    void onGroupNameToAddToVipListReceived(const QString& GroupName) override;
    /// CIP 0x100be7140: REQUEST_VIP_GROUP_UPDATE for the group the edit
    /// dialog was opened for.
    void onGroupNameEditedReceived(const QString& GroupName) override;
    void saveWidgetOptions() override;

private:
    /// CIP TVipGameActionProvider: the candidates of a click.
    input::TGameActionList candidates(const QString& vipName, const QString& groupName) const;
    void runClick(const input::TGameActionList& candidates, int mouseButton, int modifiers);
    void openAddGroupDialog();
    void readOptions();

    TVipStorage* m_storage;
    TVipWidgetDataModel* m_model;
    TVipWidgetDataModelSortFilterProxy* m_proxy;
    TGenericEnterTextDialogController* m_addVipDialog;
    TGenericEnterTextDialogController* m_addGroupDialog;
    TGenericEnterTextDialogController* m_editGroupDialog;
    TEditVipDialogController* m_editVipDialog;
    TMessageDialogController* m_messageDialog;
    QPointer<config::TSidebarOptions> m_options;
    exivaoptions::TExivaOptionsStorage* m_exivaOptions = nullptr;
    int m_worldType = -1;
    input::TGameActionMapper m_mapper;
    int m_editedGroupId = -1;
    bool m_readingOptions = false;
};

/// Owns the VIP storage, the exiva options storage and the widget while it is
/// open, and routes the widget's game actions to it. The sidebar widgets
/// manager opens and closes it (EWidgetType 3) through open / close / current.
class TVipWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TVipWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TVipWidgetManager() override;

    TVipStorage* storage() const { return m_storage; }
    exivaoptions::TExivaOptionsStorage* exivaOptions() const { return m_exivaOptions; }
    /// Before the sidebar widgets manager restores the open widgets.
    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme);
    /// CIP EWorldType of the session (worldTypeSet): exiva whitelist entries
    /// only on optional PvP worlds (1).
    void setWorldType(int worldType);

    /// Makes the widget (if needed) and returns it, for the sidebar manager to place.
    TSidebarWidget* open();
    /// Saves its options and deletes it.
    void close();
    TSidebarWidget* current() const;
    TVipWidgetController* controller() const { return m_controller; }

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    TVipStorage* m_storage;
    exivaoptions::TExivaOptionsStorage* m_exivaOptions;
    QPointer<TVipWidgetController> m_controller;
    QPointer<config::TSidebarOptions> m_options;
    int m_controlScheme = 0;
    int m_worldType = -1;
};

}
