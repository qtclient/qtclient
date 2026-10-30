#include "sidebarwidgetsmanager.h"

#include "config/sidebaroptions.h"
#include "gamewindow/battlelistcontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/containerstoragecontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/skillswidgetcontroller.h"
#include "input/gameactionhandlers.h"

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
    new TInspectObjectDialogController(this);
    new TInspectPlayerDialogController(this);
    new TLargePerkInformation(this);
    new TTileInformation(this);
    new TVesselInformation(this);
    connect(sidebars, &TSidebarManager::widgetConfigurationChanged, this, &TSidebarWidgetsManager::saveCurrentOpenWidgets);
    connect(m_battleLists.get(), &TBattleListControllerStorage::widgetVisible, this, &TSidebarWidgetsManager::widgetVisible);
    connect(m_battleLists.get(), &TBattleListControllerStorage::widgetHidden, this, &TSidebarWidgetsManager::widgetHidden);

    registerWidget(Skills, { [this]() -> TSidebarWidget* {
                                if (!m_skills) {
                                    m_skills = new TSkillsWidgetController(m_gameWindowItem, m_engine);
                                    m_skills->setSidebarOptions(m_options);
                                }
                                return m_skills->widget();
                            },
                             [this] {
                                 if (!m_skills)
                                     return;
                                 m_skills->saveWidgetOptions();
                                 m_skills->deleteLater();
                                 m_skills = nullptr;
                             },
                             [this]() -> TSidebarWidget* { return m_skills ? m_skills->widget() : nullptr; } });

    input::setActionHandler(A::TOGGLE_WIDGET, [this](const input::TGameAction& action) {
        // Key bindings carry the widget in instance, older publishers in parameter.
        toggleWidget(action.instance != 0 ? action.instance : action.parameter);
        return true;
    });
    input::setActionHandler(A::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR, [this](const input::TGameAction& action) {
        if (!m_containers)
            return false;
        m_containers->openInSidebar(action.instance, action.sidebarId);
        return true;
    });
}

TSidebarWidgetsManager::~TSidebarWidgetsManager()
{
    input::setActionHandler(A::TOGGLE_WIDGET, nullptr);
    input::setActionHandler(A::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR, nullptr);
    delete m_skills;
}

void TSidebarWidgetsManager::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    if (!options)
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

TSidebarWidget* TSidebarWidgetsManager::currentWidget(const int widgetType) const
{
    const auto it = m_widgets.find(widgetType);
    return it != m_widgets.end() && it->second.current ? it->second.current() : nullptr;
}

bool TSidebarWidgetsManager::isWidgetOpen(const int widgetType) const
{
    switch (widgetType) {
        case BattleList:
            return m_battleLists->isOpen(m_battleLists->primaryId());
        case PartyList:
            return m_battleLists->isOpen(TBattleListControllerStorage::PartyListId);
        default:
            return currentWidget(widgetType) != nullptr;
    }
}

uint TSidebarWidgetsManager::resolveOpenSidebarId(const uint sidebarId) const
{
    if (sidebarId != TSidebarWidget::NoSidebar && m_sidebars && m_sidebars->hasSidebar(sidebarId))
        return sidebarId;
    if (m_hudPlacement && m_sidebars && m_sidebars->hasSidebar(TSidebarManager::HudRightId))
        return TSidebarManager::HudRightId;
    return sidebarId;
}

void TSidebarWidgetsManager::setHudPlacementEnabled(const bool on)
{
    m_hudPlacement = on;
}

void TSidebarWidgetsManager::closeAllWidgets()
{
    closeAll();
}

void TSidebarWidgetsManager::openWidget(const int widgetType, const uint sidebarId)
{
    if (!m_sidebars)
        return;
    const uint targetId = resolveOpenSidebarId(sidebarId);
    switch (widgetType) {
        case BattleList:
            m_battleLists->open(m_battleLists->primaryId(), targetId);
            return;
        case PartyList:
            m_battleLists->open(TBattleListControllerStorage::PartyListId, targetId);
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
        m_closeConnections[widgetType] = connect(widget->controller(), &TSidebarWidget::Controller::widgetClosed, this,
                                                 [this, widgetType] { closeWidget(widgetType); });
    }
    if (targetId != TSidebarWidget::NoSidebar && m_sidebars->hasSidebar(targetId))
        m_sidebars->addWidget(targetId, widget);
    else
        m_sidebars->addWidget(widget);
    widget->show();
    Q_EMIT widgetVisible(widgetType);
}

void TSidebarWidgetsManager::closeWidget(const int widgetType)
{
    switch (widgetType) {
        case BattleList:
            m_battleLists->close(m_battleLists->primaryId());
            return;
        case PartyList:
            m_battleLists->close(TBattleListControllerStorage::PartyListId);
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
    if (const WidgetHooks& hooks = m_widgets[widgetType]; hooks.close)
        hooks.close();
    Q_EMIT widgetHidden(widgetType);
}

void TSidebarWidgetsManager::toggleWidget(const int widgetType)
{
    if (isWidgetOpen(widgetType))
        closeWidget(widgetType);
    else
        openWidget(widgetType);
}

QQuickItem* TSidebarWidgetsManager::widgetQuickItem(const int widgetType) const
{
    if (widgetType == BattleList || widgetType == PartyList) {
        const int id = widgetType == PartyList ? TBattleListControllerStorage::PartyListId : m_battleLists->primaryId();
        for (const auto& [listId, widget] : m_battleLists->widgets()) {
            if (listId == id && widget)
                return widget->quickItem();
        }
        return nullptr;
    }
    if (TSidebarWidget* widget = currentWidget(widgetType))
        return widget->quickItem();
    return nullptr;
}

void TSidebarWidgetsManager::closeAll()
{
    for (const auto& [type, hooks] : m_widgets)
        closeWidget(type);
    m_battleLists->closeAll();
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

/// CIP FUN_100bc8330: a sidebar's open widgets in their order, with the
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
    for (const auto& [type, hooks] : m_widgets)
        collect(hooks.current ? hooks.current() : nullptr, { { QStringLiteral("type"), widgetTypeName(type) } });
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

void TSidebarWidgetsManager::saveCurrentOpenWidgets()
{
    if (!m_options || !m_sidebars || m_restoring)
        return;
    config::SidebarWidgetsManagerOptions o;
    o.leftSidebarCount = m_sidebars->sidebarCount(TSidebarManager::Left);
    for (const uint id : m_sidebars->classicSidebarIds())
        o.openWidgetsOrderPerSidebar.append(serializeSidebar(id));
    m_options->setSidebarWidgetsManagerOptions(o);
}

void TSidebarWidgetsManager::restoreSidebarConfigurationFromOptions()
{
    if (!m_options || !m_sidebars || m_restoring)
        return;
    const config::SidebarWidgetsManagerOptions saved = m_options->sidebarWidgetsManagerOptions();
    config::SidebarWidgetsManagerOptions current;
    current.leftSidebarCount = m_sidebars->sidebarCount(TSidebarManager::Left);
    for (const uint id : m_sidebars->classicSidebarIds())
        current.openWidgetsOrderPerSidebar.append(serializeSidebar(id));
    if (current == saved)
        return;

    const QScopedValueRollback restoring(m_restoring, true);
    closeAll();
    if (m_containers)
        m_containers->beginRestore();
    const int right = saved.rightSidebarCount();
    m_sidebars->setSidebarCount(TSidebarManager::Right, right);
    m_sidebars->setSidebarCount(TSidebarManager::Left, saved.leftSidebarCount);
    for (int i = 0; i < saved.openWidgetsOrderPerSidebar.size(); ++i) {
        const uint sidebarId = i < right ? static_cast<uint>(i) : TSidebarManager::LeftSidebarIdBase + static_cast<uint>(i - right);
        const QJsonArray widgets = saved.openWidgetsOrderPerSidebar.at(i).toArray();
        for (int position = 0; position < widgets.size(); ++position) {
            const QJsonObject entry = widgets.at(position).toObject();
            const int type = widgetTypeOf(entry.value(QLatin1String("type")).toString());
            const int instance = entry.value(QLatin1String("instance")).toInt();
            if (type == BattleList) {
                if (instance >= 0 && (instance < TBattleListControllerStorage::MaxBattleLists || instance == TBattleListControllerStorage::PartyListId))
                    m_battleLists->open(instance, sidebarId);
            } else if (type == Container) {
                if (m_containers)
                    m_containers->openInSidebar(instance, sidebarId, position);
            } else if (type >= 0) {
                openWidget(type, sidebarId);
            }
        }
    }
    if (m_containers)
        m_containers->endRestore();
}

}
