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
/// CIP EWorldType: TVipWidgetGameActionProvider offers the exiva
/// whitelist entries on optional PvP worlds only.
constexpr int WorldTypeOptionalPvp = 1;

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

/// CIP 's status order: online (1, 3, 4) before the rest.
int statusRank(const int state) { return state == 1 || state == 3 || state == 4 ? 0 : 1; }

input::TGameActionPtr makeNamed(const A type, const QString& text, const int instance = 0, const int parameter = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->text = text;
    action->instance = instance;
    action->parameter = parameter;
    return action;
}

}

// b1/e2a7be0a53f30566
TVipWidgetDataModel::TVipWidgetDataModel(TVipStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TVipWidgetDataModel(parent)
    , m_storage(storage)
{
    connect(storage, &TVipStorage::vipsChanged, this, &TVipWidgetDataModel::onVipGroupDataChanged);
    connect(storage, &TVipStorage::groupsChanged, this, &TVipWidgetDataModel::onVipGroupDataChanged);
    connect(storage, &TVipStorage::vipStateChanged, this, &TVipWidgetDataModel::onVipStateChanged);
    onVipGroupDataChanged();
}

// b1/6dc215293452750e
void TVipWidgetDataModel::onVipStateChanged(const uint32_t id, const int previousState, const int state)
{
    TVipRenderInfo*& info = m_renderInfos[id];
    if (!info) {
        info = new TVipRenderInfo(id, this);
        connect(info, &TVipRenderInfo::vipRenderInfoChanged, this, [this](const uint32_t characterId) {
            if (TVipRenderInfo* render = m_renderInfos.value(characterId); !render || !render->highlighted())
                m_highlighted.remove(characterId);
            emitHighlightChanged(characterId);
        });
    }
    info->clearHighlight();
    if (isOffline(previousState) && isOnline(state)) {
        m_highlighted.insert(id);
        info->startHighlight(HighlightMs);
        emitHighlightChanged(id);
    } else {
        m_highlighted.remove(id);
        emitHighlightChanged(id);
    }
}

void TVipWidgetDataModel::emitHighlightChanged(const uint32_t id)
{
    for (size_t row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].id == id) {
            const QModelIndex changed = index(static_cast<int>(row), 0);
            Q_EMIT dataChanged(changed, changed, { VipHighlightedRole });
        }
    }
}

int TVipWidgetDataModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_rows.size()); }

// b1/783f0b1c97dba067
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
    return {
        { VipNameRole, "vipName" },   { VipStateRole, "vipState" },     { VipHighlightedRole, "vipHighlighted" },
        { VipIconIdRole, "vipIconId" }, { GroupNameRole, "groupName" }, { PrimaryRole, "primary" },
    };
}

// b1/53e62a67e3ee7e73
void TVipWidgetDataModel::onVipGroupDataChanged()
{
    beginResetModel();
    m_rows.clear();
    const std::vector<TVipStorage::Group>& groups = m_storage->groups();
    for (const TVipStorage::Vip& vip : m_storage->vips()) {
        if (vip.groups.empty()) {
            m_rows.push_back({ vip.id, vip.name, vip.state, vip.iconId, ungroupedName(), true });
            continue;
        }
        bool primary = true;
        for (const uint8_t groupId : vip.groups) {
            QString groupName = ungroupedName();
            const auto it = std::find_if(groups.begin(), groups.end(), [groupId](const TVipStorage::Group& group) { return group.id == groupId; });
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
    Q_UNUSED(characterId);
    onVipGroupDataChanged();
}

void TVipWidgetDataModel::onVipRenderInfoChanged(const uint32_t characterId) { emitHighlightChanged(characterId); }

TVipWidgetDataModelSortFilterProxy::TVipWidgetDataModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TVipWidgetDataModelSortFilterProxy(parent)
{
    setDynamicSortFilter(true);
    sort(0);
}

bool TVipWidgetDataModelSortFilterProxy::showGroups() const { return m_showGroups; }

// b1/88bb8a3e16a4e1f0
void TVipWidgetDataModelSortFilterProxy::setHideOffline(const bool hide, const bool silent)
{
    m_hideOffline = hide;
    invalidate();
    if (!silent)
        Q_EMIT configChanged();
}

void TVipWidgetDataModelSortFilterProxy::setShowGroups(const bool show, const bool silent)
{
    m_showGroups = show;
    invalidate();
    Q_EMIT showGroupsChanged();
    if (!silent)
        Q_EMIT configChanged();
}

// b1/5182c38bc22f500a
void TVipWidgetDataModelSortFilterProxy::setSortOrder(const std::vector<int>& order)
{
    m_sortOrder = order;
    invalidate();
}

// b1/066cc1100f6aeadb
void TVipWidgetDataModelSortFilterProxy::sortBy(const int key)
{
    std::erase(m_sortOrder, key);
    m_sortOrder.insert(m_sortOrder.begin(), key);
    invalidate();
    Q_EMIT sortOrderChanged();
    Q_EMIT configChanged();
}

// b1/ca45fec93b8a56e4
bool TVipWidgetDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_hideOffline && isOffline(index.data(TVipWidgetDataModel::VipStateRole).toInt()))
        return false;
    return m_showGroups || index.data(TVipWidgetDataModel::PrimaryRole).toBool();
}

// b1/26cde54b3894f746
bool TVipWidgetDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (m_showGroups) {
        // CIP : the ungrouped section last.
        const QString leftGroup = left.data(TVipWidgetDataModel::GroupNameRole).toString();
        const QString rightGroup = right.data(TVipWidgetDataModel::GroupNameRole).toString();
        if (leftGroup != rightGroup) {
            const QString ungrouped = ungroupedName();
            if (leftGroup == ungrouped || rightGroup == ungrouped)
                return rightGroup == ungrouped;
            return leftGroup.compare(rightGroup, Qt::CaseInsensitive) < 0;
        }
    }
    const QString leftName = left.data(TVipWidgetDataModel::VipNameRole).toString();
    const QString rightName = right.data(TVipWidgetDataModel::VipNameRole).toString();
    for (const int key : m_sortOrder) {
        int order = 0;
        switch (key) {
            case ByStatus:
                order = statusRank(left.data(TVipWidgetDataModel::VipStateRole).toInt()) -
                        statusRank(right.data(TVipWidgetDataModel::VipStateRole).toInt());
                break;
            case ByIcon:
                order = left.data(TVipWidgetDataModel::VipIconIdRole).toInt() - right.data(TVipWidgetDataModel::VipIconIdRole).toInt();
                break;
            case ByName:
                order = leftName.compare(rightName, Qt::CaseInsensitive);
                break;
            default:
                break;
        }
        if (order != 0)
            return order < 0;
    }
    // CIP leaves rows equal on every key in the source order; the name keeps the list stable here.
    return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
}

TVipWidgetController::TVipWidgetController(TVipStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_model(new TVipWidgetDataModel(storage, this))
    , m_proxy(new TVipWidgetDataModelSortFilterProxy(this))
    , m_addVipDialog(new TGenericEnterTextDialogController(this))
    , m_addGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editGroupDialog(new TGenericEnterTextDialogController(this))
    , m_editVipDialog(new TEditVipDialogController(storage, this))
    , m_messageDialog(new TMessageDialogController(this))
{
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
void TVipWidgetController::onVipDataEdited { /* TVipStorage signals rebuild the list model. */ }
void TVipWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }
void TVipWidgetController::onWidgetQuickItemDestroyed() {}

void TVipWidgetController::handleGameAction()
{
    // CIP moc shared_ptr dispatch; qtclient routes via setActionHandler → handleAction.
}

void TVipWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    readOptions();
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

input::TGameActionList TVipWidgetController::candidates(const QString& vipName, const QString& groupName) const
{
    input::TGameActionList result{ input::makeAction(A::CONTEXT_MENU), input::makeAction(A::OPEN_ADD_VIP_DIALOG),
                                   input::makeAction(A::SORT_VIPS_BY_NAME), input::makeAction(A::SORT_VIPS_BY_ICON),
                                   input::makeAction(A::SORT_VIPS_BY_STATUS),
                                   input::makeAction(m_proxy->hideOffline() ? A::SHOW_OFFLINE_VIPS : A::HIDE_OFFLINE_VIPS),
                                   input::makeAction(m_proxy->showGroups() ? A::HIDE_VIPS_GROUPS : A::SHOW_VIPS_GROUPS) };
    if (tibia::game::localPlayer() && tibia::game::localPlayerIsPremium())
        result.push_back(input::makeAction(A::OPEN_ADD_VIP_GROUP_DIALOG));
    if (!groupName.isEmpty()) {
        if (const std::optional<TVipStorage::Group> group = m_storage->groupByName(groupName); group && group->editable) {
            result.push_back(makeNamed(A::OPEN_EDIT_VIP_GROUP_DIALOG, group->name, 0, group->id));
            result.push_back(makeNamed(A::REMOVE_VIP_GROUP, group->name, 0, group->id));
        }
    }
    if (!vipName.isEmpty()) {
        if (const std::optional<TVipStorage::Vip> vip = m_storage->vipByName(vipName)) {
            const int id = static_cast<int>(vip->id);
            result.push_back(makeNamed(A::OPEN_EDIT_VIP_DIALOG, vip->name, id));
            result.push_back(makeNamed(A::REMOVE_VIP, vip->name, id));
            result.push_back(makeNamed(A::COPY_CREATURE_NAME_TO_CLIPBOARD, vip->name, id));
            if (vip->state == 1 || vip->state == 4)
                result.push_back(makeNamed(A::OPEN_MESSAGE_TO_CHAT_CHANNEL, vip->name));
            result.push_back(makeNamed(A::REPORT_NAME, vip->name));
            if (m_exivaOptions && m_worldType == WorldTypeOptionalPvp)
                result.push_back(makeNamed(m_exivaOptions->isCharacterWhitelisted(vip->name) ? A::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST
                                                                                            : A::EXIVA_ADD_CHARACTER_TO_WHITELIST,
                                           vip->name, id));
        }
    }
    return result;
}

void TVipWidgetController::setExivaOptions(exivaoptions::TExivaOptionsStorage* exivaOptions, const int worldType)
{
    m_exivaOptions = exivaOptions;
    m_worldType = worldType;
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

// b1/c4316c8600c4ccaf
void TVipWidgetController::onVipDoubleClicked(const QString& VipName) { input::publish(makeNamed(A::OPEN_MESSAGE_TO_CHAT_CHANNEL, VipName)); }

// b1/a034e79693c33847
void TVipWidgetController::onVipOrGroupClicked(const QString& VipName, const QString& GroupName, const int MouseButtonRaw,
                                               const int KeyboardModifiersRaw)
{
    runClick(candidates(VipName, GroupName), MouseButtonRaw, KeyboardModifiersRaw);
}

// b1/5f6e53e1a2e6fcad
void TVipWidgetController::onWidgetClicked(const int MouseButtonRaw, const int KeyboardModifiersRaw)
{
    runClick(candidates({}, {}), MouseButtonRaw, KeyboardModifiersRaw);
}

// b1/e808c2095d54a1f7
void TVipWidgetController::onPlayerNameToAddToVipListReceived(const QString& PlayerName)
{
    input::publish(makeNamed(A::ADD_NEW_VIP, PlayerName));
}

// b1/67e26866a966dcc7
void TVipWidgetController::onGroupNameToAddToVipListReceived(const QString& GroupName)
{
    input::publish(makeNamed(A::ADD_NEW_VIP_GROUP, GroupName));
}

// b1/048f73c5e7920d78
void TVipWidgetController::onGroupNameEditedReceived(const QString& GroupName)
{
    if (m_editedGroupId < 0)
        return;
    input::publish(makeNamed(A::REQUEST_VIP_GROUP_UPDATE, GroupName, 0, m_editedGroupId));
    m_editedGroupId = -1;
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

// b1/19675bba2e526bba
bool TVipWidgetController::handleAction(const input::TGameAction& action)
{
    switch (action.type) {
        case A::OPEN_ADD_VIP_DIALOG:
            // CIP chat/hotkey can open the dialog with a prefilled speaker name
            // (action.text); empty text is the blank Add VIP path.
            m_addVipDialog->setText(action.text);
            m_addVipDialog->dialog()->show();
            return true;
        case A::OPEN_EDIT_VIP_DIALOG: {
            std::optional<TVipStorage::Vip> vip = m_storage->vipByName(action.text);
            if (vip)
                m_editVipDialog->open(*vip);
            return true;
        }
        case A::OPEN_ADD_VIP_GROUP_DIALOG:
            openAddGroupDialog();
            return true;
        case A::OPEN_EDIT_VIP_GROUP_DIALOG: {
            const std::optional<TVipStorage::Group> group = m_storage->groupByName(action.text);
            if (!group || !group->editable)
                return true;
            m_editedGroupId = group->id;
            m_editGroupDialog->setText(group->name);
            m_editGroupDialog->dialog()->show();
            return true;
        }
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
    , m_storage(new TVipStorage(this))
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

void TVipWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TVipWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
