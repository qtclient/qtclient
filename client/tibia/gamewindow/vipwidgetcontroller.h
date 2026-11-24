#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/viprenderinfo.h"
#include "qtconnectionmanager.h"
#include "creatures/creaturesciptypes.h"
#include "creatures/vipstorage.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QHash>
#include <QPointer>

#include <functional>
#include <map>
#include <memory>
#include <vector>

class QTimer;

namespace tibia {
namespace game {
class IGameSession;
}
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace exivaoptions {
class TExivaOptionsStorage;
}
}

namespace tibia {
namespace gamewindow {

class TEditVipDialogController;
class TGenericEnterTextDialogController;
class TMessageDialogController;
class TSidebarWidget;

/// The VIP list rows (CIP: TVipWidgetDataModel): one TVipRenderInfo per
/// (VIP, group) pair; an ungrouped VIP has one row in
/// "vip_ungrouped_group_name". Rebuilt whenever a VIP or the groups change.
class TVipWidgetDataModel final : public cip::tibia::gamewindow::TVipWidgetDataModel
{
    Q_OBJECT
public:
    enum Role
    {
        VipNameRole = 0x101,
        VipStateRole,
        VipHighlightedRole,
        VipIconIdRole,
        GroupNameRole,
        /// The VIP's first row: shown when groups are hidden (no role name).
        PrimaryRole,
    };

    explicit TVipWidgetDataModel(QObject* parent = nullptr);

    /// The rows follow the session's VIP storage (none without a session).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Drops the old storage's connections, connects the new one's changes
    /// and rebuilds the rows.
    void setVipStorage(const std::shared_ptr<creatures::TVipStorage>& VipStorage);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    void onVipDataChangedOrAdded();
    /// A state change stops the rows' highlight timers; going from offline
    /// (0, 5) to online (1, 4) highlights them for the timer's interval.
    void onVipStateChanged(uint32_t CharacterID, int State);
    void onVipRemoved(uint32_t CharacterID);
    void onVipRenderInfoChanged(uint32_t CharacterID);
    void onVipGroupDataChanged() override;

private:
    /// Removes every row, then adds the storage's VIPs with its groups.
    void rebuild();
    void removeAllRows();
    /// Each VIP's rows: one per group (the first one is the VIP's first
    /// entry), or one in the ungrouped group.
    void addVips(const std::vector<creatures::TVipStorage::Vip>& Vips);
    void addVipRenderInfo(const creatures::TVipStorage::Vip& VipData, const QString& GroupName, bool FirstGroupEntry);

    std::vector<std::shared_ptr<TVipRenderInfo>> m_vipRenderInfos;
    std::shared_ptr<creatures::TVipStorage> m_vipStorage;
    shared::TQtConnectionManager m_connectionManager;
    std::map<uint8_t, creatures::TVipStorage::Group> m_groups;
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
    /// CIP moc property (QML section.property when true).
    bool showGroups() const override;
    /// CIP / : configChanged unless silent.
    void setHideOffline(bool hide, bool silent);
    void setShowGroups(bool show, bool silent);
    /// CIP / : the sort keys, most significant first.
    const std::vector<int>& sortOrder() const { return m_sortOrder; }
    void setSortOrder(const std::vector<int>& order);
    /// CIP (SORT_VIPS_BY_*): `key` becomes the most significant.
    void sortBy(int key);

    /// Group names in section order: case-insensitive, ungrouped last.
    static int compareGroups(const QString& left, const QString& right);

protected:
    /// Offline VIPs are hidden on request; a VIP's rows of further groups
    /// show only with groups.
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    /// Group (with groups shown), then the sort keys in order; rows equal on
    /// every key keep their source order.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    std::vector<int> m_sortOrder;
    bool m_hideOffline = false;
};

/// The VIP widget, vipwidget.qml (CIP: TVipWidgetController, ctor
/// ): the list, its add/edit dialogs, context menus and options.
/// Chat ADD_NEW_VIP / OPEN_ADD_VIP_DIALOG(text) add or prefill from speaker name.
class TVipWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TVipWidgetController>
{
    Q_OBJECT
public:
    TVipWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TVipWidgetController() override;

    /// vipWidgetOptions: size, sort order, hidden offline VIPs, groups.
    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    /// The widget follows `GameSession`'s VIP list: the add-VIP, add-group
    /// and edit-VIP dialogs close, the old session's connections drop, the
    /// options are read again and the list model follows the session.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP : the widget's dialogs, sorting and filters.
    bool handleAction(const input::IGameAction& action);

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    /// CIP : OPEN_MESSAGE_TO_CHAT_CHANNEL.
    void onVipDoubleClicked(const QString& VipName) override;
    /// CIP : the mapped action of the clicked VIP or group header.
    void onVipOrGroupClicked(const QString& VipName, const QString& GroupName, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    /// CIP : the widget's own actions (its menu button, empty space).
    void onWidgetClicked(int MouseButtonRaw, int KeyboardModifiersRaw) override;
    /// CIP : ADD_NEW_VIP from the add-VIP dialog.
    void onPlayerNameToAddToVipListReceived(const QString& PlayerName) override;
    /// CIP : ADD_NEW_VIP_GROUP.
    void onGroupNameToAddToVipListReceived(const QString& GroupName) override;
    /// CIP : REQUEST_VIP_GROUP_UPDATE for the group the edit
    /// dialog was opened for.
    void onGroupNameEditedReceived(const QString& GroupName) override;
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    /// CIP moc handleGameAction(shared_ptr<IGameAction>) — qtclient live path is
    /// TVipWidgetManager action handlers → handleAction.
    void handleGameAction();

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
    /// The edit dialog's result: REQUST_VIP_DATA_UPDATE.
    void onVipDataEdited(const tibia::creatures::TVipStorage::Vip& VipData);
    /// A VIP logged in (4) or out (5): the VIP list sound.
    void onLoginOrLogoutStateUpdate(int state);
    /// CIP onWidgetOptionsChanged — persist contentHeight / filters / sort.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void runClick(const input::TGameActionList& candidates, int mouseButton, int modifiers);
    /// OPEN_ADD_VIP_GROUP_DIALOG: the add dialog while groups are left, else a message.
    void openAddGroupDialog();
    /// OPEN_EDIT_VIP_DIALOG for the VIP named `vipName`.
    void openEditVipDialog(const QString& vipName);
    /// OPEN_EDIT_VIP_GROUP_DIALOG: remembers the group and shows its name.
    void openEditGroupDialog(const QString& groupName);
    void readOptions();

    shared::TQtConnectionManager m_connectionManager;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's VIP storage (the dialogs and the action provider read it).
    creatures::TVipStorage* m_storage = nullptr;
    TVipWidgetDataModel* m_model;
    TVipWidgetDataModelSortFilterProxy* m_proxy;
    TGenericEnterTextDialogController* m_addVipDialog;
    TGenericEnterTextDialogController* m_addGroupDialog;
    TGenericEnterTextDialogController* m_editGroupDialog;
    TEditVipDialogController* m_editVipDialog;
    TMessageDialogController* m_messageDialog;
    QPointer<config::TSidebarOptions> m_options;
    /// The session's provider (it follows the session's world type).
    std::shared_ptr<creatures::TVipWidgetGameActionProvider> m_provider;
    input::TGameActionMapper m_mapper;
    /// The group the edit-group dialog was opened for.
    creatures::TVipStorage::Group m_editedGroup;
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

    creatures::TVipStorage* storage() const { return m_storage; }
    /// The widget follows `GameSession`'s VIP list and exiva options. Host: the
    /// sidebar widgets manager's connector reaches the VIP widget through here.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Before the sidebar widgets manager restores the open widgets.
    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme);

    /// Makes the widget (if needed) and returns it, for the sidebar manager to place.
    TSidebarWidget* open();
    /// Saves its options and deletes it.
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;
    TVipWidgetController* controller() const { return m_controller; }


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<creatures::TVipStorage> m_storage;
    QPointer<TVipWidgetController> m_controller;
    QPointer<config::TSidebarOptions> m_options;
    int m_controlScheme = 0;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
