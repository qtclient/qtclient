#include "vipwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/editvipdialogcontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "input/gameactionhandlers.h"


#include <QJsonArray>
#include <QJsonObject>
#include <QQuickItem>
#include <QTimer>

#include <algorithm>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr auto OptionsKey = "vipWidgetOptions";
/// CIP : TGameActionMapper context of the VIP widget.
constexpr int VipWidgetContext = 7;
/// CIP : the add/edit name dialogs' input length.
constexpr uint MaxNameLength = 29;
/// CIP : TVipRenderInfo's single-shot highlight timer.
constexpr int HighlightMs = 1000;

/// CIP 's context menu order.
constexpr A ContextMenuOrder[] = {
    A::OPEN_EDIT_VIP_DIALOG, A::REMOVE_VIP, A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::OPEN_ADD_VIP_DIALOG, A::CONTEXT_MENU_SEPARATOR,
    A::OPEN_EDIT_VIP_GROUP_DIALOG, A::REMOVE_VIP_GROUP, A::OPEN_ADD_VIP_GROUP_DIALOG, A::CONTEXT_MENU_SEPARATOR, A::SORT_VIPS_BY_NAME,
    A::SORT_VIPS_BY_ICON, A::SORT_VIPS_BY_STATUS, A::HIDE_OFFLINE_VIPS, A::SHOW_OFFLINE_VIPS, A::HIDE_VIPS_GROUPS, A::SHOW_VIPS_GROUPS,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_NAME, A::CONTEXT_MENU_SEPARATOR, A::EXIVA_ADD_CHARACTER_TO_WHITELIST,
    A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST, A::CONTEXT_MENU_SEPARATOR, A::COPY_CREATURE_NAME_TO_CLIPBOARD,
};

/// The widget's own game actions (CIP).
constexpr A WidgetActions[] = {
    A::OPEN_ADD_VIP_DIALOG, A::OPEN_EDIT_VIP_DIALOG, A::OPEN_ADD_VIP_GROUP_DIALOG, A::OPEN_EDIT_VIP_GROUP_DIALOG,
    A::SORT_VIPS_BY_NAME,   A::SORT_VIPS_BY_ICON,    A::SORT_VIPS_BY_STATUS,       A::HIDE_OFFLINE_VIPS,
    A::SHOW_OFFLINE_VIPS,   A::SHOW_VIPS_GROUPS,     A::HIDE_VIPS_GROUPS,
};

QString ungroupedName() { return qtTrId("vip_ungrouped_group_name"); }

bool isOffline(const int state) { return state == 0 || state == 5; }

bool isOnline(const int state) { return state == 1 || state == 4; }

/// The status sort rank: online (1, 3, 4) before offline (0, 2, 5), unknown
/// states last.
int statusRank(const int state)
{
    static constexpr int Rank[] = { 1, 0, 1, 0, 0, 1 };
    return state >= 0 && state < 6 ? Rank[state] : 2;
}

input::TGameActionPtr makeNamed(const A type, const QString& text, const int instance = 0, const int parameter = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->text = text;
    action->instance = instance;
    action->parameter = parameter;
    return action;
}

}

TVipWidgetDataModel::TVipWidgetDataModel(creatures::TVipStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TVipWidgetDataModel(parent)
    , m_storage(storage)
{
    connect(storage, &creatures::TVipStorage::vipDataChangedOrAdded, this, &TVipWidgetDataModel::onVipDataChangedOrAdded);
    connect(storage, &creatures::TVipStorage::vipDataRemoved, this, &TVipWidgetDataModel::onVipRemoved);
    connect(storage, &creatures::TVipStorage::vipGroupDataChanged, this, &TVipWidgetDataModel::onVipGroupDataChanged);
    connect(storage, &creatures::TVipStorage::vipStateChanged, this, &TVipWidgetDataModel::onVipStateChanged);
    onVipGroupDataChanged();
}

/// A VIP's rows take the new state; coming online from offline highlights them
/// until the highlight timer ends.
void TVipWidgetDataModel::onVipStateChanged(const uint32_t id, const int state)
{
    const auto it = std::find_if(m_rows.begin(), m_rows.end(), [id](const Row& row) { return row.id == id; });
    if (it == m_rows.end() || it->state == state)
        return;
    TVipRenderInfo*& info = m_renderInfos[id];
    if (!info) {
        info = new TVipRenderInfo(id, this);
        connect(info, &TVipRenderInfo::vipRenderInfoChanged, this, [this](const uint32_t characterId) {
            if (TVipRenderInfo* render = m_renderInfos.value(characterId); !render || !render->highlighted())
                m_highlighted.remove(characterId);
            onVipRenderInfoChanged(characterId);
        });
    }
    info->clearHighlight();
    if (isOffline(it->state) && isOnline(state)) {
        m_highlighted.insert(id);
        info->startHighlight(HighlightMs);
    }
    for (Row& row : m_rows) {
        if (row.id == id)
            row.state = state;
    }
    onVipRenderInfoChanged(id);
}

int TVipWidgetDataModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }

QVariant TVipWidgetDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const Row& row = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
        case VipNameRole:
            return row.name;
        case VipStateRole:
            return row.state;
        case VipHighlightedRole:
            return m_highlighted.contains(row.id);
        case VipIconIdRole:
            return row.iconId;
        case GroupNameRole:
            return row.groupName;
        case PrimaryRole:
            return row.primary;
        default:
            return {};
    }
}

QHash<int, QByteArray> TVipWidgetDataModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert({
        { VipNameRole, "vipName" },   { VipStateRole, "vipState" },     { VipHighlightedRole, "vipHighlighted" },
        { VipIconIdRole, "vipIconId" }, { GroupNameRole, "groupName" },
    });
    return roles;
}

void TVipWidgetDataModel::onVipGroupDataChanged()
{
    beginResetModel();
    m_rows.clear();
    const std::vector<creatures::TVipStorage::Group>& groups = m_storage->groups();
    for (const creatures::TVipStorage::Vip& vip : m_storage->vips()) {
        if (vip.groups.empty()) {
            m_rows.push_back({ vip.id, vip.name, vip.state, vip.iconId, ungroupedName(), true });
            continue;
        }
        bool primary = true;
        for (const uint8_t groupId : vip.groups) {
            QString groupName = ungroupedName();
            const auto it = std::find_if(groups.begin(), groups.end(), [groupId](const creatures::TVipStorage::Group& group) { return group.id == groupId; });
            if (it != groups.end())
                groupName = it->name;
            m_rows.push_back({ vip.id, vip.name, vip.state, vip.iconId, groupName, primary });
            primary = false;
        }
    }
    endResetModel();
}

void TVipWidgetDataModel::onVipDataChangedOrAdded() { onVipGroupDataChanged(); }

void TVipWidgetDataModel::onVipRemoved(const uint32_t characterId)
{
    for (;;) {
        const auto it = std::find_if(m_rows.begin(), m_rows.end(), [characterId](const Row& row) { return row.id == characterId; });
        if (it == m_rows.end())
            return;
        const int row = static_cast<int>(it - m_rows.begin());
        beginRemoveRows({}, row, row);
        m_rows.erase(it);
        endRemoveRows();
    }
}

/// Every row of the VIP changed.
void TVipWidgetDataModel::onVipRenderInfoChanged(const uint32_t characterId)
{
    for (size_t row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].id == characterId) {
            const QModelIndex changed = index(static_cast<int>(row), 0);
            Q_EMIT dataChanged(changed, changed);
        }
    }
}

TVipWidgetDataModelSortFilterProxy::TVipWidgetDataModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TVipWidgetDataModelSortFilterProxy(parent)
{
    setDynamicSortFilter(true);
    sort(0);
}

bool TVipWidgetDataModelSortFilterProxy::showGroups() const { return m_showGroups; }

void TVipWidgetDataModelSortFilterProxy::setHideOffline(const bool hide, const bool silent)
{
    if (m_hideOffline == hide)
        return;
    m_hideOffline = hide;
    invalidate();
    if (!silent)
        Q_EMIT configChanged();
}

void TVipWidgetDataModelSortFilterProxy::setShowGroups(const bool show, const bool silent)
{
    if (m_showGroups == show)
        return;
    m_showGroups = show;
    Q_EMIT showGroupsChanged();
    invalidate();
    if (!silent)
        Q_EMIT configChanged();
}

void TVipWidgetDataModelSortFilterProxy::setSortOrder(const std::vector<int>& order)
{
    m_sortOrder = order;
    invalidate();
}

void TVipWidgetDataModelSortFilterProxy::sortBy(const int key)
{
    std::erase(m_sortOrder, key);
    m_sortOrder.insert(m_sortOrder.begin(), key);
    invalidate();
    Q_EMIT sortOrderChanged();
    Q_EMIT configChanged();
}

bool TVipWidgetDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel())
        return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    if (m_hideOffline && isOffline(index.data(TVipWidgetDataModel::VipStateRole).toInt()))
        return false;
    return index.data(TVipWidgetDataModel::PrimaryRole).toBool() || m_showGroups;
}

int TVipWidgetDataModelSortFilterProxy::compareGroups(const QString& left, const QString& right)
{
    // The ungrouped section compares as an empty name and goes last.
    const QString ungrouped = ungroupedName();
    const QString a = left == ungrouped ? QString() : left;
    const QString b = right == ungrouped ? QString() : right;
    const int order = a.compare(b, Qt::CaseInsensitive);
    return a.isEmpty() || b.isEmpty() ? -order : order;
}

bool TVipWidgetDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (!sourceModel())
        return false;
    using M = TVipWidgetDataModel;
    if (m_showGroups) {
        if (const int order = compareGroups(left.data(M::GroupNameRole).toString(), right.data(M::GroupNameRole).toString()); order != 0)
            return order < 0;
    }
    for (const int key : m_sortOrder) {
        int order = 0;
        switch (key) {
            case ByStatus:
                order = statusRank(left.data(M::VipStateRole).toInt()) - statusRank(right.data(M::VipStateRole).toInt());
                break;
            case ByIcon:
                order = static_cast<uint8_t>(left.data(M::VipIconIdRole).toInt()) - static_cast<uint8_t>(right.data(M::VipIconIdRole).toInt());
                break;
            case ByName:
                order = left.data(M::VipNameRole).toString().compare(right.data(M::VipNameRole).toString(), Qt::CaseInsensitive);
                break;
            default:
                break;
        }
        if (order != 0)
            return order < 0;
    }
    return false;
}

TVipWidgetController::TVipWidgetController(creatures::TVipStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_model(new TVipWidgetDataModel(storage, this))
    , m_proxy(new TVipWidgetDataModelSortFilterProxy(this))
    , m_addVipDialog(new TGenericEnterTextDialogController(this))
    , m_addGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editVipDialog(new TEditVipDialogController(this))
    , m_messageDialog(new TMessageDialogController(this))
    , m_provider(storage, nullptr)
{
    connect(m_editVipDialog, &TEditVipDialogController::vipDataUpdated, this, &TVipWidgetController::onVipDataEdited);
    connect(storage, &creatures::TVipStorage::loginOrLogoutStateUpdate, this, &TVipWidgetController::onLoginOrLogoutStateUpdate);
    m_proxy->setSourceModel(m_model);

    m_addVipDialog->setCaption(qtTrId("add_new_vip_caption"));
    m_addVipDialog->setDescription(qtTrId("add_new_vip_text"));
    m_addVipDialog->setMaxInputLength(MaxNameLength);
    connect(m_addVipDialog, &TGenericEnterTextDialogController::textEntered, this,
            &TVipWidgetController::onPlayerNameToAddToVipListReceived);
    m_addGroupDialog->setDescription(qtTrId("add_new_vip_group_text"));
    m_addGroupDialog->setMaxInputLength(MaxNameLength);
    connect(m_addGroupDialog, &TGenericEnterTextDialogController::textEntered, this,
            &TVipWidgetController::onGroupNameToAddToVipListReceived);
    m_editGroupDialog->setCaption(qtTrId("edit_vip_group_caption"));
    m_editGroupDialog->setDescription(qtTrId("edit_vip_group_text"));
    m_editGroupDialog->setMaxInputLength(MaxNameLength);
    connect(m_editGroupDialog, &TGenericEnterTextDialogController::textEntered, this, &TVipWidgetController::onGroupNameEditedReceived);

    widget()->createItem(QStringLiteral("vipwidget.qml"));
    if (QQuickItem* item = quickItem())
        item->setProperty("vipModel", QVariant::fromValue<QObject*>(m_proxy));
    connect(m_proxy, &TVipWidgetDataModelSortFilterProxy::configChanged, this, &TVipWidgetController::saveWidgetOptions);
}

TVipWidgetController::~TVipWidgetController() = default;

QQuickItem* TVipWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
void TVipWidgetController::onCloseClicked() { onRequestClose(); }
void TVipWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TVipWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TVipWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TVipWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TVipWidgetController::onLevelUpClicked() {}
void TVipWidgetController::setCursorToResizeVertical(const bool Enabled) { widget()->setCursorToResizeVertical(Enabled); }
void TVipWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
/// CIP: the stored widget options are applied (saving is saveWidgetOptions).
void TVipWidgetController::onWidgetOptionsChanged() { readOptions(); }
void TVipWidgetController::onWidgetQuickItemDestroyed() {}

void TVipWidgetController::handleGameAction()
{
    // CIP moc shared_ptr dispatch; qtclient routes via setActionHandler → handleAction.
}

void TVipWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    onWidgetOptionsChanged();
}

void TVipWidgetController::readOptions()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    std::vector<int> order;
    for (const QJsonValue& value : json.value(QLatin1String("vipSortOrder")).toArray())
        order.push_back(value.toInt());
    m_readingOptions = true;
    m_proxy->setSortOrder(order);
    m_proxy->setHideOffline(json.value(QLatin1String("hideOfflineVips")).toBool(false), true);
    m_proxy->setShowGroups(json.value(QLatin1String("showGrouped")).toBool(false), true);
    m_readingOptions = false;
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TVipWidgetController::saveWidgetOptions()
{
    if (!m_options || m_readingOptions)
        return;
    QJsonArray order;
    for (const int key : m_proxy->sortOrder())
        order.append(key);
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    json.insert(QStringLiteral("vipSortOrder"), order);
    json.insert(QStringLiteral("hideOfflineVips"), m_proxy->hideOffline());
    json.insert(QStringLiteral("showGrouped"), m_proxy->showGroups());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TVipWidgetController::setExivaOptions(exivaoptions::TExivaOptionsStorage* exivaOptions, const int worldType)
{
    m_provider = creatures::TVipWidgetGameActionProvider(m_storage, exivaOptions);
    m_provider.setWorldType(worldType);
}

void TVipWidgetController::runClick(const input::TGameActionList& candidates, const int mouseButton, const int modifiers)
{
    const input::TGameActionPtr action = m_mapper.map(candidates, VipWidgetContext, static_cast<Qt::MouseButton>(mouseButton), modifiers);
    if (!action)
        return;
    if (action->type != A::CONTEXT_MENU) {
        input::publish(action);
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); !entries.empty())
        menu->showGameActions(entries, VipWidgetContext, m_mapper);
}

void TVipWidgetController::onVipDoubleClicked(const QString& VipName) { input::publish(makeNamed(A::OPEN_MESSAGE_TO_CHAT_CHANNEL, VipName)); }

void TVipWidgetController::onVipOrGroupClicked(const QString& VipName, const QString& GroupName, const int MouseButtonRaw,
                                               const int KeyboardModifiersRaw)
{
    runClick(m_provider.gameActions(VipName, GroupName, m_proxy->hideOffline(), !m_proxy->showGroups()), MouseButtonRaw,
             KeyboardModifiersRaw);
}

void TVipWidgetController::onWidgetClicked(const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    onVipOrGroupClicked({}, {}, MouseButtonRaw, KeyboardModifiersRaw);
}

void TVipWidgetController::onPlayerNameToAddToVipListReceived(const QString& PlayerName)
{
    input::publish(makeNamed(A::ADD_NEW_VIP, PlayerName));
}

void TVipWidgetController::onGroupNameToAddToVipListReceived(const QString& GroupName)
{
    input::publish(makeNamed(A::ADD_NEW_VIP_GROUP, GroupName));
}

void TVipWidgetController::onGroupNameEditedReceived(const QString& GroupName)
{
    input::publish(makeNamed(A::REQUEST_VIP_GROUP_UPDATE, GroupName, 0, m_editedGroup.id));
    m_editedGroup = {};
}

void TVipWidgetController::openAddGroupDialog()
{
    const int left = m_storage->groupsLeft();
    if (left > 0) {
        m_addGroupDialog->setCaption(qtTrId("add_new_vip_group_caption").arg(left));
        m_addGroupDialog->setText({});
        m_addGroupDialog->dialog()->show();
        return;
    }
    m_messageDialog->setCloseOnButtonClicked(true);
    m_messageDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                           QKeySequence(QStringLiteral("Return"))) });
    m_messageDialog->showMessage(qtTrId("no_more_userdefined_groups_caption"), qtTrId("no_more_userdefined_groups_text"));
}

void TVipWidgetController::openEditVipDialog(const QString& vipName)
{
    if (const std::optional<creatures::TVipStorage::Vip> vip = m_storage->vipByName(vipName))
        m_editVipDialog->open(*vip, m_storage->groupMap());
}

void TVipWidgetController::openEditGroupDialog(const QString& groupName)
{
    const std::optional<creatures::TVipStorage::Group> group = m_storage->groupByName(groupName);
    m_editedGroup = group.value_or(creatures::TVipStorage::Group{});
    if (!group)
        return;
    m_editGroupDialog->setText(groupName);
    m_editGroupDialog->dialog()->show();
}

void TVipWidgetController::onVipDataEdited(const creatures::TVipStorage::Vip& VipData)
{
    auto action = input::mutableClone(*input::makeAction(A::REQUST_VIP_DATA_UPDATE));
    action->instance = static_cast<int>(VipData.id);
    action->text = VipData.description;
    action->parameter = VipData.iconId;
    action->enable = VipData.notifyLogin;
    action->vipGroups = VipData.groups;
    input::publish(action);
}

void TVipWidgetController::onLoginOrLogoutStateUpdate(const int state)
{
    using Sound = cip::tibia::qmlcomponents::TSoundHelperService;
    if (state != creatures::TVipStorage::LoggedIn && state != creatures::TVipStorage::LoggedOut)
        return;
    auto action = input::mutableClone(*input::makeAction(A::SOUND_TRIGGER));
    action->parameter = state == creatures::TVipStorage::LoggedIn ? Sound::VIP_LIST_LOGIN : Sound::VIP_LIST_LOGOUT;
    input::publish(action);
}

bool TVipWidgetController::handleAction(const input::TGameAction& action)
{
    switch (action.type) {
        case A::OPEN_ADD_VIP_DIALOG:
            m_addVipDialog->setText({});
            m_addVipDialog->dialog()->show();
            return true;
        case A::OPEN_EDIT_VIP_DIALOG:
            openEditVipDialog(action.text);
            return true;
        case A::OPEN_ADD_VIP_GROUP_DIALOG:
            openAddGroupDialog();
            return true;
        case A::OPEN_EDIT_VIP_GROUP_DIALOG:
            openEditGroupDialog(action.text);
            return true;
        case A::SORT_VIPS_BY_NAME:
            m_proxy->sortBy(TVipWidgetDataModelSortFilterProxy::ByName);
            return true;
        case A::SORT_VIPS_BY_ICON:
            m_proxy->sortBy(TVipWidgetDataModelSortFilterProxy::ByIcon);
            return true;
        case A::SORT_VIPS_BY_STATUS:
            m_proxy->sortBy(TVipWidgetDataModelSortFilterProxy::ByStatus);
            return true;
        case A::HIDE_OFFLINE_VIPS:
            m_proxy->setHideOffline(true, false);
            return true;
        case A::SHOW_OFFLINE_VIPS:
            m_proxy->setHideOffline(false, false);
            return true;
        case A::SHOW_VIPS_GROUPS:
            m_proxy->setShowGroups(true, false);
            return true;
        case A::HIDE_VIPS_GROUPS:
            m_proxy->setShowGroups(false, false);
            return true;
        default:
            return false;
    }
}

TVipWidgetManager::TVipWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
    , m_storage(creatures::TVipStorage::instance())
    , m_exivaOptions(new exivaoptions::TExivaOptionsStorage(this))
{
    for (const A type : WidgetActions)
        input::setActionHandler(type, [this](const input::TGameAction& action) { return m_controller && m_controller->handleAction(action); });
}

TVipWidgetManager::~TVipWidgetManager()
{
    for (const A type : WidgetActions)
        input::setActionHandler(type, nullptr);
    delete m_controller;
}

void TVipWidgetManager::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (m_controller)
        m_controller->setSidebarOptions(options);
}

void TVipWidgetManager::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    if (m_controller)
        m_controller->setControlScheme(controlScheme);
}

void TVipWidgetManager::setWorldType(const int worldType)
{
    m_worldType = worldType;
    if (m_controller)
        m_controller->setExivaOptions(m_exivaOptions, worldType);
}

TSidebarWidget* TVipWidgetManager::open()
{
    if (!m_controller) {
        m_controller = new TVipWidgetController(m_storage, m_gameWindowItem, m_engine);
        m_controller->setControlScheme(m_controlScheme);
        m_controller->setExivaOptions(m_exivaOptions, m_worldType);
        m_controller->setSidebarOptions(m_options);
    }
    return m_controller->widget();
}

void TVipWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TVipWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
