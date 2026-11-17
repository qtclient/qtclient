#include "battlelistcontroller.h"

#include "chat/chatchannel.h"
#include "config/sidebaroptions.h"
#include "creatures/creaturestorage.h"
#include "creatures/creaturesciptypes.h"
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

#include <QGuiApplication>
#include <QQuickItem>

#include <algorithm>
#include <cstdlib>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Filter = BattleListOptions::Filter;
using Cursor = TMouseCursorController;

constexpr int UpdateIntervalMs = 100;
/// CIP : the mapper context of a battle list and the party list.
constexpr int BattleListContext = 1;
constexpr int PartyListContext = 2;
/// The shared experience range (tiles, floors).
constexpr int SharedXpRange = 30;
constexpr int SharedXpFloors = 1;

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

/// The party shields of members and leaders (3..10), as in the game action provider.
bool isPartyMember(const TCreature& creature)
{
    const uint8_t shield = creature.shield();
    return shield >= 3 && shield <= 10;
}

/// The protocol's vocation without its promotion.
int baseVocation(const TCreature& creature) { return creature.vocation() % 10; }

QColor colorOf(const QColor& c) { return QColor(c.red(), c.green(), c.blue()); }

QColor outfitColor(const int index) { return colorOf(Outfit::getColor(index)); }

QString stripIcon(const char* provider, const uint8_t value)
{
    return value ? QStringLiteral("image://creaturestateflags-%1/%2").arg(QLatin1String(provider)).arg(value - 1) : QString();
}

/// A battle list entry's actions: the creature's, then CONTEXT_MENU.
input::TGameActionList battleListGameActions(const uint32_t creatureId)
{
    input::TGameActionList actions = input::TGenericObjectGameActionProvider::creatureActions(creatureId);
    actions.push_back(input::makeGameAction(A::CONTEXT_MENU));
    return actions;
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

bool BattleListEntry::operator==(const BattleListEntry& other) const
{
    // CIP compares the outfit by pointer and the status (icons and marks) as
    // one record; qtclient carries both by value.
    return creatureId == other.creatureId && name == other.name && healthPercent == other.healthPercent
        && frameColors == other.frameColors && statusIcons == other.statusIcons && fiendish == other.fiendish
        && leader == other.leader && leaderMinion == other.leaderMinion && outfitId == other.outfitId
        && outfitIsObject == other.outfitIsObject && headColor == other.headColor && torsoColor == other.torsoColor
        && legsColor == other.legsColor && detailColor == other.detailColor && firstAddOn == other.firstAddOn
        && secondAddOn == other.secondAddOn && tutorialMarkerId == other.tutorialMarkerId && visibleOrder == other.visibleOrder
        && distance == other.distance && temporarySelected == other.temporarySelected && manaPercent == other.manaPercent
        && showMana == other.showMana && inSharedXpDistance == other.inSharedXpDistance && inViewport == other.inViewport
        && vocation == other.vocation;
}

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
    if (current.leader != incoming.leader)
        roles.append(0x118);
    if (current.leaderMinion != incoming.leaderMinion)
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
        case 0x118: return e.leader;
        case 0x119: return e.leaderMinion;
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

bool TBattleListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
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
    creatures::ownProductSurface(this);
    m_renameDialog->setCaption(qtTrId("battlelist_rename"));
    m_renameDialog->setDescription(qtTrId("battlelist_rename_text"));
    m_renameDialog->setMaxInputLength(30);
    connect(m_renameDialog, &TGenericEnterTextDialogController::textEntered, this, &TBattleListController::onNewNameEntered);
    m_proxy->setSourceModel(model);
    m_battleListModel = QVariant::fromValue<QObject*>(m_proxy);
    m_options = storage->options(battleListId);
    m_proxy->setOptions(m_options);
    m_hasPremiumStatus = storage->hasPremium();
    widget()->setInitialContentHeightAndMaximized(m_options.contentHeight, m_options.contentMaximized);

    widget()->createItem(QStringLiteral("battlelist.qml"));
    QQuickItem* item = quickItem();
    if (!item)
        return;
    connect(item, SIGNAL(currentlyHoveredCreatureChanged(int)), this, SLOT(onCurrentlyHoveredCreatureChanged(int)));
    connect(item, SIGNAL(creatureClicked(int, int, int)), this, SLOT(onCreatureClicked(int, int, int)));
    connect(item, SIGNAL(creatureTargetSelected(int)), this, SLOT(onCreatureTargetSelected(int)));
}

TBattleListController::~TBattleListController() { QObject::disconnect(m_contextMenuClosed); }

void TBattleListController::handleGameAction()
{
    // The game actions reach the list through the mapper and the action listeners.
}

void TBattleListController::handleGameActionAtMouseCursor()
{
    // The storage forwards USE_AT_MOUSECURSOR to onAtMouseCursor.
}

void TBattleListController::handleGameActionCastSpellAtMouseCursor()
{
    // The storage forwards SPELL_AT_MOUSE_CURSOR to onAtMouseCursor.
}

bool TBattleListController::onAtMouseCursor(const input::TGameAction& action)
{
    if (!m_hoveredCreature)
        return false;
    const input::TGameActionPtr target = input::unportedGameAction(findAction(battleListGameActions(m_hoveredCreature), A::TARGET_SELECTED));
    if (!target || !target->creatureId)
        return false;
    if (action.type == A::USE_AT_MOUSECURSOR) {
        auto use = input::mutableClone(*input::makeAction(A::USE_ON_CREATURE));
        use->objectClientId = action.objectClientId;
        use->position = action.position;
        use->stackPosition = action.stackPosition;
        use->creatureId = target->creatureId;
        input::publish(use);
        return true;
    }
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    const TCreaturePtr creature = storage ? storage->creatureAt(target->creatureId) : nullptr;
    if (!creature || action.text.isEmpty())
        return false;
    tibia::game::setNextTalkAim(creature->position());
    tibia::game::setNextTalkAimMode(2);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(action.talkMode), 0, action.text.toStdString());
    return true;
}

void TBattleListController::applyOptions(const BattleListOptions& options)
{
    const bool changed = !(options == m_options);
    m_options = options;
    if (m_proxy)
        m_proxy->setOptions(options);
    if (changed)
        Q_EMIT optionsChanged();
}

void TBattleListController::setPremium(const bool premium)
{
    if (premium == m_hasPremiumStatus)
        return;
    m_hasPremiumStatus = premium;
    Q_EMIT premiumTimeChanged();
}

bool TBattleListController::filterButtonsVisible() const { return m_options.showFilters; }
bool TBattleListController::isPartyView() const { return m_options.isPartyView; }
QString TBattleListController::name() const { return m_options.name.toHtmlEscaped(); }
bool TBattleListController::isPrimary() const { return m_options.isPrimary; }
bool TBattleListController::hasPremiumStatus() const { return m_storage->hasPremium(); }
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
        input::publish(action);
        return;
    }
    auto action = std::make_shared<input::TGameActionOpenStore>(1, 0);
    input::publish(action);
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
    input::publish(action);
}

void TBattleListController::removeThisBattleListOptionsIfNotSpecialBattleList()
{
    if (!m_options.isPrimary && !m_options.isPartyView)
        m_storage->removeOptions(m_id);
}

void TBattleListController::onWidgetOptionsChanged() { applyOptions(m_storage->options(m_id)); }

void TBattleListController::onThisWidgetOptionsChanged(const int BattleListID)
{
    if (BattleListID == m_id)
        onWidgetOptionsChanged();
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
        input::publish(temporarySelect);
}

void TBattleListController::onCreatureClicked(const int creatureId, const int mouseButton, const int keyboardModifiers)
{
    const input::TGameActionList actions = battleListGameActions(static_cast<uint32_t>(creatureId));
    const input::IGameActionPtr action = m_mapper.map(actions, mapperContext(), static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
    if (!action)
        return;
    if (action->gameActionType() != A::CONTEXT_MENU) {
        input::publish(action);
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
        input::publish(action);
}

// ---------------------------------------------------------------------------

namespace {
TBattleListControllerStorage* g_battleListStorage = nullptr;

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
    g_battleListStorage = this;
    m_updateTimer.setInterval(UpdateIntervalMs);
    connect(&m_updateTimer, &QTimer::timeout, this, &TBattleListControllerStorage::onUpdateTimer);

    for (const A type : { A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, A::MAKE_BATTLE_LIST_PRIMARY }) {
        input::setActionHandler(type, [this](const input::IGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
    // CIP forwards the at-mouse-cursor actions to every list; the hovered one
    // takes it.
    for (const A type : { A::USE_AT_MOUSECURSOR, A::SPELL_AT_MOUSE_CURSOR }) {
        m_actionListeners.push_back(
            input::addActionListener(type, [this](const input::IGameAction& action) { return handleGameAction(action); }));
    }
    // The creature storage's selected creature gets its frame; with
    // mouseHighlightTarget on it is also the world map's mouse target,
    // otherwise the mouse target is cleared.
    input::setActionHandler(A::TEMPORARY_SELECT_CREATURE, [this](const input::TGameActionCreature& action) {
        setTemporarySelectedCreature(action.creatureId());
        if (m_worldMapRenderProvider) {
            creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
            const bool highlighted = creatures::TCreature::markOptions().mouseHighlightTarget && storage
                && storage->creatureAt(action.creatureId());
            m_worldMapRenderProvider->setMouseTargetCreatureId(highlighted ? action.creatureId() : 0);
        }
        return true;
    });
    m_gameActionHandler = new TBattleListGameActionHandler(this, this);
    input::setActionHandler(A::ATTACK_FIRST_TARGET, [this](const input::TGameAction&) {
        m_gameActionHandler->attackFirstTarget();
        return true;
    });
    input::setActionHandler(A::ATTACK_NEXT_TARGET, [this](const input::TGameAction&) {
        m_gameActionHandler->attackNextOrPreviousTarget(false);
        return true;
    });
    input::setActionHandler(A::ATTACK_PREVIOUS_TARGET, [this](const input::TGameAction&) {
        m_gameActionHandler->attackNextOrPreviousTarget(true);
        return true;
    });

    // Party list CipSoft publishes — creature fields are already updated; refresh rows.
    using Args = std::vector<std::any>;
    const auto refreshParty = [this](GameObject*, const Args&) {
        triggerUpdate();
        return std::any{};
    };
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyLeft", refreshParty);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyMemberHealthChange", refreshParty);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyMemberManaChange", refreshParty);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyMemberInfoChange", refreshParty);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPartyMemberShowStatusChange", refreshParty);
}

TBattleListControllerStorage::~TBattleListControllerStorage()
{
    if (g_battleListStorage == this)
        g_battleListStorage = nullptr;
    for (const A type : { A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, A::MAKE_BATTLE_LIST_PRIMARY, A::TEMPORARY_SELECT_CREATURE,
                          A::ATTACK_FIRST_TARGET, A::ATTACK_NEXT_TARGET, A::ATTACK_PREVIOUS_TARGET })
        input::setActionHandler(type, nullptr);
    for (const int id : m_actionListeners)
        input::removeActionListener(id);
    for (auto& [id, controller] : m_controllers)
        delete controller;
}

TBattleListControllerStorage* TBattleListControllerStorage::instance()
{
    return g_battleListStorage;
}

void TBattleListControllerStorage::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_sidebarOptions)
        disconnect(m_sidebarOptions, nullptr, this, nullptr);
    m_sidebarOptions = options;
    if (!options) {
        m_loginRampUpFinished = false;
        return;
    }
    // A list follows its options (CIP: each list listens to its id).
    connect(options, &config::TSidebarOptions::battleListWidgetOptionsChanged, this, [this](const int battleListId) {
        if (const auto it = m_controllers.find(static_cast<uint8_t>(battleListId));
            battleListId >= 0 && battleListId < MaxBattleLists && it != m_controllers.end())
            it->second->applyOptions(this->options(battleListId));
    });
    for (const auto& [id, controller] : m_controllers)
        controller->applyOptions(this->options(id));
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

void TBattleListControllerStorage::removeOptions(const int battleListId)
{
    if (m_sidebarOptions)
        m_sidebarOptions->removeBattleListOptions(battleListId);
}

bool TBattleListControllerStorage::hasPremium() const
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
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
    input::publish(status);
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
            const auto* fields = input::gameActionAs<input::TGameAction>(&action);
            if (!fields)
                return false;
            // Every list sees it; the list with a hovered creature takes it.
            bool handled = false;
            for (const auto& [id, controller] : m_controllers)
                handled |= controller->onAtMouseCursor(*fields);
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

void TBattleListControllerStorage::closeAll()
{
    while (!m_controllers.empty())
        removeBattleList(m_controllers.begin()->first, true);
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
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
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

std::vector<TCreaturePtr> TBattleListControllerStorage::battleListCreatures() const
{
    std::vector<TCreaturePtr> result;
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    if (!tibia::game::hasLocalPlayer() || !storage)
        return result;
    const Position center = tibia::game::localPlayerPosition();
    const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
    for (const TCreaturePtr& creature : storage->spectatorsNear(center, true, range.left, range.right, range.top, range.bottom)) {
        if (!creature || creature->isLocalPlayer() || creature->isDead())
            continue;
        // Party members wherever they are, the others only inside the view
        // and placed on a field.
        if (!isPartyMember(*creature)) {
            const Position pos = creature->position();
            if (pos.z != center.z || std::abs(pos.x - center.x) >= range.left || std::abs(pos.y - center.y) >= range.top)
                continue;
            if (!creature->isPlacedOnField())
                continue;
        }
        result.push_back(creature);
    }
    return result;
}

BattleListEntry TBattleListControllerStorage::entryOf(const TCreaturePtr& creature) const
{
    BattleListEntry e;
    const Position center = tibia::game::localPlayerPosition();
    const Position& pos = creature->position();
    const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
    e.creatureId = creature->creatureId();
    e.name = creature->name();
    e.healthPercent = creature->healthPercent() / 100.f;
    e.healthColor = colorOf(creature->informationColor());
    e.manaPercent = creature->maxMana() > 0 ? creature->mana() / float(creature->maxMana()) : 0.f;
    e.showMana = creature->isPlayer();
    // Every frame the creature shows on the map, the white selection frame too.
    for (const QColor& color : creature->marks())
        e.frameColors.append(color);
    for (const auto& [icon, category, count] : creature->hudIcons()) {
        if (category != 0 && icon != 0)
            e.statusIcons.append(stripIcon("othercreatureicons", icon));
        e.fiendish |= category != 0 && icon == 5;
    }
    e.leader = creature->hasStatusMark(0);
    e.leaderMinion = creature->hasStatusMark(1);
    // A summon is the player's own (1) or another player's (2), by its master.
    uint8_t summon = 0;
    if (creature->creatureType() == Proto::CreatureTypeSummonOwn) {
        const creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
        summon = storage && creature->masterId() == storage->playerID() ? 1 : 2;
    }
    // Empty strings stay in: the QML Row skips the zero-sized images.
    e.statusIcons << stripIcon("summon", summon) << stripIcon("guild", creature->emblem()) << stripIcon("party", creature->shield())
                  << stripIcon("playerkiller", creature->skull());
    const Outfit& outfit = creature->outfit();
    e.outfitIsObject = outfit.getCategory() == ThingCategoryItem;
    e.outfitId = e.outfitIsObject ? outfit.getAuxId() : outfit.getId();
    e.headColor = outfitColor(outfit.getHead());
    e.torsoColor = outfitColor(outfit.getBody());
    e.legsColor = outfitColor(outfit.getLegs());
    e.detailColor = outfitColor(outfit.getFeet());
    e.firstAddOn = (outfit.getAddons() & 1) != 0;
    e.secondAddOn = (outfit.getAddons() & 2) != 0;
    if (const auto it = m_visibleOrder.find(e.creatureId); it != m_visibleOrder.end())
        e.visibleOrder = it->second;
    e.distance = static_cast<uint>(std::max(std::abs(pos.x - center.x), std::abs(pos.y - center.y)));
    if (const creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance())
        e.temporarySelected = storage->temporarySelectedID() == e.creatureId;
    e.inViewport = pos.z == center.z && std::abs(pos.x - center.x) < range.left && std::abs(pos.y - center.y) < range.top;
    e.inSharedXpDistance = std::abs(pos.z - center.z) <= SharedXpFloors && static_cast<int>(e.distance) <= SharedXpRange;
    e.vocation = creature->isPlayer() ? baseVocation(*creature) : 0;
    if (m_gameActionHandler)
        e.tutorialMarkerId = m_gameActionHandler->creatureTutorialMarker(e.creatureId);
    return e;
}

void TBattleListControllerStorage::setCreatureTutorialMarker(const uint32_t creatureId, const int markerId)
{
    if (m_gameActionHandler)
        m_gameActionHandler->setCreatureTutorialMarker(creatureId, markerId);
    triggerUpdate();
}

void TBattleListControllerStorage::clearCreatureTutorialMarkers()
{
    if (m_gameActionHandler)
        m_gameActionHandler->clearCreatureTutorialMarkers();
    triggerUpdate();
}

void TBattleListControllerStorage::triggerUpdate()
{
    if (m_controllers.empty())
        return;
    const std::vector<TCreaturePtr> creatures = battleListCreatures();
    std::set<uint32_t> present;
    for (const TCreaturePtr& creature : creatures) {
        present.insert(creature->creatureId());
        if (!m_visibleOrder.contains(creature->creatureId()))
            m_visibleOrder[creature->creatureId()] = m_nextVisibleOrder++;
    }
    std::erase_if(m_visibleOrder, [&present](const auto& entry) { return !present.contains(entry.first); });
    std::vector<BattleListEntry> entries;
    entries.reserve(creatures.size());
    for (const TCreaturePtr& creature : creatures)
        entries.push_back(entryOf(creature));
    m_dataModel.setEntries(std::move(entries));
}

void TBattleListControllerStorage::onUpdateTimer()
{
    triggerUpdate();
    const bool premium = hasPremium();
    for (const auto& [id, controller] : m_controllers)
        controller->setPremium(premium);
}

bool TBattleListControllerStorage::attackFirstTarget()
{
    const uint32_t targetId = tibia::game::attackingCreatureId();
    for (const TCreaturePtr& creature : battleListCreatures()) {
        if (creature->isNpc() || creature->creatureId() == targetId)
            continue;
        input::publish(input::makeCreatureAction(A::ATTACK_CREATURE, creature->creatureId()));
        return true;
    }
    return true;
}

bool TBattleListControllerStorage::attackNextOrPreviousTarget(const bool previous)
{
    std::vector<TCreaturePtr> list = battleListCreatures();
    if (list.empty())
        return true;
    if (previous)
        std::reverse(list.begin(), list.end());
    const uint32_t targetId = tibia::game::attackingCreatureId();
    if (const auto it = std::find_if(list.begin(), list.end(),
                                     [targetId](const TCreaturePtr& c) { return c && c->creatureId() == targetId; });
        it != list.end())
        std::rotate(list.begin(), it + 1, list.end());
    for (const TCreaturePtr& creature : list) {
        if (creature->isNpc() || creature->creatureId() == targetId)
            continue;
        input::publish(input::makeCreatureAction(A::ATTACK_CREATURE, creature->creatureId()));
        return true;
    }
    return true;
}

}
