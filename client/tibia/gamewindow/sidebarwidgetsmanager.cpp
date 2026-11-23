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
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"

#include "config/characterconfigurationmanager.h"
#include "container/containerciptypes.h"
#include "gamewindow/bosstrackerwidgetcontroller.h"
#include "gamewindow/creaturetrackerwidgetcontroller.h"
#include "gamewindow/depotsearchwidgetcontroller.h"
#include "gamewindow/imbuementtrackerwidgetcontroller.h"
#include "gamewindow/killtrackerwidgetcontroller.h"
#include "gamewindow/loottrackingwidgetcontroller.h"
#include "gamewindow/playerguidewidgetcontroller.h"
#include "gamewindow/playertradecontroller.h"
#include "gamewindow/questtrackerwidgetcontroller.h"
#include "gamewindow/spelllistwidgetcontroller.h"
#include "gamewindow/unjustifiedpointswidgetcontroller.h"
#include "gamewindow/vipwidgetcontroller.h"
#include "trade/tradeciptypes.h"

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

TSidebarWidgetsManager::TSidebarWidgetsManager(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                               analysers::TAnalyserStorages* analyserStorages, QObject* parent)
    : cip::tibia::gamewindow::TSidebarWidgetsManager(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
    , m_battleLists(std::make_unique<TBattleListControllerStorage>(sidebars, gameWindowItem, engine))
{
    // CIP sidebar base carriers.
    new TSidebarWidgetControllerBase(gameWindowItem, engine, this);
    new TSidebarWidgetControllerWithFocusHandling(gameWindowItem, engine, this);
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
    connect(m_battleLists.get(), &TBattleListControllerStorage::battleListRemovedFromStorage, this,
            &TSidebarWidgetsManager::saveCurrentOpenWidgets);

    registerWidget(Skills, { [this]() -> TSidebarWidget* {
                                if (!m_skills) {
                                    m_skills = new TSkillsWidgetController(m_gameWindowItem, m_engine);
                                    connect(m_skills, &TSkillsWidgetController::publishGameAction, this,
                                            &TSidebarWidgetsManager::publishGameAction);
                                    m_skills->connectToGameSession(m_gameSession);
                                    m_skills->setPremiumTimeStorage(m_premiumTimeStorage);
                                }
                                return m_skills->widget();
                            },
                             [this] { closeSkills(true); },
                             [this]() -> TSidebarWidget* { return m_skills ? m_skills->widget() : nullptr; },
                             [this] { closeSkills(false); } });

    // The widgets kept by type. CipSoft makes each controller when it opens;
    // qtclient's per-widget managers do that, and their game actions reach
    // the manager's publishGameAction (CIP connects every controller's).
    const auto publish = [this](auto* manager) {
        connect(manager, &std::remove_pointer_t<decltype(manager)>::publishGameAction, this, &TSidebarWidgetsManager::publishGameAction);
    };
    m_vip = std::make_unique<TVipWidgetManager>(gameWindowItem, engine);
    publish(m_vip.get());
    registerWidget(Vip, hooksFor(*m_vip, [vip = m_vip.get()] { return vip->open(); }));
    m_killTracker = std::make_unique<TKillTrackerWidgetManager>(gameWindowItem, engine);
    publish(m_killTracker.get());
    registerWidget(KillTracker, hooksFor(*m_killTracker, [this, w = m_killTracker.get()] { return w->open(m_options); }));
    m_questTracker = std::make_unique<TQuestTrackerWidgetManager>(gameWindowItem, engine);
    publish(m_questTracker.get());
    registerWidget(QuestTracker, hooksFor(*m_questTracker, [this, w = m_questTracker.get()] { return w->open(m_gameSession); }));
    m_lootTracking = std::make_unique<TLootTrackingWidgetManager>(analyserStorages, gameWindowItem, engine);
    publish(m_lootTracking.get());
    registerWidget(LootTracker, hooksFor(*m_lootTracking, [this, w = m_lootTracking.get()] { return w->open(m_options); }));
    m_spellList = std::make_unique<TSpellListWidgetManager>(gameWindowItem, engine);
    publish(m_spellList.get());
    registerWidget(SpellList, hooksFor(*m_spellList, [this, w = m_spellList.get()] { return w->open(m_gameSession); }));
    m_unjustifiedPoints = std::make_unique<TUnjustifiedPointsWidgetManager>(gameWindowItem, engine);
    registerWidget(UnjustifiedPoints, hooksFor(*m_unjustifiedPoints, [this, w = m_unjustifiedPoints.get()] { return w->open(m_options); }));
    m_imbuementTracker = std::make_unique<TImbuementTrackerWidgetManager>(gameWindowItem, engine);
    registerWidget(ImbuementTracker, hooksFor(*m_imbuementTracker, [this, w = m_imbuementTracker.get()] { return w->open(m_options); }));
    // The trackers take the session's tracker storages when they connect.
    using CreatureMode = TCreatureTrackerWidgetController::EWidgetMode;
    m_bestiaryTracker = std::make_unique<TCreatureTrackerWidgetManager>(CreatureMode::Bestiary, nullptr, nullptr, gameWindowItem, engine);
    publish(m_bestiaryTracker.get());
    registerWidget(BestiaryTracker, hooksFor(*m_bestiaryTracker, [this, w = m_bestiaryTracker.get()] { return w->open(m_options); }));
    m_bosstiaryTracker = std::make_unique<TCreatureTrackerWidgetManager>(CreatureMode::Bosstiary, nullptr, nullptr, gameWindowItem, engine);
    publish(m_bosstiaryTracker.get());
    registerWidget(BosstiaryTracker, hooksFor(*m_bosstiaryTracker, [this, w = m_bosstiaryTracker.get()] { return w->open(m_options); }));
    m_bossTracker = std::make_unique<TBossTrackerWidgetManager>(nullptr, gameWindowItem, engine);
    publish(m_bossTracker.get());
    registerWidget(BossCooldowns, hooksFor(*m_bossTracker, [this, w = m_bossTracker.get()] { return w->open(m_options); }));
    m_playerGuide = std::make_unique<TPlayerGuideWidgetManager>(gameWindowItem, engine);
    publish(m_playerGuide.get());
    registerWidget(PlayerGuide, hooksFor(*m_playerGuide, [this, w = m_playerGuide.get()] { return w->open(m_options); }));

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
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    delete m_skills;
    m_skills = nullptr;
    m_battleLists.reset();
    delete m_depotSearch.data();
    delete m_playerTrade.data();
    m_playerGuide.reset();
    m_imbuementTracker.reset();
    m_bossTracker.reset();
    m_bosstiaryTracker.reset();
    m_bestiaryTracker.reset();
    m_questTracker.reset();
    m_lootTracking.reset();
    m_spellList.reset();
    m_killTracker.reset();
    m_unjustifiedPoints.reset();
    m_vip.reset();
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
    m_vip->setSidebarOptions(options);
    if (m_depotSearch)
        m_depotSearch->setSidebarOptions(options);
    if (!options)
        return;
    connect(options, &config::TSidebarOptions::sidebarWidgetsManagerOptionsChanged, this,
            &TSidebarWidgetsManager::restoreSidebarConfigurationFromOptions);
    restoreSidebarConfigurationFromOptions();
}

void TSidebarWidgetsManager::setContainerStorage(TContainerStorageController* containers)
{
    if (m_containers)
        disconnect(m_containers, &TContainerStorageController::containerRemovedFromStorage, this,
                   &TSidebarWidgetsManager::saveCurrentOpenWidgets);
    m_containers = containers;
    // CIP (constructor): a container that leaves the storage saves the open widgets.
    if (m_containers)
        connect(m_containers, &TContainerStorageController::containerRemovedFromStorage, this,
                &TSidebarWidgetsManager::saveCurrentOpenWidgets);
}

void TSidebarWidgetsManager::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    m_battleLists->setControlScheme(controlScheme);
    m_vip->setControlScheme(controlScheme);
}

void TSidebarWidgetsManager::onLoginRampUpFinished() { m_battleLists->onLoginRampUpFinished(); }

void TSidebarWidgetsManager::registerWidget(const int widgetType, WidgetHooks hooks) { m_widgets[widgetType] = std::move(hooks); }

void TSidebarWidgetsManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    // CIP: the old options go first, so closing every widget saves nothing.
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = nullptr;
    m_gameSession = GameSession;
    closeAll();
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    if (session) {
        config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get();
        m_options = configuration ? configuration->sidebarOptions() : nullptr;
        using Self = TSidebarWidgetsManager;
        if (trade::TPlayerTradeProtocolMessageHandler* trades = session->playerTradeProtocolMessageHandler().get()) {
            m_gameSessionConnections.push_back(connect(trades, &trade::TPlayerTradeProtocolMessageHandler::requestOpenPlayerTradeWidget,
                                                       this, &Self::onPlayerTradeWidgetOpenRequest));
            m_gameSessionConnections.push_back(connect(trades, &trade::TPlayerTradeProtocolMessageHandler::requestClosePlayerTradeWidget,
                                                       this, &Self::onPlayerTradeWidgetCloseRequest));
        }
        if (configuration)
            m_gameSessionConnections.push_back(connect(configuration, &config::TCharacterConfigurationManager::configurationLoadedForCharacter,
                                                       this, &Self::restoreSidebarConfigurationFromOptions));
        if (m_options)
            m_gameSessionConnections.push_back(connect(m_options.data(), &config::TSidebarOptions::sidebarWidgetsManagerOptionsChanged, this,
                                                       &Self::restoreSidebarConfigurationFromOptions));
        if (container::TContainerProtocolMessageHandler* containers = session->containerProtocolMessageHandler().get()) {
            m_gameSessionConnections.push_back(connect(containers, &container::TContainerProtocolMessageHandler::requestOpenDepotSearchWidget,
                                                       this, &Self::onDepotSearchWidgetOpenRequest));
            m_gameSessionConnections.push_back(connect(containers, &container::TContainerProtocolMessageHandler::requestCloseDepotSearchWidget,
                                                       this, &Self::onDepotSearchWidgetCloseRequest));
        }
    }
    // Host: the battle lists and the VIP widget keep the options themselves.
    m_battleLists->setSidebarOptions(m_options);
    m_vip->setSidebarOptions(m_options);
    if (m_playerTrade)
        m_playerTrade->setGameSession(session ? session->playerTradeStorage().get() : nullptr,
                                      session ? session->playerTradeProtocolMessageHandler().get() : nullptr);
    if (m_depotSearch)
        m_depotSearch->setGameSession(session);
    if (m_skills)
        m_skills->connectToGameSession(m_gameSession);
    for (const auto& [widgetType, hooks] : m_widgets) {
        if (hooks.connectToGameSession)
            hooks.connectToGameSession(GameSession);
    }
    setPremiumTimeStorage(GameSession ? game::TPremiumTimeStorage::liveInstance() : nullptr);
}

void TSidebarWidgetsManager::onPlayerTradeWidgetOpenRequest()
{
    onPlayerTradeWidgetCloseRequest();
    m_playerTrade = new TPlayerTradeController(m_gameWindowItem, m_engine);
    connect(m_playerTrade, &TPlayerTradeController::widgetClosed, this, &TSidebarWidgetsManager::onPlayerTradeWidgetCloseRequest,
            Qt::QueuedConnection);
    connect(m_playerTrade, &cip::tibia::gamewindow::TSidebarWidgetControllerBase::publishGameAction, this,
            &TSidebarWidgetsManager::publishGameAction);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_playerTrade->setGameSession(session ? session->playerTradeStorage().get() : nullptr,
                                  session ? session->playerTradeProtocolMessageHandler().get() : nullptr);
    if (session && session->isLoginRampUpFinished())
        m_playerTrade->widget()->maximize();
    if (!m_sidebars)
        return;
    m_sidebars->addWidget(m_playerTrade->widget());
    m_playerTrade->widget()->show();
}

void TSidebarWidgetsManager::onPlayerTradeWidgetCloseRequest()
{
    TPlayerTradeController* playerTrade = m_playerTrade;
    m_playerTrade = nullptr;
    if (!playerTrade)
        return;
    playerTrade->widget()->hide();
    playerTrade->deleteLater();
}

void TSidebarWidgetsManager::onDepotSearchWidgetOpenRequest()
{
    if (m_depotSearch)
        return;
    m_depotSearch = new TDepotSearchWidgetController(m_gameWindowItem, m_engine);
    connect(m_depotSearch, &cip::tibia::gamewindow::TSidebarWidgetControllerBase::widgetClosed, this,
            &TSidebarWidgetsManager::onDepotSearchWidgetCloseRequest, Qt::QueuedConnection);
    connect(m_depotSearch, &TDepotSearchWidgetController::publishGameAction, this, &TSidebarWidgetsManager::publishGameAction);
    // Host: the widget keeps its options in the character's sidebar options.
    m_depotSearch->setSidebarOptions(m_options);
    auto* session = dynamic_cast<game::TGameSessionBase*>(m_gameSession.get());
    m_depotSearch->setGameSession(session);
    if (session && session->isLoginRampUpFinished())
        m_depotSearch->widget()->maximize();
    if (!m_sidebars)
        return;
    m_sidebars->addWidget(m_depotSearch->widget());
    m_depotSearch->widget()->show();
}

void TSidebarWidgetsManager::onDepotSearchWidgetCloseRequest()
{
    TDepotSearchWidgetController* depotSearch = m_depotSearch;
    m_depotSearch = nullptr;
    if (!depotSearch)
        return;
    depotSearch->saveWidgetOptions();
    depotSearch->widget()->hide();
    depotSearch->deleteLater();
}

void TSidebarWidgetsManager::setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                   &TSidebarWidgetsManager::onPremiumTimeChanged);
    m_premiumTimeStorage = premiumTimeStorage;
    if (m_skills)
        m_skills->setPremiumTimeStorage(premiumTimeStorage);
    if (!m_premiumTimeStorage)
        return;
    connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TSidebarWidgetsManager::onPremiumTimeChanged);
    onPremiumTimeChanged();
}

void TSidebarWidgetsManager::onPremiumTimeChanged()
{
    if (!m_premiumTimeStorage || m_premiumTimeStorage->isPremium() || !currentWidget(HuntingSessionAnalyser))
        return;
    onWidgetClosed(HuntingSessionAnalyser, false);
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
    // CIP: the VIP widget, while open, then the battle lists.
    if (m_vip)
        m_vip->handleGameAction(pGameAction);
    if (m_battleLists)
        m_battleLists->handleGameAction(pGameAction);
}

}
