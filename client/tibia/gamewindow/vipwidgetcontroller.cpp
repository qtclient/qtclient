#include "vipwidgetcontroller.h"
#include "game/gamesessionbase.h"

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

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr auto OptionsKey = "vipWidgetOptions";
/// CIP : TGameActionMapper context of the VIP widget.
constexpr int VipWidgetContext = 7;
/// CIP : the add/edit name dialogs' input length.
constexpr uint MaxNameLength = 29;

/// CIP 's context menu order.
constexpr A ContextMenuOrder[] = {
    A::OPEN_EDIT_VIP_DIALOG, A::REMOVE_VIP, A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::OPEN_ADD_VIP_DIALOG, A::CONTEXT_MENU_SEPARATOR,
    A::OPEN_EDIT_VIP_GROUP_DIALOG, A::REMOVE_VIP_GROUP, A::OPEN_ADD_VIP_GROUP_DIALOG, A::CONTEXT_MENU_SEPARATOR, A::SORT_VIPS_BY_NAME,
    A::SORT_VIPS_BY_ICON, A::SORT_VIPS_BY_STATUS, A::HIDE_OFFLINE_VIPS, A::SHOW_OFFLINE_VIPS, A::HIDE_VIPS_GROUPS, A::SHOW_VIPS_GROUPS,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_NAME, A::CONTEXT_MENU_SEPARATOR, A::EXIVA_ADD_CHARACTER_TO_WHITELIST,
    A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST, A::CONTEXT_MENU_SEPARATOR, A::COPY_CREATURE_NAME_TO_CLIPBOARD,
};

/// The game actions the widget handles itself. // not-a-cip-table
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

}

TVipWidgetDataModel::TVipWidgetDataModel(QObject* parent)
    : cip::tibia::gamewindow::TVipWidgetDataModel(parent)
{
}

void TVipWidgetDataModel::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    setVipStorage(session ? session->vipStorage() : nullptr);
}

void TVipWidgetDataModel::setVipStorage(const std::shared_ptr<creatures::TVipStorage>& VipStorage)
{
    m_connectionManager.disconnectAll();
    m_vipStorage = VipStorage;
    m_groups.clear();
    if (creatures::TVipStorage* storage = m_vipStorage.get()) {
        m_connectionManager.add(
            connect(storage, &creatures::TVipStorage::vipDataChangedOrAdded, this, &TVipWidgetDataModel::onVipDataChangedOrAdded));
        m_connectionManager.add(connect(storage, &creatures::TVipStorage::vipStateChanged, this, &TVipWidgetDataModel::onVipStateChanged));
        m_connectionManager.add(connect(storage, &creatures::TVipStorage::vipDataRemoved, this, &TVipWidgetDataModel::onVipRemoved));
        m_connectionManager.add(
            connect(storage, &creatures::TVipStorage::vipGroupDataChanged, this, &TVipWidgetDataModel::onVipGroupDataChanged));
    }
    rebuild();
}

void TVipWidgetDataModel::rebuild()
{
    removeAllRows();
    if (!m_vipStorage)
        return;
    m_groups = m_vipStorage->groupMap();
    addVips(m_vipStorage->vips());
}

void TVipWidgetDataModel::removeAllRows()
{
    if (rowCount() == 0)
        return;
    beginRemoveRows({}, 0, rowCount() - 1);
    m_vipRenderInfos.clear();
    endRemoveRows();
}

void TVipWidgetDataModel::addVips(const std::vector<creatures::TVipStorage::Vip>& Vips)
{
    for (const creatures::TVipStorage::Vip& vip : Vips) {
        if (vip.groups.empty()) {
            beginInsertRows({}, rowCount(), rowCount());
            addVipRenderInfo(vip, ungroupedName(), true);
        } else {
            // The announced range is one row longer than the rows added.
            beginInsertRows({}, rowCount(), rowCount() + static_cast<int>(vip.groups.size()));
            bool firstGroupEntry = true;
            for (const uint8_t groupId : vip.groups) {
                QString groupName = ungroupedName();
                if (const auto it = m_groups.find(groupId); it != m_groups.end())
                    groupName = it->second.name;
                addVipRenderInfo(vip, groupName, firstGroupEntry);
                firstGroupEntry = false;
            }
        }
        endInsertRows();
    }
}

void TVipWidgetDataModel::addVipRenderInfo(const creatures::TVipStorage::Vip& VipData, const QString& GroupName, const bool FirstGroupEntry)
{
    const auto renderInfo = std::make_shared<TVipRenderInfo>(VipData, GroupName, FirstGroupEntry);
    m_vipRenderInfos.push_back(renderInfo);
    connect(renderInfo.get(), &TVipRenderInfo::vipRenderInfoChanged, this, &TVipWidgetDataModel::onVipRenderInfoChanged);
}

void TVipWidgetDataModel::onVipStateChanged(const uint32_t CharacterID, const int State)
{
    std::vector<std::shared_ptr<TVipRenderInfo>> renderInfos;
    for (const std::shared_ptr<TVipRenderInfo>& renderInfo : m_vipRenderInfos) {
        if (renderInfo->m_characterId == CharacterID)
            renderInfos.push_back(renderInfo);
    }
    for (const std::shared_ptr<TVipRenderInfo>& renderInfo : renderInfos) {
        if (!renderInfo || renderInfo->m_state == State)
            continue;
        renderInfo->m_highlightedTimer.stop();
        if (isOffline(renderInfo->m_state) && isOnline(State)) {
            renderInfo->m_highlighted = true;
            renderInfo->m_highlightedTimer.start();
        }
        renderInfo->m_state = State;
        Q_EMIT renderInfo->vipRenderInfoChanged(renderInfo->m_characterId);
    }
}

int TVipWidgetDataModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_vipRenderInfos.size());
}

QVariant TVipWidgetDataModel::data(const QModelIndex& index, const int role) const
{
    if (index.row() < 0 || index.column() < 0 || !index.model() || index.row() >= rowCount())
        return {};
    const std::shared_ptr<TVipRenderInfo> renderInfo = m_vipRenderInfos[static_cast<size_t>(index.row())];
    switch (role) {
        case VipNameRole:
            return renderInfo->name();
        case VipStateRole:
            return renderInfo->state();
        case VipHighlightedRole:
            return renderInfo->highlighted();
        case VipIconIdRole:
            return static_cast<int>(renderInfo->iconId());
        case GroupNameRole:
            return renderInfo->groupName();
        case PrimaryRole:
            return renderInfo->firstGroupEntry();
        default:
            return {};
    }
}

QHash<int, QByteArray> TVipWidgetDataModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[VipNameRole] = "vipName";
    roles[VipStateRole] = "vipState";
    roles[VipHighlightedRole] = "vipHighlighted";
    roles[VipIconIdRole] = "vipIconId";
    roles[GroupNameRole] = "groupName";
    return roles;
}

void TVipWidgetDataModel::onVipGroupDataChanged() { rebuild(); }

void TVipWidgetDataModel::onVipDataChangedOrAdded() { rebuild(); }

void TVipWidgetDataModel::onVipRemoved(const uint32_t CharacterID)
{
    for (;;) {
        const auto it = std::find_if(m_vipRenderInfos.begin(), m_vipRenderInfos.end(),
                                     [CharacterID](const std::shared_ptr<TVipRenderInfo>& renderInfo) { return renderInfo->m_characterId == CharacterID; });
        if (it == m_vipRenderInfos.end())
            return;
        const int row = static_cast<int>(it - m_vipRenderInfos.begin());
        beginRemoveRows({}, row, row);
        m_vipRenderInfos.erase(it);
        endRemoveRows();
    }
}

/// Every row of the VIP changed: name, highlight, state, icon and group.
void TVipWidgetDataModel::onVipRenderInfoChanged(const uint32_t CharacterID)
{
    static const QList<int> ChangedRoles = { VipNameRole, VipHighlightedRole, VipStateRole, VipIconIdRole, GroupNameRole };
    std::vector<int> rows;
    for (size_t row = 0; row < m_vipRenderInfos.size(); ++row) {
        if (m_vipRenderInfos[row]->m_characterId == CharacterID)
            rows.push_back(static_cast<int>(row));
    }
    for (const int row : rows) {
        const QModelIndex changed = createIndex(row, 0);
        Q_EMIT dataChanged(changed, changed, ChangedRoles);
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

TVipWidgetController::TVipWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_model(new TVipWidgetDataModel(this))
    , m_proxy(new TVipWidgetDataModelSortFilterProxy(this))
    , m_addVipDialog(new TGenericEnterTextDialogController(this))
    , m_addGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editVipDialog(new TEditVipDialogController(this))
    , m_messageDialog(new TMessageDialogController(this))
{
    connect(m_editVipDialog, &TEditVipDialogController::vipDataUpdated, this, &TVipWidgetController::onVipDataEdited);
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
    // CIP moc shared_ptr dispatch; the manager's router slot hands the actions to handleAction.
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

/// Host: the sidebar options come from the sidebar widgets manager
/// (setSidebarOptions), which also re-reads them when they change.
void TVipWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_addVipDialog->dialog()->close();
    m_addGroupDialog->dialog()->close();
    m_editVipDialog->dialog()->close();
    m_connectionManager.disconnectAll();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_storage = session ? session->vipStorage().get() : nullptr;
    m_provider = session ? session->vipWidgetGameActionProvider() : nullptr;
    if (m_gameSession) {
        if (m_storage)
            m_connectionManager.add(connect(m_storage, &creatures::TVipStorage::loginOrLogoutStateUpdate, this,
                                            &TVipWidgetController::onLoginOrLogoutStateUpdate));
        readOptions();
    }
    m_model->connectToGameSession(GameSession);
}

void TVipWidgetController::runClick(const input::TGameActionList& candidates, const int mouseButton, const int modifiers)
{
    const input::IGameActionPtr action = m_mapper.map(candidates, VipWidgetContext, static_cast<Qt::MouseButton>(mouseButton), modifiers);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT publishGameAction(action);
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); !entries.empty())
        menu->showGameActions(entries, VipWidgetContext, m_mapper);
}

void TVipWidgetController::onVipDoubleClicked(const QString& VipName) { Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenMessageToChatChannel>(VipName)); }

void TVipWidgetController::onVipOrGroupClicked(const QString& VipName, const QString& GroupName, const int MouseButtonRaw,
                                               const int KeyboardModifiersRaw)
{
    if (!m_provider)
        return;
    runClick(m_provider->gameActions(VipName, GroupName, m_proxy->hideOffline(), !m_proxy->showGroups()), MouseButtonRaw,
             KeyboardModifiersRaw);
}

void TVipWidgetController::onWidgetClicked(const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    onVipOrGroupClicked({}, {}, MouseButtonRaw, KeyboardModifiersRaw);
}

void TVipWidgetController::onPlayerNameToAddToVipListReceived(const QString& PlayerName)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionAddNewVip>(PlayerName));
}

void TVipWidgetController::onGroupNameToAddToVipListReceived(const QString& GroupName)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionAddNewVipGroup>(GroupName));
}

void TVipWidgetController::onGroupNameEditedReceived(const QString& GroupName)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionRequestVipGroupUpdate>(
        input::TGameActionRequestVipGroupUpdate::TVipGroup{ m_editedGroup.id, GroupName, m_editedGroup.editable }));
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
    m_messageDialog->setButtons(okDialogButtons());
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
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionRequestVipDataUpdate>(
        VipData.id, VipData.description, static_cast<uint8_t>(VipData.iconId), VipData.notifyLogin, VipData.groups));
}

void TVipWidgetController::onLoginOrLogoutStateUpdate(const int state)
{
    using Sound = cip::tibia::qmlcomponents::TSoundHelperService;
    if (state != creatures::TVipStorage::LoggedIn && state != creatures::TVipStorage::LoggedOut)
        return;
    auto action = std::make_shared<input::gameactions::TGameActionSoundTrigger>(
        input::gameactions::TGameActionSoundTrigger::TSoundEffect{ state == creatures::TVipStorage::LoggedIn ? Sound::VIP_LIST_LOGIN : Sound::VIP_LIST_LOGOUT });
    Q_EMIT publishGameAction(action);
}

bool TVipWidgetController::handleAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
        case A::OPEN_ADD_VIP_DIALOG:
            m_addVipDialog->setText({});
            m_addVipDialog->dialog()->show();
            return true;
        case A::OPEN_EDIT_VIP_DIALOG:
            if (const auto* vip = input::gameActionAs<input::TGameActionVip>(&action))
                openEditVipDialog(vip->vipName());
            return true;
        case A::OPEN_ADD_VIP_GROUP_DIALOG:
            openAddGroupDialog();
            return true;
        case A::OPEN_EDIT_VIP_GROUP_DIALOG:
            if (const auto* group = input::gameActionAs<input::TGameActionVipGroup>(&action))
                openEditGroupDialog(group->groupName());
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
{
    for (const A type : WidgetActions)
        m_gameActions.on(type, [this](const input::IGameAction& action) { return m_controller && m_controller->handleAction(action); });
}

TVipWidgetManager::~TVipWidgetManager()
{
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

void TVipWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_storage = session ? session->vipStorage().get() : nullptr;
    if (!m_controller)
        return;
    m_controller->connectToGameSession(GameSession);
}

TSidebarWidget* TVipWidgetManager::open()
{
    if (!m_controller) {
        m_controller = new TVipWidgetController(m_gameWindowItem, m_engine);
        connect(m_controller, &TVipWidgetController::publishGameAction, this, &TVipWidgetManager::publishGameAction);
        m_controller->connectToGameSession(m_gameSession);
        m_controller->setControlScheme(m_controlScheme);
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


void TVipWidgetManager::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    m_gameActions.handle(pGameAction);
}

}
