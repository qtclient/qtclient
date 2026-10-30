#include "unjustifiedpointswidgetcontroller.h"

#include "config/sidebaroptions.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <QJsonObject>
#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
constexpr auto OptionsKey = "unjustifiedPointsWidgetOptions";

QString skullSource(const int skull)
{
    if (skull <= 0)
        return {};
    return QStringLiteral("image://creaturestateflags-playerkiller/%1").arg(skull);
}

}

TUnjustifiedPointsWidgetController::TUnjustifiedPointsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                       QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
{
    m_hooks.emplace_back("g_game", "onUnjustifiedPointsChange", [this](LuaObject*, const Args&) {
        refresh();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onOpenPvpSituationsChange", [this](LuaObject*, const Args&) {
        refresh();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        refresh();
        return std::any{};
    });
    widget()->createItem(QStringLiteral("unjustifiedpointswidget.qml"));
    refresh();
}

void TUnjustifiedPointsWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TUnjustifiedPointsWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TUnjustifiedPointsWidgetController::refresh()
{
    const UnjustifiedPoints points = g_game.getUnjustifiedPoints();
    m_progressDay = std::clamp(points.killsDay / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullDay = points.killsDayRemaining;
    m_progressWeek = std::clamp(points.killsWeek / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullWeek = points.killsWeekRemaining;
    m_progressMonth = std::clamp(points.killsMonth / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullMonth = points.killsMonthRemaining;
    m_skullDuration = points.skullTime;
    m_openPvpSituationCount = static_cast<uint>(g_game.getOpenPvpSituations());

    int skull = 0;
    if (const LocalPlayerPtr player = g_game.getLocalPlayer())
        skull = player->getSkull();
    m_currentSkullSource = skullSource(skull);
    m_nextSkullSource = skullSource(skull > 0 ? std::min(skull + 1, 5) : 3);
    Q_EMIT displayDataChanged();
}

TUnjustifiedPointsWidgetManager::TUnjustifiedPointsWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                 QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TUnjustifiedPointsWidgetManager::~TUnjustifiedPointsWidgetManager() { delete m_controller; }

TSidebarWidget* TUnjustifiedPointsWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TUnjustifiedPointsWidgetController(m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TUnjustifiedPointsWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TUnjustifiedPointsWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
