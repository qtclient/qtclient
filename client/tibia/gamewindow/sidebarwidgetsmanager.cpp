#include "sidebarwidgetsmanager.h"

#include "config/sidebaroptions.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/containerstoragecontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/skillswidgetcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"

#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <QJsonObject>
#include <QQuickItem>
#include <QScopedValueRollback>

#include <algorithm>
#include <climits>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

/// sidebars.json's widget type names, by EWidgetType.
constexpr std::pair<int, const char*> WidgetTypeNames[] = {
    { TSidebarWidgetsManager::Skills, "skills" },
    { TSidebarWidgetsManager::BattleList, "battleList" },
    { TSidebarWidgetsManager::PartyList, "partyList" },
    { TSidebarWidgetsManager::Vip, "vip" },
    { TSidebarWidgetsManager::UnjustifiedPoints, "unjustifiedPoints" },
    { TSidebarWidgetsManager::Prey, "prey" },
    { TSidebarWidgetsManager::SpellList, "spellList" },
    { TSidebarWidgetsManager::Container, "container" },
    { TSidebarWidgetsManager::QuestTracker, "questTracker" },
    { TSidebarWidgetsManager::BestiaryTracker, "bestiaryTracker" },
    { TSidebarWidgetsManager::BosstiaryTracker, "bosstiaryTracker" },
    { TSidebarWidgetsManager::BossCooldowns, "bossCooldowns" },
    { TSidebarWidgetsManager::LootTracker, "lootTracker" },
    { TSidebarWidgetsManager::XpAnalyser, "xpAnalyser" },
    { TSidebarWidgetsManager::LootAnalyser, "lootAnalyser" },
    { TSidebarWidgetsManager::WasteAnalyser, "wasteAnalyser" },
    { TSidebarWidgetsManager::ImpactAnalyser, "impactAnalyser" },
    { TSidebarWidgetsManager::DamageInputAnalyser, "damageInputAnalyser" },
    { TSidebarWidgetsManager::HuntingSessionAnalyser, "huntingSessionAnalyser" },
    { TSidebarWidgetsManager::AnalyticsSelector, "analyticsSelector" },
    { TSidebarWidgetsManager::PartyHuntAnalyser, "partyHuntAnalyser" },
    { TSidebarWidgetsManager::ImbuementTracker, "imbuementTracker" },
    { TSidebarWidgetsManager::PlayerGuide, "playerGuide" },
};

}

TSidebarWidgetsManager::TSidebarWidgetsManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : cip::tibia::gamewindow::TSidebarWidgetsManager(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
    , m_battleLists(std::make_unique<TBattleListControllerStorage>(sidebars, gameWindowItem, engine))
{
    // CIP sidebar base carriers (tracker widgets register via managers in main).
    new TSidebarWidgetControllerBase(gameWindowItem, engine, this);
    new TSidebarWidgetControllerWithFocusHandling(gameWindowItem, engine, this);
    // CipSoft DepotSearchWidget is owned by TDepotSearchWidgetManager (main.cpp).
    connect(new TInspectObjectDialogController(this), &TInspectObjectDialogController::publishGameAction, this,
            &TSidebarWidgetsManager::publishGameAction);
    connect(new TInspectPlayerDialogController(this), &TInspectPlayerDialogController::publishGameAction, this,
            &TSidebarWidgetsManager::publishGameAction);
    // TLargePerkInformation / TTileInformation are owned and
    // filled by TSkillWheelPageController (fillLargePerk / fillTile / fillVessel).
    connect(sidebars, &TSidebarManager::widgetConfigurationChanged, this, &TSidebarWidgetsManager::saveCurrentOpenWidgets);
    connect(m_battleLists.get(), &TBattleListControllerStorage::publishGameAction, this,
            &TSidebarWidgetsManager::publishGameAction);
    connect(m_battleLists.get(), &TBattleListControllerStorage::widgetVisible, this, &TSidebarWidgetsManager::widgetVisible);
    connect(m_battleLists.get(), &TBattleListControllerStorage::widgetHidden, this, &TSidebarWidgetsManager::widgetHidden);

    registerWidget(Skills, { [this]() -> TSidebarWidget* {
                                if (!m_skills) {
                                    m_skills = new TSkillsWidgetController(m_gameWindowItem, m_engine);
                                    connect(m_skills, &TSkillsWidgetController::publishGameAction, this,
                                            &TSidebarWidgetsManager::publishGameAction);
                                    m_skills->setSidebarOptions(m_options);
                                    m_skills->connectToGameSession(m_gameSession);
                                }
                                return m_skills->widget();
                            },
                             [this] { closeSkills(true); },
                             [this]() -> TSidebarWidget* { return m_skills ? m_skills->widget() : nullptr; },
                             [this] { closeSkills(false); } });

    connect(this, &TSidebarWidgetsManager::widgetVisible, this, &TSidebarWidgetsManager::onWidgetVisible);
    connect(this, &TSidebarWidgetsManager::widgetHidden, this, &TSidebarWidgetsManager::onWidgetHidden);
    m_gameActions.on(A::TOGGLE_WIDGET, [this](const input::IGameAction& action) { return handleGameAction(action); });
    m_gameActions.on(A::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR, [this](const input::TGameActionOpenContainerInSpecificSidebar& action) {
        if (!m_containers)
            return false;
        m_containers->openInSidebar(action.containerId(), action.sidebarId());
        return true;
    });
}

TSidebarWidgetsManager::~TSidebarWidgetsManager()
{
    delete m_skills;
}

void TSidebarWidgetsManager::setSidebarOptions(config::TSidebarOptions* options)
{
    // CIP: the old options go first, so closing every widget does not save
    // the empty sidebars over them.
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = nullptr;
    closeAll();
    m_options = options;
    m_battleLists->setSidebarOptions(options);
    if (m_skills)
        m_skills->setSidebarOptions(options);
    if (!options)
        return;
    connect(options, &config::TSidebarOptions::sidebarWidgetsManagerOptionsChanged, this,
            &TSidebarWidgetsManager::restoreSidebarConfigurationFromOptions);
    restoreSidebarConfigurationFromOptions();
}

void TSidebarWidgetsManager::setContainerStorage(TContainerStorageController* containers) { m_containers = containers; }

void TSidebarWidgetsManager::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    m_battleLists->setControlScheme(controlScheme);
}

void TSidebarWidgetsManager::onLoginRampUpFinished() { m_battleLists->onLoginRampUpFinished(); }

void TSidebarWidgetsManager::registerWidget(const int widgetType, WidgetHooks hooks) { m_widgets[widgetType] = std::move(hooks); }

void TSidebarWidgetsManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    if (m_skills)
        m_skills->connectToGameSession(m_gameSession);
    for (const auto& [widgetType, hooks] : m_widgets) {
        if (hooks.connectToGameSession)
            hooks.connectToGameSession(GameSession);
    }
}

TSidebarWidget* TSidebarWidgetsManager::currentWidget(const int widgetType) const
{
    const auto it = m_widgets.find(widgetType);
    return it != m_widgets.end() && it->second.current ? it->second.current() : nullptr;
}

bool TSidebarWidgetsManager::isWidgetOpen(const int widgetType) const
{
    switch (widgetType) {
        case BattleList:
            return m_battleLists->primaryBattleList() != nullptr;
        case PartyList:
            return m_battleLists->partyBattleList() != nullptr;
        default:
            return currentWidget(widgetType) != nullptr;
    }
}

void TSidebarWidgetsManager::closeAllWidgets()
{
    closeAll();
}

void TSidebarWidgetsManager::openWidget(const int widgetType, const uint sidebarId)
{
    if (!m_sidebars)
        return;
    switch (widgetType) {
        case BattleList:
            if (!m_battleLists->primaryBattleList())
                m_battleLists->togglePrimaryBattleList(static_cast<uint8_t>(sidebarId));
            return;
        case PartyList:
            if (!m_battleLists->partyBattleList())
                m_battleLists->togglePartyBattleList(static_cast<uint8_t>(sidebarId));
            return;
        default:
            break;
    }
    const auto it = m_widgets.find(widgetType);
    if (it == m_widgets.end() || !it->second.open)
        return;
    TSidebarWidget* widget = currentWidget(widgetType);
    if (widget) {
        m_sidebars->removeWidget(widget);
    } else {
        widget = it->second.open();
        if (!widget)
            return;
        if (QObject* controller = widget->controller()) {
            // Tip NOTIFY deepen: bind widgetClosed on the dynamic (qtclient) moc.
            m_closeConnections[widgetType] =
                connect(controller, SIGNAL(widgetClosed()), this, SLOT(onRegisteredWidgetClosed()));
        }
    }
    m_sidebars->placeWidget(sidebarId, widget);
    widget->show();
    Q_EMIT widgetVisible(widgetType);
}

void TSidebarWidgetsManager::onRegisteredWidgetClosed()
{
    QObject* controller = sender();
    if (!controller)
        return;
    for (const auto& [widgetType, hooks] : m_widgets) {
        TSidebarWidget* widget = hooks.current ? hooks.current() : nullptr;
        if (widget && widget->controller() == controller) {
            onWidgetClosed(widgetType);
            return;
        }
    }
}

void TSidebarWidgetsManager::onWidgetClosed(const int widgetType, const bool saveWidgetOptions)
{
    switch (widgetType) {
        case BattleList:
            if (TBattleListController* primary = m_battleLists->primaryBattleList())
                primary->onRequestClose();
            return;
        case PartyList:
            if (TBattleListController* party = m_battleLists->partyBattleList())
                party->onRequestClose();
            return;
        default:
            break;
    }
    TSidebarWidget* widget = currentWidget(widgetType);
    if (!widget)
        return;
    if (const auto it = m_closeConnections.find(widgetType); it != m_closeConnections.end()) {
        disconnect(it->second);
        m_closeConnections.erase(it);
    }
    if (m_sidebars)
        m_sidebars->removeWidget(widget);
    widget->hide();
    if (const WidgetHooks& hooks = m_widgets[widgetType]; !saveWidgetOptions && hooks.discard)
        hooks.discard();
    else if (hooks.close)
        hooks.close();
    saveCurrentOpenWidgets();
    Q_EMIT widgetHidden(widgetType);
}

void TSidebarWidgetsManager::toggleWidget(const int widgetType, const uint sidebarId)
{
    if (isWidgetOpen(widgetType))
        onWidgetClosed(widgetType, true);
    else
        openWidget(widgetType, sidebarId);
}

bool TSidebarWidgetsManager::handleGameAction(const input::IGameAction& action)
{
    const auto* toggle = input::gameActionAs<input::TGameActionToggleWidget>(&action);
    if (!toggle)
        return false;
    const int widgetType = toggle->widgetType();
    const uint sidebarId = toggle->sidebarId();
    switch (widgetType) {
        case Skills: onToggleSkillsWidget(sidebarId); break;
        case UnjustifiedPoints: onToggleUnjustifiedPointsWidget(sidebarId); break;
        case KillTracker: onToggleKillTrackerWidget(sidebarId); break;
        case SpellList: onToggleSpellListWidget(sidebarId); break;
        case QuestTracker: onToggleQuestTrackerWidget(sidebarId); break;
        case BossCooldowns: onToggleBossTrackerWidget(sidebarId); break;
        case LootTracker: onToggleLootTrackerWidget(sidebarId); break;
        case XpAnalyser: onToggleProgressAnalyserWidget(sidebarId); break;
        case LootAnalyser: onToggleLootAnalyserWidget(sidebarId); break;
        case WasteAnalyser: onToggleWasteAnalyserWidget(sidebarId); break;
        case ImpactAnalyser: onToggleImpactAnalyserWidget(sidebarId); break;
        case DamageInputAnalyser: onToggleDamageInputAnalyserWidget(sidebarId); break;
        case HuntingSessionAnalyser: onToggleHuntingSessionAnalyserWidget(sidebarId); break;
        case AnalyticsSelector: onToggleAnalyticsSelectorWidget(sidebarId); break;
        case PartyHuntAnalyser: onTogglePartyHuntAnalyserWidget(sidebarId); break;
        // The battle lists toggle through the battle list storage (CIP passes
        // it every game action); it toggles the primary and party list in the
        // action's sidebar.
        case BattleList:
        case PartyList: m_battleLists->handleGameAction(action); break;
        // The widgets the manager keeps by type (VIP, bestiary and bosstiary
        // trackers, imbuement tracker, player guide) and pack widgets.
        default: toggleWidget(widgetType, sidebarId); break;
        // CIP toggles no container widget.
        case Container: break;
    }
    return true;
}

void TSidebarWidgetsManager::onWidgetVisible(const int widgetType)
{
    if (m_options && widgetType == ImbuementTracker)
        protocol::gameclient::sendImbuementDurations(true);
}

void TSidebarWidgetsManager::onWidgetHidden(const int widgetType)
{
    if (m_options && widgetType == ImbuementTracker)
        protocol::gameclient::sendImbuementDurations(false);
}

QQuickItem* TSidebarWidgetsManager::widgetQuickItem(const int widgetType) const
{
    if (widgetType == BattleList || widgetType == PartyList) {
        const TBattleListController* list =
            widgetType == PartyList ? m_battleLists->partyBattleList() : m_battleLists->primaryBattleList();
        return list ? list->widget()->quickItem() : nullptr;
    }
    if (TSidebarWidget* widget = currentWidget(widgetType))
        return widget->quickItem();
    return nullptr;
}

void TSidebarWidgetsManager::closeAll()
{
    m_battleLists->closeAll();
    for (const int type : { Skills, UnjustifiedPoints, KillTracker, SpellList, LootTracker, LootAnalyser, WasteAnalyser, BossCooldowns,
                            QuestTracker, ImpactAnalyser, DamageInputAnalyser, XpAnalyser, HuntingSessionAnalyser, AnalyticsSelector,
                            PartyHuntAnalyser })
        onWidgetClosed(type, false);
    std::vector<int> others;
    for (const auto& [type, hooks] : m_widgets)
        others.push_back(type);
    for (const int type : others)
        onWidgetClosed(type, false);
    if (m_sidebars) {
        m_sidebars->setSidebarCount(TSidebarManager::Right, 1);
        m_sidebars->setSidebarCount(TSidebarManager::Left, 0);
    }
}

void TSidebarWidgetsManager::closeSkills(const bool saveWidgetOptions)
{
    if (!m_skills)
        return;
    if (saveWidgetOptions)
        m_skills->saveWidgetOptions();
    m_skills->deleteLater();
    m_skills = nullptr;
}

QString TSidebarWidgetsManager::widgetTypeName(const int widgetType)
{
    for (const auto& [type, name] : WidgetTypeNames) {
        if (type == widgetType)
            return QLatin1String(name);
    }
    return {};
}

int TSidebarWidgetsManager::widgetTypeOf(const QString& name)
{
    for (const auto& [type, typeName] : WidgetTypeNames) {
        if (name == QLatin1String(typeName))
            return type;
    }
    return -1;
}

/// CIP : a sidebar's open widgets in their order, with the
/// restored places of containers that are not open yet at their saved index.
QJsonArray TSidebarWidgetsManager::serializeSidebar(const uint sidebarId) const
{
    std::vector<std::pair<int, QJsonObject>> entries;
    const auto collect = [&](TSidebarWidget* widget, QJsonObject entry) {
        if (widget && widget->parentSidebarId() == sidebarId && widget->identifier() > 0)
            entries.emplace_back(widget->identifier(), std::move(entry));
    };
    for (const auto& [id, widget] : m_battleLists->widgets())
        collect(widget, { { QStringLiteral("instance"), id }, { QStringLiteral("type"), widgetTypeName(BattleList) } });
    for (const auto& [type, hooks] : m_widgets) {
        // qtclient: pack widget types (e.g. the IdleOT minimap, 100) have no
        // options name; parseWidgetsManager drops nameless entries, so storing
        // them would never compare equal and every save would re-run the
        // restore (closeAll) below.
        const QString name = widgetTypeName(type);
        if (!name.isEmpty())
            collect(hooks.current ? hooks.current() : nullptr, { { QStringLiteral("type"), name } });
    }
    if (m_containers) {
        for (const auto& [id, widget] : m_containers->widgets())
            collect(widget, { { QStringLiteral("instance"), id }, { QStringLiteral("type"), widgetTypeName(Container) } });
    }
    std::ranges::stable_sort(entries, {}, &std::pair<int, QJsonObject>::first);
    QJsonArray list;
    for (const auto& [identifier, entry] : entries)
        list.append(entry);
    if (m_containers) {
        std::vector<std::pair<int, int>> pending;
        for (const auto& [id, place] : m_containers->pendingPlaces()) {
            if (place.sidebarId == sidebarId)
                pending.emplace_back(place.position < 0 ? INT_MAX : place.position, id);
        }
        std::ranges::sort(pending);
        for (const auto& [position, id] : pending) {
            list.insert(std::min<qsizetype>(position, list.size()),
                        QJsonObject{ { QStringLiteral("instance"), id }, { QStringLiteral("type"), widgetTypeName(Container) } });
        }
    }
    return list;
}

config::SidebarWidgetsManagerOptions TSidebarWidgetsManager::currentWidgetsManagerOptions() const
{
    config::SidebarWidgetsManagerOptions o;
    if (m_restoring || !m_options || !m_sidebars)
        return o;
    o.leftSidebarCount = m_sidebars->sidebarCount(TSidebarManager::Left);
    for (const uint id : m_sidebars->sidebarIds())
        o.openWidgetsOrderPerSidebar.append(serializeSidebar(id));
    return o;
}

void TSidebarWidgetsManager::saveCurrentOpenWidgets()
{
    if (m_restoring || !m_options || !m_sidebars)
        return;
    m_options->setSidebarWidgetsManagerOptions(currentWidgetsManagerOptions());
}

void TSidebarWidgetsManager::restoreSidebarConfigurationFromOptions()
{
    if (!m_options || !m_sidebars)
        return;
    const config::SidebarWidgetsManagerOptions saved = m_options->sidebarWidgetsManagerOptions();
    if (currentWidgetsManagerOptions() == saved || m_restoring)
        return;

    const QScopedValueRollback restoring(m_restoring, true);
    closeAll();
    if (m_containers)
        m_containers->beginRestore();
    // CIP: one sidebar takes 176 px next to a 240 px map; when the saved ones
    // do not fit the game window, only the main sidebar opens.
    const auto savedCount = static_cast<int>(saved.openWidgetsOrderPerSidebar.size());
    const int wanted = std::max(1, savedCount);
    // qtclient: a pack layout that does not put the sidebars beside the map
    // (a phone sheet) states the width they get as `sidebarRestoreWidth`.
    const qreal roomOverride = m_gameWindowItem ? m_gameWindowItem->property("sidebarRestoreWidth").toReal() : 0.0;
    const qreal width = roomOverride > 0.0 ? roomOverride : (m_gameWindowItem ? m_gameWindowItem->width() : 0.0);
    const int total = (wanted * 176 + 240 <= width || width <= 0.0) ? wanted : 1;
    const int left = saved.leftSidebarCount < total ? saved.leftSidebarCount : 0;
    const int right = total - left;
    m_sidebars->setSidebarCount(TSidebarManager::Right, right);
    m_sidebars->setSidebarCount(TSidebarManager::Left, left);
    const auto restoreSidebar = [this](const uint sidebarId, const QJsonArray& widgets) {
        for (int position = 0; position < widgets.size(); ++position) {
            const QJsonObject entry = widgets.at(position).toObject();
            const int type = widgetTypeOf(entry.value(QLatin1String("type")).toString());
            const int instance = entry.value(QLatin1String("instance")).toInt();
            if (type == BattleList) {
                m_battleLists->openBattleList(static_cast<uint8_t>(sidebarId), static_cast<uint8_t>(instance), false, false);
            } else if (type == Container) {
                if (m_containers)
                    m_containers->openInSidebar(instance, sidebarId, position);
            } else if (type >= 0) {
                openWidget(type, sidebarId);
            }
        }
    };
    for (int i = 0; i < std::min(savedCount, right); ++i)
        restoreSidebar(static_cast<uint>(i), saved.openWidgetsOrderPerSidebar.at(i).toArray());
    for (int i = 0; i < std::min(savedCount - right, left); ++i)
        restoreSidebar(TSidebarManager::LeftSidebarIdBase + static_cast<uint>(i), saved.openWidgetsOrderPerSidebar.at(right + i).toArray());
    if (m_containers)
        m_containers->endRestore();
}


void TSidebarWidgetsManager::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    m_gameActions.handle(pGameAction);
    if (m_battleLists)
        m_battleLists->handleGameAction(pGameAction);
}

}
