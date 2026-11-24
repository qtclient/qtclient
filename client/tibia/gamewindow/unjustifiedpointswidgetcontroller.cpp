#include "unjustifiedpointswidgetcontroller.h"
#include "game/gamesessionbase.h"

#include "config/sidebaroptions.h"
#include "creatures/creaturestorage.h"
#include "creatures/creaturesciptypes.h"

#include <client/staticdata.h>

#include <QJsonObject>
#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {


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
    onWidgetOptionsChanged();
}

/// Only the maximized state is restored; the widget keeps its own height.
void TUnjustifiedPointsWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    widget()->setMaximized(m_options->unjustifiedPointsWidgetOptions().contentMaximized);
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

/// The widget's options hold no height (always 0), only whether it is maximized.
void TUnjustifiedPointsWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    config::TWidgetOptionsBase options;
    options.setContentHeightAndMaximized(0, widget()->isMaximized());
    m_options->setUnjustifiedPointsWidgetOptions(options);
}


void TUnjustifiedPointsWidgetController::connectPlayerKillerModeChangedToDisplayDataChanged()
{
    // CIP: the own creature's player killer mark signal.
    disconnect(m_playerKillerMarkChanged);
    m_playerKillerMarkChanged = {};
    const std::shared_ptr<creatures::TCreatureStorage> storage = m_creatureStorage.lock();
    const std::shared_ptr<creatures::TCreature> player = storage ? storage->player() : nullptr;
    if (player)
        m_playerKillerMarkChanged = connect(player.get(), &creatures::TCreature::playerKillerMarkChanged, this, [this] { refresh(); });
}

void TUnjustifiedPointsWidgetController::onPlayerCreatureAddedToCreatureStorage()
{
    connectPlayerKillerModeChangedToDisplayDataChanged();
    refresh();
}

void TUnjustifiedPointsWidgetController::refresh()
{
    // CIP: the progress is the server's percentage, unclamped. No session: all 0.
    const creatures::TUnjustifiedPointsStorage* points = m_unjustifiedPointsStorage.get();
    m_progressDay = points ? static_cast<float>(points->progressDay() * 0.01) : 0.0f;
    m_fullKillsToNextSkullDay = points ? points->fullKillsToNextSkullDay() : 0;
    m_progressWeek = points ? static_cast<float>(points->progressWeek() * 0.01) : 0.0f;
    m_fullKillsToNextSkullWeek = points ? points->fullKillsToNextSkullWeek() : 0;
    m_progressMonth = points ? static_cast<float>(points->progressMonth() * 0.01) : 0.0f;
    m_fullKillsToNextSkullMonth = points ? points->fullKillsToNextSkullMonth() : 0;
    m_skullDuration = points ? points->skullDuration() : 0;
    m_openPvpSituationCount = points ? points->openPvpSituations() : 0;

    // CIP: the current skull shows only when it is red or black; the next one
    // is black after those and red otherwise. Without a player both are empty.
    m_currentSkullSource.clear();
    m_nextSkullSource.clear();
    if (tibia::game::isOnline()) {
        const int skull = tibia::game::localPlayerSkull();
        const bool redOrBlack = (skull & 0xfe) == 4;
        if (redOrBlack)
            m_currentSkullSource = skullSource(skull);
        m_nextSkullSource = skullSource(redOrBlack ? 5 : 4);
    }
    Q_EMIT displayDataChanged();
}

TUnjustifiedPointsWidgetManager::TUnjustifiedPointsWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                 QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TUnjustifiedPointsWidgetManager::~TUnjustifiedPointsWidgetManager() { delete m_controller; }

void TUnjustifiedPointsWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    if (m_controller)
        m_controller->connectToGameSession(m_gameSession);
}

/// Without a session the widget keeps what it shows. With one, the session's
/// unjustified points refresh the display; the player's skull does once the
/// player exists (now, or when the creature storage adds it).
/// Host: the sidebar options come from the manager (setSidebarOptions).
void TUnjustifiedPointsWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    disconnect(m_playerAdded);
    disconnect(m_unjustifiedPointsChanged);
    disconnect(m_playerKillerMarkChanged);
    m_unjustifiedPointsStorage.reset();
    m_creatureStorage.reset();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    if (!session)
        return;
    m_unjustifiedPointsStorage = session->unjustifiedPointsStorage();
    if (m_unjustifiedPointsStorage)
        m_unjustifiedPointsChanged =
            connect(m_unjustifiedPointsStorage.get(), &creatures::TUnjustifiedPointsStorage::unjustifiedPointsDataChanged, this,
                    [this] { refresh(); });
    const std::shared_ptr<creatures::TCreatureStorage>& creatureStorage = session->creatureStorage();
    m_creatureStorage = creatureStorage;
    if (creatureStorage && creatureStorage->player())
        connectPlayerKillerModeChangedToDisplayDataChanged();
    else if (creatureStorage)
        m_playerAdded = connect(creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
                                [this](std::weak_ptr<creatures::TCreature>) { onPlayerCreatureAddedToCreatureStorage(); });
    onWidgetOptionsChanged();
    refresh();
}

TSidebarWidget* TUnjustifiedPointsWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TUnjustifiedPointsWidgetController(m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
        m_controller->connectToGameSession(m_gameSession);
    }
    return m_controller->widget();
}

void TUnjustifiedPointsWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TUnjustifiedPointsWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
