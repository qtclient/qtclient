#include "battlelistcontroller.h"

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
#include <QJsonArray>
#include <QJsonObject>
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

constexpr auto OptionsKey = "battleListsOptions";
constexpr int UpdateIntervalMs = 100;
/// CIP : the mapper context of a battle list and the party list.
constexpr int BattleListContext = 1;
constexpr int PartyListContext = 2;
/// The shared experience range (tiles, floors).
constexpr int SharedXpRange = 30;
constexpr int SharedXpFloors = 1;

/// CIP DAT_1022d2920 + 0x28: a battle list entry's context menu.
constexpr A ContextMenuOrder[] = {
    A::START_TALKING_TO_NPC, A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::FOLLOW_CREATURE, A::STOP_FOLLOWING_CREATURE, A::LOOK,
    A::LOOK_AT_CREATURE, A::CONTEXT_MENU_SEPARATOR, A::OPEN_MESSAGE_TO_CHAT_CHANNEL, A::INVITE_PLAYER_TO_PRIVATE_CHAT_CHANNEL, A::ADD_NEW_VIP,
    A::IGNORE_NAME, A::UNIGNORE_NAME, A::INVITE_TO_PARTY, A::JOIN_PARTY, A::REVOKE_PARTY_INVITATION, A::PASS_PARTY_LEADERSHIP, A::JOIN_AGGRESSION,
    A::SET_SHARED_EXPERIENCE, A::LEAVE_PARTY,
    A::INSPECT_PLAYER, A::ASK_TO_INSPECT_PLAYER, A::INVITE_TO_INSPECT_PLAYER, A::ALLOW_INSPECT_PLAYER, A::REVOKE_INSPECT_PLAYER,
    A::CONTEXT_MENU_SEPARATOR, A::REPORT_NAME, A::REPORT_BOT, A::REPORT_HIRELING_NAME, A::CONTEXT_MENU_SEPARATOR,
    A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
};

/// sidebars.json's filter names, in EBattleListWidgetFilterKey order.
constexpr const char* FilterNames[] = {
    "hidePlayers",           "hideKnights",       "hidePaladins",           "hideDruids",
    "hideSorcerers",         "hideMonks",         "hideNPCs",               "hideMonsters",
    "hideNonSkulledPlayers", "hidePartyMembers",  "hidePlayerSummons",      "hideMembersOfOwnGuild",
};
constexpr const char* SortNames[] = {
    "byAgeAscending",       "byAgeDescending",       "byDistanceAscending", "byDistanceDescending",
    "byHitpointsAscending", "byHitpointsDescending", "byNameAscending",     "byNameDescending",
};
/// CIP 's sort entries, in Sort order.
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

bool isMonster(const TCreature& creature)
{
    return !creature.isPlayer() && !creature.isNpc();
}

QColor colorOf(const Color& c) { return QColor(c.r(), c.g(), c.b()); }

QColor outfitColor(const int index) { return colorOf(Outfit::getColor(index)); }

QString stripIcon(const char* provider, const uint8_t value)
{
    return value ? QStringLiteral("image://creaturestateflags-%1/%2").arg(QLatin1String(provider)).arg(value - 1) : QString;
}

BattleListOptions parseOptions(const QJsonObject& json)
{
    BattleListOptions o;
    for (const QJsonValue& value : json.value(QLatin1String("battleListFilters")).toArray()) {
        for (int i = 0; i < static_cast<int>(std::size(FilterNames)); ++i) {
            if (value.toString() == QLatin1String(FilterNames[i]))
                o.filters.insert(static_cast<Filter>(i));
        }
    }
    const QJsonArray sort = json.value(QLatin1String("battleListSortOrder")).toArray();
    for (int level = 0; level < 2 && level < sort.size(); ++level) {
        for (int i = 0; i < static_cast<int>(std::size(SortNames)); ++i) {
            if (sort.at(level).toString() == QLatin1String(SortNames[i]))
                o.sortOrder[level] = static_cast<BattleListOptions::Sort>(i);
        }
    }
    o.contentHeight = json.value(QLatin1String("contentHeight")).toInt(o.contentHeight);
    o.contentMaximized = json.value(QLatin1String("contentMaximized")).toBool(o.contentMaximized);
    o.isPartyView = json.value(QLatin1String("isPartyView")).toBool();
    o.isPrimary = json.value(QLatin1String("isPrimary")).toBool();
    o.name = json.value(QLatin1String("name")).toString();
    o.showFilters = json.value(QLatin1String("showFilters")).toBool();
    return o;
}

QJsonObject serializeOptions(const BattleListOptions& o)
{
    QJsonArray filters;
    for (const Filter filter : o.filters)
        filters.append(QLatin1String(FilterNames[static_cast<int>(filter)]));
    return {
        { QStringLiteral("battleListFilters"), filters },
        { QStringLiteral("battleListSortOrder"), QJsonArray{ QLatin1String(SortNames[o.sortOrder[0]]), QLatin1String(SortNames[o.sortOrder[1]]) } },
        { QStringLiteral("contentHeight"), o.contentHeight },
        { QStringLiteral("contentMaximized"), o.contentMaximized },
        { QStringLiteral("isPartyView"), o.isPartyView },
        { QStringLiteral("isPrimary"), o.isPrimary },
        { QStringLiteral("name"), o.name },
        { QStringLiteral("showFilters"), o.showFilters },
    };
}

/// CIP's filter keys on one creature (TBattleListGameActionHandler::filterDrawCommands).
bool hiddenByFilter(const TCreature& creature, const std::set<Filter>& filters)
{
    const auto has = [&filters](const Filter f) { return filters.contains(f); };
    const bool player = creature.isPlayer();
    if (player) {
        if (has(Filter::HIDE_PLAYERS))
            return true;
        switch (baseVocation(creature)) {
            case 1: if (has(Filter::HIDE_KNIGHTS)) return true; break;
            case 2: if (has(Filter::HIDE_PALADINS)) return true; break;
            case 3: if (has(Filter::HIDE_SORCERERS)) return true; break;
            case 4: if (has(Filter::HIDE_DRUIDS)) return true; break;
            case 5: if (has(Filter::HIDE_MONKS)) return true; break;
            default: break;
        }
        if (has(Filter::HIDE_NON_SKULLED_PLAYERS) && creature.skull() == 0)
            return true;
        if (has(Filter::HIDE_PARTY_MEMBERS) && isPartyMember(creature))
            return true;
        if (has(Filter::HIDE_MEMBERS_OF_OWN_GUILD) && creature.emblem() == 1)
            return true;
    }
    if (creature.isNpc() && has(Filter::HIDE_NPCS))
        return true;
    const uint8_t type = creature.creatureType();
    if ((type == Proto::CreatureTypeSummonOwn || type == Proto::CreatureTypeSummonOther) && has(Filter::HIDE_PLAYER_SUMMONS))
        return true;
    if (isMonster(creature) && type != Proto::CreatureTypeSummonOwn && type != Proto::CreatureTypeSummonOther && has(Filter::HIDE_MONSTERS))
        return true;
    return false;
}

}

// ---------------------------------------------------------------------------

void TBattleListDataModel::setEntries(std::vector<BattleListEntry> entries)
{
    const bool sameRows = entries.size() == m_entries.size()
        && std::equal(entries.begin(), entries.end(), m_entries.begin(),
                      [](const BattleListEntry& a, const BattleListEntry& b) { return a.creatureId == b.creatureId; });
    if (!sameRows) {
        beginResetModel();
        m_entries = std::move(entries);
        endResetModel();
        return;
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i] == m_entries[i])
            continue;
        m_entries[i] = std::move(entries[i]);
        const QModelIndex row = index(static_cast<int>(i));
        Q_EMIT dataChanged(row, row);
    }
}

int TBattleListDataModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TBattleListDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
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
        case 0x11a: return e.vocation;
        default: return {};
    }
}

TBattleListDataModelSortFilterProxy::TBattleListDataModelSortFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void TBattleListDataModelSortFilterProxy::setFilters(std::set<Filter> filters)
{
    if (filters == m_filters)
        return;
    m_filters = std::move(filters);
    invalidateFilter();
}

void TBattleListDataModelSortFilterProxy::setSortOrder(std::array<int, 2> order)
{
    if (order == m_sortOrder)
        return;
    m_sortOrder = order;
    invalidate();
    sort(0);
}

// b1/46876bdcccb0a9e1
bool TBattleListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    if (m_filters.empty())
        return true;
    const auto creatureId = static_cast<creatures::TCreatureID>(source->data(index, 0x101).toUInt());
    if (creatureId == 0)
        return true;
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    const TCreaturePtr creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature)
        return true;
    return !hiddenByFilter(*creature, m_filters);
}

// b1/e45137a1184e032e
bool TBattleListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const auto roleUInt = [&](const QModelIndex& index, const int role) {
        return source->data(index, role).toUInt();
    };
    const auto roleFloat = [&](const QModelIndex& index, const int role) {
        return source->data(index, role).toFloat();
    };
    const auto roleString = [&](const QModelIndex& index, const int role) {
        return source->data(index, role).toString();
    };
    for (const int key : m_sortOrder) {
        switch (key) {
            case BattleListOptions::ByAgeAscending: {
                const auto a = roleUInt(left, 0x112);
                const auto b = roleUInt(right, 0x112);
                if (a != b)
                    return a < b;
                break;
            }
            case BattleListOptions::ByAgeDescending: {
                const auto a = roleUInt(left, 0x112);
                const auto b = roleUInt(right, 0x112);
                if (a != b)
                    return a > b;
                break;
            }
            case BattleListOptions::ByDistanceAscending: {
                const auto a = roleUInt(left, 0x113);
                const auto b = roleUInt(right, 0x113);
                if (a != b)
                    return a < b;
                break;
            }
            case BattleListOptions::ByDistanceDescending: {
                const auto a = roleUInt(left, 0x113);
                const auto b = roleUInt(right, 0x113);
                if (a != b)
                    return a > b;
                break;
            }
            case BattleListOptions::ByHitpointsAscending: {
                const float a = roleFloat(left, 0x103);
                const float b = roleFloat(right, 0x103);
                if (a != b)
                    return a < b;
                break;
            }
            case BattleListOptions::ByHitpointsDescending: {
                const float a = roleFloat(left, 0x103);
                const float b = roleFloat(right, 0x103);
                if (a != b)
                    return a > b;
                break;
            }
            case BattleListOptions::ByNameAscending: {
                const int c = QString::compare(roleString(left, 0x102), roleString(right, 0x102), Qt::CaseSensitive);
                if (c != 0)
                    return c < 0;
                break;
            }
            case BattleListOptions::ByNameDescending: {
                const int c = QString::compare(roleString(left, 0x102), roleString(right, 0x102), Qt::CaseSensitive);
                if (c != 0)
                    return c > 0;
                break;
            }
            default:
                break;
        }
    }
    return roleUInt(left, 0x101) < roleUInt(right, 0x101);
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

TBattleListController::TBattleListController(const int battleListId, TBattleListControllerStorage* storage, QQuickItem* gameWindowItem,
                                             QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_id(battleListId)
    , m_storage(storage)
    , m_model(this)
    , m_proxy(new TBattleListDataModelSortFilterProxy(this))
    , m_renameDialog(new TGenericEnterTextDialogController(this))
{
    creatures::ownProductSurface(this);
    m_renameDialog->setCaption(qtTrId("battlelist_rename"));
    m_renameDialog->setDescription(qtTrId("battlelist_rename_text"));
    m_renameDialog->setMaxInputLength(30);
    connect(m_renameDialog, &TGenericEnterTextDialogController::textEntered, this, &TBattleListController::onNewNameEntered);
    m_proxy->setSourceModel(&m_model);
    m_battleListModel = QVariant::fromValue<QObject*>(m_proxy);
    m_options = storage->options(battleListId);
    m_proxy->setFilters(m_options.filters);
    m_proxy->setSortOrder({ static_cast<int>(m_options.sortOrder[0]), static_cast<int>(m_options.sortOrder[1]) });
    m_filterButtonsVisible = m_options.showFilters;
    m_isPartyView = m_options.isPartyView;
    m_name = m_options.name;
    m_isPrimary = m_options.isPrimary;
    m_hasPremiumStatus = storage->hasPremium();
    widget()->setInitialContentHeightAndMaximized(m_options.contentHeight, m_options.contentMaximized);

    if (TContextMenuController* menu = TContextMenuController::instance())
        connect(menu, &TContextMenuController::contextMenuClosed, this, &TBattleListController::onContextMenuControllerMenuClosed);

    widget()->createItem(QStringLiteral("battlelist.qml"));
    QQuickItem* item = quickItem();
    if (!item)
        return;
    connect(item, SIGNAL(currentlyHoveredCreatureChanged(int)), this, SLOT(onCurrentlyHoveredCreatureChanged(int)));
    connect(item, SIGNAL(creatureClicked(int, int, int)), this, SLOT(onCreatureClicked(int, int, int)));
    connect(item, SIGNAL(creatureTargetSelected(int)), this, SLOT(onCreatureTargetSelected(int)));
    for (const A type : { A::USE_AT_MOUSECURSOR, A::SPELL_AT_MOUSE_CURSOR })
        m_actionListeners.push_back(input::addActionListener(type, [this](const input::TGameAction& action) { return onAtMouseCursor(action); }));
}

TBattleListController::~TBattleListController()
{
    for (const int id : m_actionListeners)
        input::removeActionListener(id);
}

void TBattleListController::handleGameAction()
{
    // CIP moc shared_ptr dispatch; qtclient routes via m_mapper / addActionListener.
}

void TBattleListController::handleGameActionAtMouseCursor()
{
    // Live path: USE_AT_MOUSECURSOR listener → onAtMouseCursor.
}

void TBattleListController::handleGameActionCastSpellAtMouseCursor()
{
    // Live path: SPELL_AT_MOUSE_CURSOR listener → onAtMouseCursor.
}

// b1/96efa0113d56c24b
bool TBattleListController::onAtMouseCursor(const input::TGameAction& action)
{
    if (!m_hoveredCreature)
        return false;
    input::TGameActionPtr target;
    for (const input::TGameActionPtr& candidate : input::TGenericObjectGameActionProvider::creatureActions(m_hoveredCreature)) {
        if (candidate->type == A::TARGET_SELECTED) {
            target = candidate;
            break;
        }
    }
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
    if (m_proxy) {
        m_proxy->setFilters(options.filters);
        m_proxy->setSortOrder({ static_cast<int>(options.sortOrder[0]), static_cast<int>(options.sortOrder[1]) });
    }
    m_filterButtonsVisible = options.showFilters;
    m_isPartyView = options.isPartyView;
    m_name = options.name;
    m_isPrimary = options.isPrimary;
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

bool TBattleListController::filterButtonsVisible() const { return m_filterButtonsVisible; }
bool TBattleListController::isPartyView() const { return m_isPartyView; }
QString TBattleListController::name() const { return m_name; }
bool TBattleListController::isPrimary() const { return m_isPrimary; }
bool TBattleListController::hasPremiumStatus() const { return m_hasPremiumStatus; }
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

void TBattleListController::setOptions(const BattleListOptions& options)
{
    m_storage->setOptions(m_id, options);
    m_storage->triggerUpdate();
}

bool TBattleListController::isFilterActive(const EBattleListWidgetFilterKey filterKey) { return m_options.filters.contains(filterKey); }

void TBattleListController::toggleFilter(const EBattleListWidgetFilterKey filterKey)
{
    BattleListOptions options = m_options;
    if (!options.filters.erase(filterKey))
        options.filters.insert(filterKey);
    setOptions(options);
}

void TBattleListController::onFilterButtonsVisibleClicked()
{
    BattleListOptions options = m_options;
    options.showFilters = !options.showFilters;
    setOptions(options);
}

void TBattleListController::saveWidgetOptions()
{
    BattleListOptions options = m_options;
    options.contentHeight = widget()->maximizedContentHeight();
    options.contentMaximized = widget()->isMaximized();
    m_storage->setOptions(m_id, options);
}

void TBattleListController::onRequestClose() { Q_EMIT battleListClosed(m_id); }

// b1/99bcca1b6628ea53
void TBattleListController::requestOpenSecondaryBattleList()
{
    if (m_hasPremiumStatus) {
        auto action = input::mutableClone(*input::makeAction(A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR));
        action->instance = 0xff;
        action->sidebarId = 0xff;
        input::publish(action);
        return;
    }
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TBattleListController::requestEditName()
{
    m_renameDialog->setText(m_options.name);
    m_renameDialog->dialog()->show();
}

// b1/aa102b1bbf1aa7eb
void TBattleListController::onNewNameEntered(const QString& EnteredName)
{
    BattleListOptions options = m_options;
    options.name = EnteredName.toHtmlEscaped();
    options.contentHeight = widget()->maximizedContentHeight();
    options.contentMaximized = widget()->isMaximized();
    m_storage->setOptions(m_id, options);
}

void TBattleListController::requestMakePrimary()
{
    auto action = input::mutableClone(*input::makeAction(A::MAKE_BATTLE_LIST_PRIMARY));
    action->instance = m_id;
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

// b1/dbf94434aef8baef
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
            menu->addCustomEntry(qtTrId("battlelist_open_secondary"), m_hasPremiumStatus ? QString() : qtTrId("context_premium_feature"));
        connect(secondary, &TContextMenuEntryItem::triggered, this, &TBattleListController::requestOpenSecondaryBattleList);
        if (!m_options.isPrimary) {
            connect(menu->addCustomEntry(qtTrId("battlelist_make_primary")), &TContextMenuEntryItem::triggered, this,
                    &TBattleListController::requestMakePrimary);
        }
    }
    menu->addCustomSeparator();
    for (int i = 0; i < static_cast<int>(std::size(SortMenuEntries)); ++i) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(SortMenuEntries[i]));
        entry->setCheckable(true);
        entry->setChecked(m_options.sortOrder[0] == i);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, i] {
            BattleListOptions options = m_options;
            options.sortOrder = { static_cast<BattleListOptions::Sort>(i), m_options.sortOrder[0] };
            setOptions(options);
        });
    }
    menu->showCustomMenu();
}

int TBattleListController::mapperContext() const { return m_options.isPartyView ? PartyListContext : BattleListContext; }

void TBattleListController::temporarySelect(const uint32_t creatureId)
{
    for (const input::TGameActionPtr& action : input::TGenericObjectGameActionProvider::creatureActions(creatureId)) {
        if (action->type == A::TEMPORARY_SELECT_CREATURE) {
            input::publish(action);
            break;
        }
    }
}

// b1/a0d7c3898eb924a2
void TBattleListController::onCurrentlyHoveredCreatureChanged(const int creatureId)
{
    const bool entered = m_hoveredCreature == 0 && creatureId != 0;
    const bool exited = m_hoveredCreature != 0 && creatureId == 0;
    m_hoveredCreature = static_cast<uint32_t>(creatureId);
    if (Cursor* cursor = Cursor::instance()) {
        if (entered)
            cursor->onBattleListEntered();
        else if (exited)
            cursor->onBattleListExited();
        if (m_hoveredCreature) {
            const int modifiers = static_cast<int>(QGuiApplication::queryKeyboardModifiers());
            const input::TGameActionPtr action = m_mapper.map(input::TGenericObjectGameActionProvider::creatureActions(m_hoveredCreature),
                                                              mapperContext(), Qt::LeftButton, modifiers);
            cursor->setLayerShape(Cursor::BattleList, action ? worldmap::TWorldMapGameActionProvider::cursorShapeOf(action) : Cursor::Default);
        }
    }
    temporarySelect(m_lockedCreature ? m_lockedCreature : m_hoveredCreature);
}

// b1/d150c456206861d7
void TBattleListController::onCreatureClicked(const int creatureId, const int mouseButton, const int keyboardModifiers)
{
    const input::TGameActionList candidates = input::TGenericObjectGameActionProvider::creatureActions(static_cast<uint32_t>(creatureId));
    input::TGameActionList withMenu = candidates;
    withMenu.push_back(input::makeAction(A::CONTEXT_MENU));
    const input::TGameActionPtr action =
        m_mapper.map(withMenu, mapperContext(), static_cast<Qt::MouseButton>(mouseButton), keyboardModifiers);
    if (!action)
        return;
    if (action->type != A::CONTEXT_MENU) {
        input::publish(action);
        return;
    }
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    m_lockedCreature = static_cast<uint32_t>(creatureId);
    temporarySelect(m_lockedCreature);
    if (const input::TGameActionList entries = input::contextMenuActions(candidates, ContextMenuOrder); !entries.empty())
        menu->showGameActions(entries, mapperContext(), m_mapper);
}

void TBattleListController::onContextMenuControllerMenuClosed()
{
    if (!m_lockedCreature)
        return;
    m_lockedCreature = 0;
    temporarySelect(m_hoveredCreature);
}

void TBattleListController::onCreatureTargetSelected(const int creatureId)
{
    for (const input::TGameActionPtr& action : input::TGenericObjectGameActionProvider::creatureActions(static_cast<uint32_t>(creatureId))) {
        if (action->type == A::TARGET_SELECTED) {
            input::publish(action);
            return;
        }
    }
}

// ---------------------------------------------------------------------------

namespace {
TBattleListControllerStorage* g_battleListStorage = nullptr;
}

TBattleListControllerStorage::TBattleListControllerStorage(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : cip::tibia::gamewindow::TBattleListControllerStorage(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    g_battleListStorage = this;
    m_updateTimer.setInterval(UpdateIntervalMs);
    connect(&m_updateTimer, &QTimer::timeout, this, &TBattleListControllerStorage::refresh);

    input::setActionHandler(A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, [this](const input::TGameAction& action) {
        if (action.instance >= 0 && action.instance < MaxBattleLists && m_options.contains(action.instance))
            open(action.instance, action.sidebarId);
        else
            openSecondary(action.sidebarId);
        return true;
    });
    input::setActionHandler(A::MAKE_BATTLE_LIST_PRIMARY, [this](const input::TGameAction& action) {
        makePrimary(action.instance);
        return true;
    });
    // CIP TCreaturesGameActionHandler case 0x27 -> : the
    // creature storage's hover id (TCreatureStorage swaps the target
    // frames to the hover colours), then the world map's mouse target (v17).
    // The renderer draws both from its mouse target, which CIP only sets with
    // mouseHighlightTarget on; a creature mouse target has no sprite highlight,
    // so setting it always gives the same frames.
    input::setActionHandler(A::TEMPORARY_SELECT_CREATURE, [this](const input::TGameAction& action) {
        setTemporarySelectedCreature(action.creatureId);
        if (m_worldMapRenderProvider)
            m_worldMapRenderProvider->setMouseTargetCreatureId(action.creatureId);
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
    for (auto& [id, controller] : m_controllers)
        delete controller;
}

TBattleListControllerStorage* TBattleListControllerStorage::instance()
{
    return g_battleListStorage;
}

void TBattleListControllerStorage::setSidebarOptions(config::TSidebarOptions* options)
{
    m_sidebarOptions = options;
    readOptions();
    for (const auto& [id, controller] : m_controllers)
        controller->applyOptions(this->options(id));
}

void TBattleListControllerStorage::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    for (const auto& [id, controller] : m_controllers)
        controller->setControlScheme(controlScheme);
}

void TBattleListControllerStorage::readOptions()
{
    m_options.clear();
    if (m_sidebarOptions) {
        const QJsonObject json = m_sidebarOptions->section(QLatin1String(OptionsKey)).toObject();
        for (auto it = json.begin(); it != json.end(); ++it) {
            bool ok = false;
            const int id = it.key().toInt(&ok);
            if (ok)
                m_options[id] = parseOptions(it.value().toObject());
        }
    }
    // CIP's defaults: the primary list 0 and the party list.
    if (std::none_of(m_options.begin(), m_options.end(), [](const auto& entry) { return entry.second.isPrimary; })) {
        BattleListOptions primary = m_options[0];
        primary.isPrimary = true;
        primary.isPartyView = false;
        m_options[0] = primary;
    }
    if (!m_options.contains(PartyListId)) {
        BattleListOptions party;
        party.isPartyView = true;
        m_options[PartyListId] = party;
    }
}

void TBattleListControllerStorage::writeOptions()
{
    if (!m_sidebarOptions)
        return;
    QJsonObject json;
    for (const auto& [id, options] : m_options)
        json.insert(QString::number(id), serializeOptions(options));
    m_sidebarOptions->setSection(QLatin1String(OptionsKey), json);
}

int TBattleListControllerStorage::primaryId() const
{
    for (const auto& [id, options] : m_options) {
        if (options.isPrimary)
            return id;
    }
    return 0;
}

const BattleListOptions& TBattleListControllerStorage::options(const int battleListId) const
{
    static const BattleListOptions none;
    const auto it = m_options.find(battleListId);
    return it != m_options.end() ? it->second : none;
}

void TBattleListControllerStorage::setOptions(const int battleListId, const BattleListOptions& options)
{
    m_options[battleListId] = options;
    if (const auto it = m_controllers.find(battleListId); it != m_controllers.end())
        it->second->applyOptions(options);
    writeOptions();
}

void TBattleListControllerStorage::removeOptions(const int battleListId)
{
    if (m_options.erase(battleListId))
        writeOptions();
}

bool TBattleListControllerStorage::hasPremium() const
{
    return tibia::game::localPlayer() && tibia::game::localPlayerIsPremium();
}

TBattleListController* TBattleListControllerStorage::open(const int battleListId, const uint sidebarId)
{
    if (!m_sidebars)
        return nullptr;
    if (!m_options.contains(battleListId)) {
        BattleListOptions options;
        options.isPartyView = battleListId == PartyListId;
        m_options[battleListId] = options;
        writeOptions();
    }
    TBattleListController* controller = nullptr;
    if (const auto it = m_controllers.find(battleListId); it != m_controllers.end()) {
        controller = it->second;
        m_sidebars->removeWidget(controller->widget());
    } else {
        controller = new TBattleListController(battleListId, this, m_gameWindowItem, m_engine);
        controller->setControlScheme(m_controlScheme);
        connect(controller, &TBattleListController::battleListClosed, this, [this](const int id) {
            if (const auto it = m_controllers.find(id); it != m_controllers.end())
                it->second->removeThisBattleListOptionsIfNotSpecialBattleList();
            close(id);
        });
        connect(controller, &TBattleListController::widgetClosed, this, [this, battleListId] {
            if (m_controllers.contains(battleListId))
                close(battleListId);
        });
        m_controllers[battleListId] = controller;
    }
    if (sidebarId != TSidebarWidget::NoSidebar && m_sidebars->hasSidebar(sidebarId))
        m_sidebars->addWidget(sidebarId, controller->widget());
    else
        m_sidebars->addWidget(controller->widget());
    controller->widget()->show();
    emitVisibility(battleListId, true);
    refresh();
    if (!m_updateTimer.isActive())
        m_updateTimer.start();
    return controller;
}

void TBattleListControllerStorage::close(const int battleListId)
{
    const auto it = m_controllers.find(battleListId);
    if (it == m_controllers.end())
        return;
    TBattleListController* controller = it->second;
    if (m_options.contains(battleListId))
        controller->saveWidgetOptions();
    m_controllers.erase(it);
    if (m_sidebars)
        m_sidebars->removeWidget(controller->widget());
    controller->widget()->hide();
    controller->deleteLater();
    emitVisibility(battleListId, false);
    if (m_controllers.empty())
        m_updateTimer.stop();
    Q_EMIT battleListRemovedFromStorage();
}

void TBattleListControllerStorage::closeAll()
{
    while (!m_controllers.empty())
        close(m_controllers.begin()->first);
}

std::vector<std::pair<int, TSidebarWidget*>> TBattleListControllerStorage::widgets() const
{
    std::vector<std::pair<int, TSidebarWidget*>> result;
    for (const auto& [id, controller] : m_controllers)
        result.emplace_back(id, controller->widget());
    return result;
}

void TBattleListControllerStorage::emitVisibility(const int battleListId, const bool visible)
{
    const BattleListOptions& o = options(battleListId);
    int type = -1;
    if (o.isPartyView)
        type = 2;
    else if (o.isPrimary)
        type = 1;
    if (type < 0)
        return;
    if (visible)
        Q_EMIT widgetVisible(type);
    else
        Q_EMIT widgetHidden(type);
}

void TBattleListControllerStorage::openSecondary(const uint sidebarId)
{
    for (int id = 0; id < MaxBattleLists; ++id) {
        if (!m_options.contains(id)) {
            open(id, sidebarId);
            return;
        }
    }
}

void TBattleListControllerStorage::makePrimary(const int battleListId)
{
    if (!m_options.contains(battleListId) || battleListId == PartyListId)
        return;
    const int previous = primaryId();
    if (previous == battleListId)
        return;
    const bool previousOpen = isOpen(previous);
    const bool open = isOpen(battleListId);
    if (previousOpen)
        emitVisibility(previous, false);
    m_options[previous].isPrimary = false;
    m_options[battleListId].isPrimary = true;
    for (const int id : { previous, battleListId }) {
        if (const auto it = m_controllers.find(id); it != m_controllers.end())
            it->second->applyOptions(m_options[id]);
    }
    if (open)
        emitVisibility(battleListId, true);
    writeOptions();
}

void TBattleListControllerStorage::setTemporarySelectedCreature(const uint32_t creatureId)
{
    if (creatureId == m_temporarySelected)
        return;
    m_temporarySelected = creatureId;
    refresh();
}

void TBattleListControllerStorage::triggerUpdate() { refresh(); }

void TBattleListControllerStorage::onLoginRampUpFinished()
{
    m_visibleOrder.clear();
    const bool premium = hasPremium();
    for (const auto& [id, controller] : m_controllers)
        controller->setPremium(premium);
    refresh();
}

std::vector<TCreaturePtr> TBattleListControllerStorage::creatures(const BattleListOptions& options) const
{
    std::vector<TCreaturePtr> result;
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    if (!tibia::game::localPlayer() || !storage)
        return result;
    const Position center = tibia::game::localPlayerPosition();
    const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
    for (const TCreaturePtr& creature : storage->spectatorsNear(center, true, range.left, range.right, range.top, range.bottom)) {
        if (!creature)
            continue;
        const Position pos = creature->position();
        if (options.isPartyView) {
            if (!creature->isLocalPlayer() && !isPartyMember(*creature))
                continue;
        } else {
            if (creature->isLocalPlayer() || pos.z != center.z)
                continue;
            if (std::abs(pos.x - center.x) >= range.left || std::abs(pos.y - center.y) >= range.top)
                continue;
        }
        // Hide-keys run in TBattleListDataModelSortFilterProxy::filterAcceptsRow
        // (CIP); source model keeps the full spectator set.
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
    if (tibia::game::attackingCreatureId() == creature->creatureId())
        e.frameColors.append(QColor(0xff, 0x00, 0x00));
    else if (tibia::game::getFollowingCreatureId() == creature->creatureId())
        e.frameColors.append(QColor(0x00, 0xff, 0x00));
    for (const auto& [icon, category, count] : creature->hudIcons()) {
        if (category != 0 && icon != 0)
            e.statusIcons.append(stripIcon("othercreatureicons", icon));
        e.fiendish |= category != 0 && icon == 5;
    }
    // Echo Raid glow ↔ CIP leader/minion status marks (see creaturehud.cpp).
    e.leader = creature->echoRaidColor() == 0;
    e.leaderMinion = creature->echoRaidColor() == 1;
    uint8_t summon = 0;
    if (creature->creatureType() == Proto::CreatureTypeSummonOwn)
        summon = 1;
    else if (creature->creatureType() == Proto::CreatureTypeSummonOther)
        summon = 2;
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
    e.temporarySelected = m_temporarySelected == e.creatureId;
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
    refresh();
}

void TBattleListControllerStorage::clearCreatureTutorialMarkers()
{
    if (m_gameActionHandler)
        m_gameActionHandler->clearCreatureTutorialMarkers();
    refresh();
}

void TBattleListControllerStorage::refresh()
{
    creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance();
    if (!tibia::game::localPlayer() || !storage || m_controllers.empty())
        return;

    std::set<uint32_t> present;
    const auto ar = tibia::game::awareRange();
    const AwareRange range{ ar.left, ar.top, ar.right, ar.bottom };
    const Position center = tibia::game::localPlayerPosition();
    for (const TCreaturePtr& creature : storage->spectatorsNear(center, true, range.left, range.right, range.top, range.bottom)) {
        if (!creature)
            continue;
        present.insert(creature->creatureId());
        if (!m_visibleOrder.contains(creature->creatureId()))
            m_visibleOrder[creature->creatureId()] = m_nextVisibleOrder++;
    }
    std::erase_if(m_visibleOrder, [&present](const auto& entry) { return !present.contains(entry.first); });

    const bool premium = hasPremium();
    for (const auto& [id, controller] : m_controllers) {
        std::vector<BattleListEntry> entries;
        for (const TCreaturePtr& creature : creatures(options(id)))
            entries.push_back(entryOf(creature));
        controller->setEntries(std::move(entries));
        controller->setPremium(premium);
    }
}

// b1/1c66f7eeb1ce6a63
bool TBattleListControllerStorage::attackFirstTarget()
{
    const uint32_t targetId = tibia::game::attackingCreatureId();
    for (const TCreaturePtr& creature : creatures(options(primaryId()))) {
        if (creature->isNpc() || creature->creatureId() == targetId || creature->isLocalPlayer())
            continue;
        input::publish(input::makeCreatureAction(A::ATTACK_CREATURE, creature->creatureId()));
        return true;
    }
    return true;
}

bool TBattleListControllerStorage::attackNextOrPreviousTarget(const bool previous)
{
    std::vector<TCreaturePtr> list = creatures(options(primaryId()));
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
        if (creature->isNpc() || creature->creatureId() == targetId || creature->isLocalPlayer())
            continue;
        input::publish(input::makeCreatureAction(A::ATTACK_CREATURE, creature->creatureId()));
        return true;
    }
    return true;
}

}
