#include "battlelistcontroller.h"
#include "colorhelper.h"

#include "chat/chatchannel.h"
#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "creatures/battlelistgameactionprovider.h"
#include "creatures/creaturestorage.h"
#include "creatures/creaturesciptypes.h"
#include "game/gameciptypes.h"
#include "game/currentgamesession.h"
#include "protocol/clientmessages.h"
#include "game/gamesessionbase.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"
#include "input/genericobjectgameactionprovider.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapgameactionprovider.h"

#include <client/outfit.h>
#include <client/staticdata.h>
#include "protocol/protocolcodes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"

#include <QGuiApplication>
#include <QQuickItem>

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <tibia/game/gamenotify.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Filter = BattleListOptions::Filter;
using Cursor = TMouseCursorController;

constexpr int UpdateIntervalMs = 100;
/// CIP : the mapper context of a battle list and the party list.
constexpr int BattleListContext = 1;
constexpr int PartyListContext = 2;

/// The order of a battle list entry's context menu.
constexpr A ContextMenuOrder[] = {
    A::START_TALKING_TO_NPC, A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::FOLLOW_CREATURE, A::STOP_FOLLOWING_CREATURE, A::LOOK,
    A::LOOK_AT_CREATURE, A::CONTEXT_MENU_SEPARATOR, A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, A::ADD_NEW_VIP,
    A::IGNORE_NAME, A::UNIGNORE_NAME, A::INVITE_TO_PARTY, A::JOIN_PARTY, A::REVOKE_PARTY_INVITATION, A::PASS_PARTY_LEADERSHIP, A::JOIN_AGGRESSION,
    A::INSPECT_PLAYER, A::ASK_TO_INSPECT_PLAYER, A::INVITE_TO_INSPECT_PLAYER, A::ALLOW_INSPECT_PLAYER, A::REVOKE_INSPECT_PLAYER,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_NAME, A::REPORT_BOT, A::REPORT_HIRELING_NAME, A::CONTEXT_MENU_SEPARATOR,
    A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
};

/// EBattleListWidgetFilterKey's keys and the sort keys.
constexpr int FilterCount = 12;
constexpr int SortCount = 8;
/// The configuration menu's sort entries, in Sort order.
constexpr const char* SortMenuEntries[] = {
    "battlelist_configure_menuentry_sortby_age_ascending",       "battlelist_configure_menuentry_sortby_age_descending",
    "battlelist_configure_menuentry_sortby_distance_ascending",  "battlelist_configure_menuentry_sortby_distance_descending",
    "battlelist_configure_menuentry_sortby_hitpoints_ascending", "battlelist_configure_menuentry_sortby_hitpoints_descending",
    "battlelist_configure_menuentry_sortby_name_ascending",      "battlelist_configure_menuentry_sortby_name_descending",
};

using TCreature = creatures::TCreature;
using TCreaturePtr = std::shared_ptr<TCreature>;

/// A battle list entry's actions: the session's battle list provider's.
input::TGameActionList battleListGameActions(const uint32_t creatureId)
{
    const game::TGameSessionBase* session = game::currentGameSession();
    const std::shared_ptr<creatures::TBattleListGameActionProvider> provider =
        session ? session->battleListGameActionProvider() : nullptr;
    return provider ? provider->battleListGameActions(creatureId) : input::TGameActionList{};
}

/// The first of the actions with `type`, or none.
input::IGameActionPtr findAction(const input::TGameActionList& actions, const A type)
{
    for (const input::IGameActionPtr& action : actions) {
        if (action && action->gameActionType() == type)
            return action;
    }
    return nullptr;
}

}

// ---------------------------------------------------------------------------

QList<int> TBattleListDataModel::changedRoles(const BattleListEntry& current, const BattleListEntry& incoming)
{
    QList<int> roles;
    roles.reserve(15);
    if (current.creatureId != incoming.creatureId)
        roles.append(0x101);
    if (current.name != incoming.name)
        roles.append(0x102);
    if (current.temporarySelected != incoming.temporarySelected)
        roles.append(0x114);
    // The bar's colour follows its percentage.
    if (current.healthPercent != incoming.healthPercent) {
        roles.append(0x103);
        roles.append(0x104);
    }
    if (current.manaPercent != incoming.manaPercent)
        roles.append(0x105);
    if (current.showMana != incoming.showMana)
        roles.append(0x106);
    if (current.frameColors != incoming.frameColors)
        roles.append(0x107);
    if (current.statusIcons != incoming.statusIcons)
        roles.append(0x108);
    if (current.outfitId != incoming.outfitId)
        roles.append(0x109);
    if (current.outfitIsObject != incoming.outfitIsObject)
        roles.append(0x10a);
    if (current.headColor != incoming.headColor)
        roles.append(0x10b);
    if (current.torsoColor != incoming.torsoColor)
        roles.append(0x10c);
    if (current.legsColor != incoming.legsColor)
        roles.append(0x10d);
    if (current.detailColor != incoming.detailColor)
        roles.append(0x10e);
    if (current.firstAddOn != incoming.firstAddOn)
        roles.append(0x10f);
    if (current.secondAddOn != incoming.secondAddOn)
        roles.append(0x110);
    if (current.tutorialMarkerId != incoming.tutorialMarkerId)
        roles.append(0x111);
    if (current.visibleOrder != incoming.visibleOrder)
        roles.append(0x112);
    if (current.distance != incoming.distance)
        roles.append(0x113);
    if (current.inViewport != incoming.inViewport)
        roles.append(0x115);
    if (current.inSharedXpDistance != incoming.inSharedXpDistance)
        roles.append(0x116);
    if (current.fiendish != incoming.fiendish)
        roles.append(0x117);
    if (current.leader() != incoming.leader())
        roles.append(0x118);
    if (current.leaderMinion() != incoming.leaderMinion())
        roles.append(0x119);
    if (current.vocation != incoming.vocation)
        roles.append(0x11a);
    return roles;
}

void TBattleListDataModel::setEntries(std::vector<BattleListEntry> entries)
{
    // The rows stay ordered by creature id (the proxy sorts the view): walk
    // the new entries in that order, keeping, updating, inserting and removing
    // rows.
    std::vector<size_t> order(entries.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(),
              [&entries](const size_t a, const size_t b) { return entries[a].creatureId < entries[b].creatureId; });

    size_t row = 0;
    for (size_t next = 0; next < order.size();) {
        BattleListEntry& incoming = entries[order[next]];
        if (row < m_entries.size()) {
            BattleListEntry& current = m_entries[row];
            if (current.creatureId == incoming.creatureId) {
                const QList<int> roles = changedRoles(current, incoming);
                if (current != incoming)
                    current = std::move(incoming);
                if (!roles.isEmpty()) {
                    const QModelIndex changed = index(static_cast<int>(row));
                    Q_EMIT dataChanged(changed, changed, roles);
                }
            } else if (current.creatureId < incoming.creatureId) {
                beginRemoveRows({}, static_cast<int>(row), static_cast<int>(row));
                m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(row));
                endRemoveRows();
                continue;
            } else {
                beginInsertRows({}, static_cast<int>(row), static_cast<int>(row));
                m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(row), std::move(incoming));
                endInsertRows();
            }
        } else {
            const int end = rowCount();
            beginInsertRows({}, end, end);
            m_entries.push_back(std::move(incoming));
            endInsertRows();
        }
        ++row;
        ++next;
    }
    if (row < m_entries.size()) {
        beginRemoveRows({}, static_cast<int>(row), rowCount() - 1);
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(row), m_entries.end());
        endRemoveRows();
    }
}

int TBattleListDataModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TBattleListDataModel::data(const QModelIndex& index, const int role) const
{
    if (index.row() < 0 || index.column() < 0 || !index.model())
        return {};
    // Roles 0x101-0x119; playerVocation (0x11a) has a name but no data.
    if (index.row() >= rowCount() || static_cast<uint>(role - 0x101) > 0x18)
        return {};
    const BattleListEntry& e = m_entries[index.row()];
    switch (role) {
        case 0x101: return e.creatureId;
        case 0x102: return e.name;
        case 0x103: return e.healthPercent;
        case 0x104: return e.healthColor;
        case 0x105: return e.manaPercent;
        case 0x106: return e.showMana;
        case 0x107: return e.frameColors;
        case 0x108: return e.statusIcons;
        case 0x109: return e.outfitId;
        case 0x10a: return e.outfitIsObject;
        case 0x10b: return e.headColor;
        case 0x10c: return e.torsoColor;
        case 0x10d: return e.legsColor;
        case 0x10e: return e.detailColor;
        case 0x10f: return e.firstAddOn;
        case 0x110: return e.secondAddOn;
        case 0x111: return e.tutorialMarkerId;
        case 0x112: return e.visibleOrder;
        case 0x113: return e.distance;
        case 0x114: return e.temporarySelected;
        case 0x115: return e.inViewport;
        case 0x116: return e.inSharedXpDistance;
        case 0x117: return e.fiendish;
        case 0x118: return e.leader();
        case 0x119: return e.leaderMinion();
        default: return {};
    }
}

TBattleListDataModelSortFilterProxy::TBattleListDataModelSortFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

void TBattleListDataModelSortFilterProxy::setOptions(const BattleListOptions& options)
{
    if (options == m_options)
        return;
    m_options = options;
    invalidate();
}

void TBattleListDataModelSortFilterProxy::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_creatureStorage.reset();
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get()))
        m_creatureStorage = session->creatureStorage();
    invalidate();
}

bool TBattleListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    creatures::TCreatureStorage* storage = m_creatureStorage.get();
    if (!storage)
        return true;
    const auto creatureId = static_cast<creatures::TCreatureID>(source->data(index, 0x101).toUInt());
    const bool inViewport = source->data(index, 0x115).toBool();
    const TCreaturePtr creature = storage->creatureAt(creatureId);
    if (!creature)
        return true;
    const uint8_t type = creature->creatureType();
    const bool partyMember = creature->isPartyMember();
    const uint8_t emblem = creature->emblem();
    const bool player = type == Proto::CreatureTypePlayer;
    // The party list shows party members only, the other lists the creatures
    // in the viewport only.
    bool hidden = m_options.isPartyView ? !partyMember : !inViewport;
    for (auto it = m_options.filters.begin(); !hidden && it != m_options.filters.end(); ++it) {
        switch (*it) {
            case Filter::HIDE_PLAYERS: hidden = player; break;
            case Filter::HIDE_KNIGHTS: hidden = player && creature->vocation() == 1; break;
            case Filter::HIDE_PALADINS: hidden = player && creature->vocation() == 2; break;
            case Filter::HIDE_DRUIDS: hidden = player && creature->vocation() == 4; break;
            case Filter::HIDE_SORCERERS: hidden = player && creature->vocation() == 3; break;
            case Filter::HIDE_MONKS: hidden = player && creature->vocation() == 5; break;
            case Filter::HIDE_NPCS: hidden = type == Proto::CreatureTypeNpc; break;
            case Filter::HIDE_MONSTERS: hidden = type == Proto::CreatureTypeMonster; break;
            case Filter::HIDE_NON_SKULLED_PLAYERS: hidden = player && creature->skull() == 0; break;
            case Filter::HIDE_PARTY_MEMBERS: hidden = partyMember; break;
            case Filter::HIDE_PLAYER_SUMMONS: hidden = type == Proto::CreatureTypeSummonOwn; break;
            case Filter::HIDE_MEMBERS_OF_OWN_GUILD: hidden = emblem == 1 || emblem == 4; break;
            default: break;
        }
    }
    return !hidden;
}

bool TBattleListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (!sourceModel())
        return false;
    const uint leftAge = left.data(0x112).toUInt();
    const uint rightAge = right.data(0x112).toUInt();
    const uint leftDistance = left.data(0x113).toUInt();
    const uint rightDistance = right.data(0x113).toUInt();
    const float leftHitpoints = left.data(0x103).toFloat();
    const float rightHitpoints = right.data(0x103).toFloat();
    const QString leftName = left.data(0x102).toString();
    const QString rightName = right.data(0x102).toString();
    // The first sort key on which the two differ decides; the creature id
    // breaks ties.
    for (const int key : m_options.sortOrder) {
        switch (key) {
            case BattleListOptions::ByAgeAscending:
                if (leftAge != rightAge)
                    return leftAge < rightAge;
                break;
            case BattleListOptions::ByAgeDescending:
                if (leftAge != rightAge)
                    return leftAge > rightAge;
                break;
            case BattleListOptions::ByDistanceAscending:
                if (leftDistance != rightDistance)
                    return leftDistance < rightDistance;
                break;
            case BattleListOptions::ByDistanceDescending:
                if (leftDistance != rightDistance)
                    return leftDistance > rightDistance;
                break;
            case BattleListOptions::ByHitpointsAscending:
                if (leftHitpoints != rightHitpoints)
                    return leftHitpoints < rightHitpoints;
                break;
            case BattleListOptions::ByHitpointsDescending:
                if (leftHitpoints != rightHitpoints)
                    return leftHitpoints > rightHitpoints;
                break;
            case BattleListOptions::ByNameAscending:
                if (leftName != rightName)
                    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
                break;
            case BattleListOptions::ByNameDescending:
                if (leftName != rightName)
                    return QString::compare(leftName, rightName, Qt::CaseSensitive) > 0;
                break;
            default:
                break;
        }
    }
    return left.data(0x101).toUInt() < right.data(0x101).toUInt();
}

QHash<int, QByteArray> TBattleListDataModel::roleNames() const
{
    return {
        { 0x101, "creatureID" },
        { 0x102, "creatureName" },
        { 0x103, "healthProgressbarPercent" },
        { 0x104, "healthProgressbarColor" },
        { 0x105, "manaProgressbarPercent" },
        { 0x106, "showMana" },
        { 0x107, "frameColorsOuterToInner" },
        { 0x108, "statusIcons" },
        { 0x109, "creatureOutfitId" },
        { 0x10a, "creatureOutfitIsObject" },
        { 0x10b, "creatureHeadColor" },
        { 0x10c, "creatureTorsoColor" },
        { 0x10d, "creatureLegsColor" },
        { 0x10e, "creatureDetailColor" },
        { 0x10f, "creatureFirstAddOn" },
        { 0x110, "creatureSecondAddOn" },
        { 0x111, "tutorialMarkerID" },
        { 0x112, "creatureBecomesVisibleOrder" },
        { 0x113, "distanceToPlayer" },
        { 0x114, "isTemporarySelected" },
        { 0x115, "isInViewport" },
        { 0x116, "isInSharedXPDistance" },
        { 0x117, "isFiendishMonster" },
        { 0x118, "isLeaderMonster" },
        { 0x119, "isLeaderMinionMonster" },
        { 0x11a, "playerVocation" },
    };
}

// ---------------------------------------------------------------------------

TBattleListController::TBattleListController(const int battleListId, TBattleListControllerStorage* storage, TBattleListDataModel* model,
                                             QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_id(battleListId)
    , m_storage(storage)
    , m_proxy(new TBattleListDataModelSortFilterProxy(this))
    , m_renameDialog(new TGenericEnterTextDialogController(this))
{
    m_renameDialog->setCaption(qtTrId("battlelist_rename"));
    m_renameDialog->setDescription(qtTrId("battlelist_rename_text"));
    m_renameDialog->setMaxInputLength(30);
    m_renameDialog->setAllowEmptyText(true);
    m_proxy->setSourceModel(model);
    m_battleListModel = QVariant::fromValue<QObject*>(m_proxy);
    attachSidebarOptions(storage->sidebarOptions());

    widget()->createItem(QStringLiteral("battlelist.qml"));
    if (QQuickItem* item = quickItem()) {
        m_connections.add(connect(item, SIGNAL(currentlyHoveredCreatureChanged(int)), this, SLOT(onCurrentlyHoveredCreatureChanged(int))));
        m_connections.add(connect(item, SIGNAL(creatureClicked(int, int, int)), this, SLOT(onCreatureClicked(int, int, int))));
        m_connections.add(connect(item, SIGNAL(creatureTargetSelected(int)), this, SLOT(onCreatureTargetSelected(int))));
    }
    m_connections.add(connect(m_renameDialog, &TGenericEnterTextDialogController::textEntered, this, &TBattleListController::onNewNameEntered));
}

TBattleListController::~TBattleListController()
{
    m_connections.disconnectAll();
    QObject::disconnect(m_contextMenuClosed);
}

void TBattleListController::handleGameAction()
{
    // The game actions reach the list through the mapper and the action listeners.
}

bool TBattleListController::handleGameActionAtMouseCursor(const input::TGameActionUseAtMouseCursor& action)
{
    if (!m_hoveredCreature)
        return false;
    const auto* target = input::gameActionAs<input::TGameActionTargetSelected>(
        findAction(battleListGameActions(m_hoveredCreature), A::TARGET_SELECTED));
    if (!target || !target->creatureId())
        return false;
    Q_EMIT m_storage->publishGameAction(std::make_shared<input::TGameActionUseOnCreature>(action.context(), target->creatureId()));
    action.setHandled();
    return true;
}

bool TBattleListController::handleGameActionCastSpellAtMouseCursor(const input::TGameActionSpellAtMouseCursor& action)
{
    if (!m_hoveredCreature)
        return false;
    const auto* target = input::gameActionAs<input::TGameActionTargetSelected>(
        findAction(battleListGameActions(m_hoveredCreature), A::TARGET_SELECTED));
    if (!target || !target->creatureId())
        return false;
    // The creature's field, or no field when the creature is gone.
    game::TGameSessionBase* session = tibia::game::currentGameSession();
    if (!session)
        return true;
    const TCreaturePtr creature = session->creatureStorage() ? session->creatureStorage()->creatureAt(target->creatureId()) : nullptr;
    session->setNextTalkAim(creature ? creature->position() : Position());
    session->setNextTalkAimMode(2);
    if (session->canPerformGameAction() && !action.words().isEmpty())
        tibia::protocol::gameclient::sendTalk(static_cast<uint32_t>(action.talkMode()), 0, "", action.words().toStdString());
    return true;
}

void TBattleListController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_proxy->connectToGameSession(GameSession);
}

void TBattleListController::attachSidebarOptions(config::TSidebarOptions* options)
{
    QObject::disconnect(m_sidebarOptionsChanged);
    m_sidebarOptions = options;
    if (!options)
        return;
    m_sidebarOptionsChanged = connect(options, &config::TSidebarOptions::battleListWidgetOptionsChanged, this,
                                      &TBattleListController::onThisWidgetOptionsChanged);
    onThisWidgetOptionsChanged(m_id);
}

void TBattleListController::setPremiumTimeStorage(game::TPremiumTimeStorage* storage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TBattleListController::premiumTimeChanged);
    m_premiumTimeStorage = storage;
    if (storage)
        connect(storage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TBattleListController::premiumTimeChanged);
    Q_EMIT premiumTimeChanged();
}

bool TBattleListController::filterButtonsVisible() const { return m_options.showFilters; }
bool TBattleListController::isPartyView() const { return m_options.isPartyView; }
QString TBattleListController::name() const { return m_options.name.toHtmlEscaped(); }
bool TBattleListController::isPrimary() const { return m_options.isPrimary; }
bool TBattleListController::hasPremiumStatus() const
{
    return m_premiumTimeStorage && m_premiumTimeStorage->isPremium();
}
QVariant TBattleListController::battleListModel() const { return m_battleListModel; }

QQuickItem* TBattleListController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
void TBattleListController::onCloseClicked() { onRequestClose(); }
void TBattleListController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TBattleListController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TBattleListController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TBattleListController::onLevelUpClicked() {}
void TBattleListController::setCursorToResizeVertical(const bool Enabled) { widget()->setCursorToResizeVertical(Enabled); }
void TBattleListController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

void TBattleListController::storeOptions(BattleListOptions options)
{
    options.contentHeight = widget()->maximizedContentHeight();
    options.contentMaximized = widget()->isMaximized();
    m_storage->setOptions(m_id, options);
}

bool TBattleListController::isFilterActive(const EBattleListWidgetFilterKey filterKey) { return m_options.filters.contains(filterKey); }

void TBattleListController::toggleFilter(const EBattleListWidgetFilterKey filterKey)
{
    BattleListOptions options = m_options;
    if (!options.filters.erase(filterKey))
        options.filters.insert(filterKey);
    storeOptions(options);
}

void TBattleListController::onFilterButtonsVisibleClicked()
{
    BattleListOptions options = m_options;
    options.showFilters = !options.showFilters;
    storeOptions(options);
}

void TBattleListController::saveWidgetOptions() { storeOptions(m_options); }

void TBattleListController::onRequestClose()
{
    // The list's options leave its id; the primary and the party list keep
    // theirs under the ids the sidebar options reserve for them.
    m_storage->removeOptions(m_id);
    if (m_options.isPrimary)
        m_storage->setOptions(config::TSidebarOptions::PrimaryBattleListId, m_options);
    else if (m_options.isPartyView)
        m_storage->setOptions(config::TSidebarOptions::PartyBattleListId, m_options);
    Q_EMIT battleListClosed(m_id);
}

void TBattleListController::requestOpenSecondaryBattleList()
{
    // Premium opens a new list in the first sidebar with room, free accounts
    // get the store's premium page.
    if (hasPremiumStatus()) {
        auto action = std::make_shared<input::TGameActionOpenSecondaryBattleListInSpecificSidebar>(static_cast<uint8_t>(0xff), static_cast<uint8_t>(0xff));
        Q_EMIT m_storage->publishGameAction(action);
        return;
    }
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 0});
    Q_EMIT m_storage->publishGameAction(action);
}

void TBattleListController::requestEditName()
{
    m_renameDialog->setText(m_options.name);
    m_renameDialog->dialog()->show();
}

void TBattleListController::onNewNameEntered(const QString& EnteredName)
{
    BattleListOptions options = m_options;
    options.name = EnteredName.toHtmlEscaped();
    storeOptions(options);
}

void TBattleListController::requestMakePrimary()
{
    auto action = std::make_shared<input::TGameActionMakeBattleListPrimary>(static_cast<uint8_t>(m_id));
    Q_EMIT m_storage->publishGameAction(action);
}

void TBattleListController::removeThisBattleListOptionsIfNotSpecialBattleList()
{
    // The options leave the list's id; the primary and the party list keep
    // theirs under the ids reserved for them.
    if (!m_sidebarOptions)
        return;
    m_storage->removeOptions(m_id);
    if (m_options.isPrimary)
        m_storage->setOptions(config::TSidebarOptions::PrimaryBattleListId, m_options);
    else if (m_options.isPartyView)
        m_storage->setOptions(config::TSidebarOptions::PartyBattleListId, m_options);
}

void TBattleListController::onWidgetOptionsChanged() { onThisWidgetOptionsChanged(m_id); }

void TBattleListController::onThisWidgetOptionsChanged(const int BattleListID)
{
    if (!m_sidebarOptions || BattleListID != m_id)
        return;
    if (m_storage->hasOptions(m_id)) {
        m_options = m_storage->options(m_id);
    } else {
        // A list without options starts from the defaults and saves them.
        m_options = BattleListOptions{};
        saveWidgetOptions();
    }
    widget()->setInitialContentHeightAndMaximized(m_options.contentHeight, m_options.contentMaximized);
    m_proxy->setOptions(m_options);
    Q_EMIT optionsChanged();
}

void TBattleListController::onWidgetQuickItemDestroyed() {}

void TBattleListController::onShowConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("battlelist_rename")), &TContextMenuEntryItem::triggered, this,
            &TBattleListController::requestEditName);
    if (!m_options.isPartyView) {
        TContextMenuEntryItem* secondary =
            menu->addCustomEntry(qtTrId("battlelist_open_secondary"), hasPremiumStatus() ? QString() : qtTrId("context_premium_feature"));
        connect(secondary, &TContextMenuEntryItem::triggered, this, &TBattleListController::requestOpenSecondaryBattleList);
        if (!m_options.isPrimary) {
            connect(menu->addCustomEntry(qtTrId("battlelist_make_primary")), &TContextMenuEntryItem::triggered, this,
                    &TBattleListController::requestMakePrimary);
        }
    }
    menu->addCustomSeparator();
    // Picking a sort key makes it the first key, by age ascending the second.
    for (int i = 0; i < static_cast<int>(std::size(SortMenuEntries)); ++i) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(SortMenuEntries[i]));
        entry->setCheckable(true);
        entry->setChecked(m_options.sortOrder[0] == i);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, i] {
            BattleListOptions options = m_options;
            options.sortOrder = { static_cast<BattleListOptions::Sort>(i), BattleListOptions::ByAgeAscending };
            storeOptions(options);
        });
    }
    menu->showCustomMenu();
}

int TBattleListController::mapperContext() const { return m_options.isPartyView ? PartyListContext : BattleListContext; }

void TBattleListController::onCurrentlyHoveredCreatureChanged(const int creatureId)
{
    m_hoveredCreature = static_cast<uint32_t>(creatureId);
    // While an entry's context menu is open, that entry stays selected.
    const uint32_t selected = m_lockedCreature ? m_lockedCreature : m_hoveredCreature;
    const input::TGameActionList actions = battleListGameActions(selected);
    const input::IGameActionPtr temporarySelect = findAction(actions, A::TEMPORARY_SELECT_CREATURE);
    const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
    const input::IGameActionPtr action = m_mapper.map(actions, mapperContext(), Qt::LeftButton, modifiers);
    if (Cursor* cursor = Cursor::instance()) {
        if (action)
            cursor->setLayerShape(Cursor::BattleList, worldmap::TWorldMapGameActionProvider::cursorShapeOf(action));
        if (creatureId == 0)
            cursor->onBattleListExited();
        else
            cursor->onBattleListEntered();
    }
    if (temporarySelect)
        Q_EMIT m_storage->publishGameAction(temporarySelect);
}

void TBattleListController::onCreatureClicked(const int creatureId, const int mouseButton, const int keyboardModifiers)
{
    const input::TGameActionList actions = battleListGameActions(static_cast<uint32_t>(creatureId));
    const input::IGameActionPtr action = m_mapper.map(actions, mapperContext(), static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        Q_EMIT m_storage->publishGameAction(action);
        return;
    }
    m_lockedCreature = static_cast<uint32_t>(creatureId);
    onCurrentlyHoveredCreatureChanged(static_cast<int>(m_hoveredCreature));
    showCreatureContextMenu(input::contextMenuActions(actions, ContextMenuOrder));
}

void TBattleListController::showCreatureContextMenu(const input::TGameActionList& actions)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    QObject::disconnect(m_contextMenuClosed);
    m_contextMenuClosed =
        connect(menu, &TContextMenuController::contextMenuClosed, this, &TBattleListController::onContextMenuControllerMenuClosed);
    // The modifier hints are those of a battle list (not the party list).
    menu->showGameActions(actions, BattleListContext, m_mapper);
}

void TBattleListController::onContextMenuControllerMenuClosed()
{
    m_lockedCreature = 0;
    onCurrentlyHoveredCreatureChanged(static_cast<int>(m_hoveredCreature));
    QObject::disconnect(m_contextMenuClosed);
}

void TBattleListController::onCreatureTargetSelected(const int creatureId)
{
    if (const input::IGameActionPtr action = findAction(battleListGameActions(static_cast<uint32_t>(creatureId)), A::TARGET_SELECTED))
        Q_EMIT m_storage->publishGameAction(action);
}

// ---------------------------------------------------------------------------

namespace {

/// The controller struct from the sidebar options' typed record.
BattleListOptions fromSidebarOptions(const config::TBattleListOptions& o)
{
    BattleListOptions options;
    for (const int filter : o.filters) {
        if (filter >= 0 && filter < FilterCount)
            options.filters.insert(static_cast<Filter>(filter));
    }
    for (size_t level = 0; level < options.sortOrder.size() && level < o.sortOrder.size(); ++level) {
        if (o.sortOrder[level] >= 0 && o.sortOrder[level] < SortCount)
            options.sortOrder[level] = static_cast<BattleListOptions::Sort>(o.sortOrder[level]);
    }
    options.contentHeight = o.contentHeight;
    options.contentMaximized = o.contentMaximized;
    options.isPartyView = o.isPartyView;
    options.isPrimary = o.isPrimary;
    options.name = o.name;
    options.showFilters = o.showFilters;
    return options;
}

config::TBattleListOptions toSidebarOptions(const BattleListOptions& o)
{
    config::TBattleListOptions options;
    for (const Filter filter : o.filters)
        options.filters.push_back(static_cast<int>(filter));
    options.sortOrder = { static_cast<int>(o.sortOrder[0]), static_cast<int>(o.sortOrder[1]) };
    options.contentHeight = o.contentHeight;
    options.contentMaximized = o.contentMaximized;
    options.isPartyView = o.isPartyView;
    options.isPrimary = o.isPrimary;
    options.name = o.name;
    options.showFilters = o.showFilters;
    return options;
}
}

TBattleListControllerStorage::TBattleListControllerStorage(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : cip::tibia::gamewindow::TBattleListControllerStorage(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
    , m_dataModel(this)
{
    m_updateTimer.setInterval(UpdateIntervalMs);
    connect(&m_updateTimer, &QTimer::timeout, this, &TBattleListControllerStorage::onUpdateTimer);

    for (const A type : { A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, A::MAKE_BATTLE_LIST_PRIMARY }) {
        m_gameActions.on(type, [this](const input::IGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
    // CIP forwards the at-mouse-cursor actions to every list; the hovered one
    // takes it.
    for (const A type : { A::USE_AT_MOUSECURSOR, A::SPELL_AT_MOUSE_CURSOR }) {
        m_gameActions.on(type, [this](const input::IGameAction& action) { handleGameAction(action); });
    }
    // The creature storage's selected creature gets its frame; with
    // mouseHighlightTarget on it is also the world map's mouse target,
    // otherwise the mouse target is cleared.
    m_gameActions.on(A::TEMPORARY_SELECT_CREATURE, [this](const input::TGameActionCreature& action) {
        setTemporarySelectedCreature(action.creatureId());
        if (m_worldMapRenderProvider) {
            const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
            const creatures::TCreatureStorage* storage = session ? session->creatureStorage().get() : nullptr;
            const bool highlighted = creatures::TCreature::markOptions().mouseHighlightTarget && storage
                && storage->creatureAt(action.creatureId());
            m_worldMapRenderProvider->setMouseTargetCreatureId(highlighted ? action.creatureId() : 0);
        }
        return true;
    });
}

TBattleListControllerStorage::~TBattleListControllerStorage()
{
    for (auto& [id, controller] : m_controllers)
        delete controller;
}

void TBattleListControllerStorage::setSidebarOptions(config::TSidebarOptions* options)
{
    m_sidebarOptions = options;
    if (!options)
        m_loginRampUpFinished = false;
    // Each list follows its own entry.
    for (const auto& [id, controller] : m_controllers)
        controller->attachSidebarOptions(options);
}

void TBattleListControllerStorage::setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    m_premiumTimeStorage = premiumTimeStorage;
    for (const auto& [id, controller] : m_controllers)
        controller->setPremiumTimeStorage(m_premiumTimeStorage);
}

void TBattleListControllerStorage::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    for (const auto& [id, controller] : m_controllers)
        controller->setControlScheme(controlScheme);
}

BattleListOptions TBattleListControllerStorage::options(const int battleListId) const
{
    if (m_sidebarOptions) {
        const config::TBattleListsOptions& lists = m_sidebarOptions->battleListsOptions();
        if (const auto it = lists.find(battleListId); it != lists.end())
            return fromSidebarOptions(it->second);
    }
    return fromSidebarOptions(config::TBattleListOptions{});
}

void TBattleListControllerStorage::setOptions(const int battleListId, const BattleListOptions& options)
{
    if (m_sidebarOptions)
        m_sidebarOptions->setBattleListOptions(battleListId, toSidebarOptions(options));
}

config::TSidebarOptions* TBattleListControllerStorage::sidebarOptions() const { return m_sidebarOptions; }

bool TBattleListControllerStorage::hasOptions(const int battleListId) const
{
    return m_sidebarOptions && m_sidebarOptions->battleListsOptions().contains(battleListId);
}

void TBattleListControllerStorage::removeOptions(const int battleListId)
{
    if (m_sidebarOptions)
        m_sidebarOptions->removeBattleListOptions(battleListId);
}

bool TBattleListControllerStorage::hasPremium() const
{
    return m_premiumTimeStorage && m_premiumTimeStorage->isPremium();
}

bool TBattleListControllerStorage::findFreeBattleListId(const uint8_t requestedId, uint8_t* battleListId) const
{
    if (requestedId < MaxBattleLists && !m_controllers.contains(requestedId)) {
        *battleListId = requestedId;
        return true;
    }
    for (uint8_t id = 0; id < MaxBattleLists; ++id) {
        if (!m_controllers.contains(id)) {
            *battleListId = id;
            return true;
        }
    }
    return false;
}

bool TBattleListControllerStorage::acquireBattleListId(const uint8_t requestedId, uint8_t* battleListId) const
{
    if (m_controllers.size() >= MaxBattleLists)
        return false;
    return findFreeBattleListId(requestedId, battleListId);
}

void TBattleListControllerStorage::publishStatusLine(const QString& text, const QColor& color)
{
    auto status = std::make_shared<input::TGameActionSetStatusLine>(text, color);
    Q_EMIT publishGameAction(status);
}

void TBattleListControllerStorage::maximizeIfLoggedIn(TBattleListController* controller) const
{
    if (controller && m_loginRampUpFinished)
        controller->widget()->maximize();
}

void TBattleListControllerStorage::openBattleList(const uint8_t sidebarId, const uint8_t requestedId, const bool primary, const bool party)
{
    uint8_t battleListId = 0;
    if (!acquireBattleListId(requestedId, &battleListId)) {
        publishStatusLine(qtTrId(party ? "battlelist_party_can_not_open_more" : "battlelist_can_not_open_more"), chat::systemWhite());
        return;
    }
    if (m_sidebarOptions && (requestedId == config::TSidebarOptions::PrimaryBattleListId || primary))
        m_sidebarOptions->makePrimaryBattleList(battleListId);
    if (m_sidebarOptions && party)
        m_sidebarOptions->makePartyBattleList(battleListId);

    auto* controller = new TBattleListController(battleListId, this, &m_dataModel, m_gameWindowItem, m_engine);
    controller->setControlScheme(m_controlScheme);
    controller->setPremiumTimeStorage(m_premiumTimeStorage);
    controller->connectToGameSession(m_gameSession);
    connect(controller, &TBattleListController::battleListClosed, this, &TBattleListControllerStorage::onBattleListClosed,
            Qt::QueuedConnection);
    maximizeIfLoggedIn(controller);
    if (!controller->isPrimary() && !controller->isPartyView() && !hasPremium()) {
        // Secondary lists are a premium feature.
        controller->widget()->hide();
        controller->deleteLater();
        publishStatusLine(qtTrId("battlelist_need_premium"), QColor(0x18, 0x72, 0xc3));
        return;
    }
    if (m_sidebars)
        m_sidebars->placeWidget(sidebarId, controller->widget());
    controller->widget()->show();
    m_controllers[static_cast<uint8_t>(controller->battleListId())] = controller;
    if (controller->isPrimary()) {
        m_primary = controller;
        Q_EMIT widgetVisible(PrimaryWidgetType);
    }
    if (controller->isPartyView()) {
        m_party = controller;
        Q_EMIT widgetVisible(PartyWidgetType);
    }
    triggerUpdate();
    if (!m_updateTimer.isActive())
        m_updateTimer.start();
}

void TBattleListControllerStorage::togglePrimaryBattleList(const uint8_t sidebarId)
{
    if (m_primary)
        m_primary->onRequestClose();
    else
        openBattleList(sidebarId, AnyId, true, false);
}

void TBattleListControllerStorage::togglePartyBattleList(const uint8_t sidebarId)
{
    if (m_party)
        m_party->onRequestClose();
    else
        openBattleList(sidebarId, AnyId, false, true);
}

bool TBattleListControllerStorage::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
        case A::USE_AT_MOUSECURSOR:
        case A::SPELL_AT_MOUSE_CURSOR: {
            // Every list sees it; the list with a hovered creature takes it.
            const auto* use = input::gameActionAs<input::TGameActionUseAtMouseCursor>(&action);
            const auto* spell = input::gameActionAs<input::TGameActionSpellAtMouseCursor>(&action);
            bool handled = false;
            for (const auto& [id, controller] : m_controllers) {
                if (use)
                    handled |= controller->handleGameActionAtMouseCursor(*use);
                else if (spell)
                    handled |= controller->handleGameActionCastSpellAtMouseCursor(*spell);
            }
            return handled;
        }
        case A::TOGGLE_WIDGET:
            if (const auto* toggle = input::gameActionAs<input::TGameActionToggleWidget>(&action)) {
                if (toggle->widgetType() == PrimaryWidgetType)
                    togglePrimaryBattleList(toggle->sidebarId());
                else if (toggle->widgetType() == PartyWidgetType)
                    togglePartyBattleList(toggle->sidebarId());
            }
            return true;
        case A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR:
            if (const auto* open = input::gameActionAs<input::TGameActionOpenSecondaryBattleListInSpecificSidebar>(&action))
                openBattleList(open->sidebarId(), open->battleListId(), false, false);
            return true;
        case A::MAKE_BATTLE_LIST_PRIMARY:
            if (const auto* primary = input::gameActionAs<input::TGameActionMakeBattleListPrimary>(&action))
                makePrimary(primary->battleListId());
            return true;
        default:
            return false;
    }
}

void TBattleListControllerStorage::makePrimary(const uint8_t battleListId)
{
    if (!m_sidebarOptions)
        return;
    TBattleListController* previous = m_primary;
    m_primary = nullptr;
    Q_EMIT widgetHidden(PrimaryWidgetType);
    config::TBattleListsOptions lists = m_sidebarOptions->battleListsOptions();
    for (auto& [id, options] : lists) {
        options.isPrimary = id == battleListId;
        if (!options.isPrimary)
            continue;
        const auto it = m_controllers.find(battleListId);
        if (it == m_controllers.end())
            continue;
        m_primary = it->second;
        Q_EMIT widgetVisible(PrimaryWidgetType);
    }
    if (m_primary) {
        m_sidebarOptions->setBattleListsOptions(lists);
    } else if (previous) {
        // No open list took the role: the previous primary list keeps it.
        m_primary = previous;
        Q_EMIT widgetVisible(PrimaryWidgetType);
    }
}

void TBattleListControllerStorage::onBattleListClosed(const int battleListId) { removeBattleList(static_cast<uint8_t>(battleListId), false); }

void TBattleListControllerStorage::removeBattleList(const uint8_t battleListId, const bool saveOptions)
{
    const auto it = m_controllers.find(battleListId);
    if (it == m_controllers.end())
        return;
    TBattleListController* controller = it->second;
    if (controller == m_primary) {
        m_primary = nullptr;
        Q_EMIT widgetHidden(PrimaryWidgetType);
    }
    if (controller == m_party) {
        m_party = nullptr;
        Q_EMIT widgetHidden(PartyWidgetType);
    }
    if (saveOptions)
        controller->saveWidgetOptions();
    m_controllers.erase(it);
    if (m_sidebars)
        m_sidebars->removeWidget(controller->widget());
    controller->widget()->hide();
    controller->deleteLater();
    if (m_controllers.empty())
        m_updateTimer.stop();
    Q_EMIT battleListRemovedFromStorage();
}

void TBattleListControllerStorage::closeAll(const bool saveWidgetOptions)
{
    if (saveWidgetOptions) {
        for (const auto& [id, controller] : m_controllers)
            controller->saveWidgetOptions();
    }
    const std::map<uint8_t, TBattleListController*> lists = std::exchange(m_controllers, {});
    for (const auto& [id, controller] : lists) {
        if (m_sidebars)
            m_sidebars->removeWidget(controller->widget());
        controller->widget()->hide();
        controller->deleteLater();
    }
    m_updateTimer.stop();
    if (m_primary) {
        Q_EMIT widgetHidden(PrimaryWidgetType);
        m_primary = nullptr;
    }
    if (m_party) {
        Q_EMIT widgetHidden(PartyWidgetType);
        m_party = nullptr;
    }
}

std::vector<std::pair<int, TSidebarWidget*>> TBattleListControllerStorage::widgets() const
{
    std::vector<std::pair<int, TSidebarWidget*>> result;
    for (const auto& [id, controller] : m_controllers)
        result.emplace_back(id, controller->widget());
    return result;
}

void TBattleListControllerStorage::setTemporarySelectedCreature(const uint32_t creatureId)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    creatures::TCreatureStorage* storage = session ? session->creatureStorage().get() : nullptr;
    if (!storage || creatureId == storage->temporarySelectedID())
        return;
    storage->setTemporarySelectedID(creatureId);
    triggerUpdate();
}

void TBattleListControllerStorage::onLoginRampUpFinished()
{
    m_loginRampUpFinished = true;
    if (!m_sidebarOptions)
        return;
    config::TBattleListsOptions lists = m_sidebarOptions->battleListsOptions();
    std::erase_if(lists, [this](const auto& entry) {
        const auto& [id, options] = entry;
        return !m_controllers.contains(static_cast<uint8_t>(id)) && !options.isPrimary && !options.isPartyView;
    });
    m_sidebarOptions->setBattleListsOptions(lists);
}

void TBattleListControllerStorage::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    closeAll(true);
    m_gameSession = GameSession;
    m_loginRampUpFinished = false;
    if (m_gameSession) {
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        const config::TCharacterConfigurationManager* configuration = session ? session->characterConfigurationManager().get() : nullptr;
        setSidebarOptions(configuration ? configuration->sidebarOptions() : nullptr);
        m_gameSessionConnections.push_back(connect(m_gameSession.get(), &game::IGameSession::loginRampUpFinished, this,
                                                   &TBattleListControllerStorage::onLoginRampUpFinished));
    }
    for (const auto& [id, controller] : m_controllers)
        controller->connectToGameSession(m_gameSession);
}

void TBattleListControllerStorage::triggerUpdate()
{
    // Without a session the lists empty.
    std::vector<BattleListEntry> commands;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        if (const std::shared_ptr<creatures::TBattleListRenderProvider>& provider = session->battleListRenderProvider())
            commands = provider->drawCommands();
    }
    m_dataModel.setEntries(std::move(commands));
}

void TBattleListControllerStorage::onUpdateTimer() { triggerUpdate(); }

}
