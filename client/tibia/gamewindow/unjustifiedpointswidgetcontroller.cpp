#include "unjustifiedpointswidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "creatures/creaturestorage.h"

#include <client/staticdata.h>

#include <QJsonObject>
#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUnjustifiedPointsChange", [this](GameObject*, const Args&) {
        refresh();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOpenPvpSituationsChange", [this](GameObject*, const Args&) {
        refresh();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        refresh();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TUnjustifiedPointsWidgetController::onPlayerCreatureAddedToCreatureStorage);
    if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance()) {
        connect(storage, &creatures::TCreatureStorage::playerAdded, this,
                [this](std::weak_ptr<creatures::TCreature>) { onPlayerCreatureAddedToCreatureStorage(); });
    }
    connectPlayerKillerModeChangedToDisplayDataChanged();
    widget()->createItem(QStringLiteral("unjustifiedpointswidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TUnjustifiedPointsWidgetController::onWidgetQuickItemDestroyed);
    refresh();
}

QQuickItem* TUnjustifiedPointsWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TUnjustifiedPointsWidgetController::onCloseClicked() { onRequestClose(); }
void TUnjustifiedPointsWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TUnjustifiedPointsWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TUnjustifiedPointsWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TUnjustifiedPointsWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TUnjustifiedPointsWidgetController::onLevelUpClicked() {}
void TUnjustifiedPointsWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TUnjustifiedPointsWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TUnjustifiedPointsWidgetController::onWidgetQuickItemDestroyed() {}

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

float TUnjustifiedPointsWidgetController::progressDay() const { return m_progressDay; }

uint TUnjustifiedPointsWidgetController::fullKillsToNextSkullDay() const { return m_fullKillsToNextSkullDay; }

float TUnjustifiedPointsWidgetController::progressWeek() const { return m_progressWeek; }

uint TUnjustifiedPointsWidgetController::fullKillsToNextSkullWeek() const { return m_fullKillsToNextSkullWeek; }

float TUnjustifiedPointsWidgetController::progressMonth() const { return m_progressMonth; }

uint TUnjustifiedPointsWidgetController::fullKillsToNextSkullMonth() const { return m_fullKillsToNextSkullMonth; }

uint TUnjustifiedPointsWidgetController::skullDuration() const { return m_skullDuration; }

uint TUnjustifiedPointsWidgetController::openPvpSituationCount() const { return m_openPvpSituationCount; }

QString TUnjustifiedPointsWidgetController::currentSkullSource() const { return m_currentSkullSource; }

QString TUnjustifiedPointsWidgetController::nextSkullSource() const { return m_nextSkullSource; }

void TUnjustifiedPointsWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TUnjustifiedPointsWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

void TUnjustifiedPointsWidgetController::connectPlayerKillerModeChangedToDisplayDataChanged()
{
    if (m_playerKillerConnected)
        return;
    m_playerKillerConnected = true;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSkullChange", [this](GameObject*, const Args&) {
        refresh();
        return std::any{};
    });
}

void TUnjustifiedPointsWidgetController::onPlayerCreatureAddedToCreatureStorage()
{
    connectPlayerKillerModeChangedToDisplayDataChanged();
    refresh();
}

void TUnjustifiedPointsWidgetController::refresh()
{
    const UnjustifiedPoints points = tibia::game::getUnjustifiedPoints();
    m_progressDay = std::clamp(points.killsDay / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullDay = points.killsDayRemaining;
    m_progressWeek = std::clamp(points.killsWeek / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullWeek = points.killsWeekRemaining;
    m_progressMonth = std::clamp(points.killsMonth / 100.0f, 0.0f, 1.0f);
    m_fullKillsToNextSkullMonth = points.killsMonthRemaining;
    m_skullDuration = points.skullTime;
    m_openPvpSituationCount = static_cast<uint>(tibia::game::getOpenPvpSituations());

    const int skull = tibia::game::isOnline() ? tibia::game::localPlayerSkull() : 0;
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
