#include "screenshot/screenshotciptypes.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "game/gamesession.h"
#include "input/gameactionhandlers.h"
#include "shared/tibiafilesystemhelper.h"

#include <cip_options.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QThreadPool>

namespace tibia::screenshot {

namespace {

using O = cip::EClientOption;

constexpr int BacklogIntervalMs = 997;
constexpr size_t BacklogCapacity = 5;
// Message mode of the "screenshot saved" system message and its sound effect.
constexpr int ScreenshotSavedMessageMode = 0x1e;
constexpr int ScreenshotSavedSoundEffect = 0xad5;

bool optionEnabled(const O option)
{
    const auto name = cip::ClientOptionNames[static_cast<size_t>(option)];
    return app::ClientOptions::load()
        .options()
        .value(QLatin1String(name.data(), static_cast<qsizetype>(name.size())))
        .toBool();
}

uint s_nextOutstandingScreenshotId = 0;

QString eventName(const int event)
{
    switch (event) {
    case TScreenshotManager::EVENT_ACHIEVEMENT: return QStringLiteral("Achievement");
    case TScreenshotManager::EVENT_BESTIARY_ENTRY_COMPLETED: return QStringLiteral("BestiaryEntryCompleted");
    case TScreenshotManager::EVENT_BESTIARY_ENTRY_UNLOCKED: return QStringLiteral("BestiaryEntryUnlocked");
    case TScreenshotManager::EVENT_BOSS_DEFEATED: return QStringLiteral("BossDefeated");
    case TScreenshotManager::EVENT_DEATH_PVE: return QStringLiteral("DeathPvE");
    case TScreenshotManager::EVENT_DEATH_PVP: return QStringLiteral("DeathPvP");
    case TScreenshotManager::EVENT_LEVEL_UP: return QStringLiteral("LevelUp");
    case TScreenshotManager::EVENT_PLAYER_KILL_ASSIST: return QStringLiteral("PlayerKillAssist");
    case TScreenshotManager::EVENT_PLAYER_KILL: return QStringLiteral("PlayerKill");
    case TScreenshotManager::EVENT_PLAYER_ATTACKING: return QStringLiteral("PlayerAttacking");
    case TScreenshotManager::EVENT_TREASURE_FOUND: return QStringLiteral("TreasureFound");
    case TScreenshotManager::EVENT_SKILL_UP: return QStringLiteral("SkillUp");
    case TScreenshotManager::EVENT_GIFT_OF_LIFE: return QStringLiteral("GiftOfLife");
    case TScreenshotManager::EVENT_HIGHEST_DAMAGE_DEALT: return QStringLiteral("HighestDamageDealt");
    case TScreenshotManager::EVENT_HIGHEST_HEALING_DONE: return QStringLiteral("HighestHealingDone");
    case TScreenshotManager::EVENT_VALUABLE_LOOT: return QStringLiteral("ValuableLoot");
    case TScreenshotManager::EVENT_LOW_HEALTH: return QStringLiteral("LowHealth");
    case TScreenshotManager::EVENT_HOTKEY: return QStringLiteral("Hotkey");
    default: return QStringLiteral("Screenshot");
    }
}

}

TScreenshotManager* TScreenshotManager::s_instance = nullptr;

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

TOutstandingScreenshot::TOutstandingScreenshot(const QSharedPointer<QQuickItemGrabResult>& grabResult, const int eventType)
    : m_id(s_nextOutstandingScreenshotId++)
    , m_grabResult(grabResult)
    , m_eventType(eventType)
{
    connect(m_grabResult.data(), &QQuickItemGrabResult::ready, this, [this]() { Q_EMIT ready(m_id); });
}

TScreenshotManager::TScreenshotManager(QObject* parent)
    : cip::tibia::screenshot::TScreenshotManager(parent)
    , m_fileSystemHelper(&files::data())
{
    s_instance = this;
    m_backlogTimer.setInterval(BacklogIntervalMs);
    m_backlogTimer.setSingleShot(false);
    connect(&m_backlogTimer, &QTimer::timeout, this, &TScreenshotManager::onBacklockUpdateTriggered);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            [this](const QString& name) {
                if (name.startsWith(QLatin1String("scre")))
                    onScreenshotMangerOptionsChanged();
            });
    connect(this, &TScreenshotManager::publishGameAction, this,
            [](const input::IGameActionPtr& action) { input::publish(action); });
}

TScreenshotManager::~TScreenshotManager()
{
    if (s_instance == this)
        s_instance = nullptr;
}

TScreenshotManager* TScreenshotManager::instance()
{
    return s_instance;
}

void TScreenshotManager::configure(QQuickWindow* window, QQuickItem* mapWindow)
{
    m_rootItem = window ? window->contentItem() : nullptr;
    m_mapWindow = mapWindow;
    // CIP takes a pending screenshot once per rendered frame.
    if (window)
        connect(window, &QQuickWindow::frameSwapped, this, &TScreenshotManager::takeScreenshotIfEventIsWainting,
                Qt::UniqueConnection);
    // qtclient: the manager lives for one game session.
    setGameSessionActive(true);
}

bool TScreenshotManager::isScreenshotEnabled(const int event)
{
    const bool automatic = optionEnabled(O::screenshotsAutoScreenshotsEnabled);
    O option;
    switch (event) {
    case EVENT_ACHIEVEMENT: option = O::screenshotsAchievementEnabled; break;
    case EVENT_BESTIARY_ENTRY_COMPLETED: option = O::screenshotsBestiaryFullEnabled; break;
    case EVENT_BESTIARY_ENTRY_UNLOCKED: option = O::screenshotsBestiaryPartlyEnabled; break;
    case EVENT_BOSS_DEFEATED: option = O::screenshotsBossKillEnabled; break;
    case EVENT_DEATH_PVE: option = O::screenshotsDeathPvEEnabled; break;
    case EVENT_DEATH_PVP: option = O::screenshotsDeathPvPEnabled; break;
    case EVENT_LEVEL_UP: option = O::screenshotsLevelUpEnabled; break;
    case EVENT_PLAYER_KILL_ASSIST: option = O::screenshotsPlayerKillAssistEnabled; break;
    case EVENT_PLAYER_KILL: option = O::screenshotsPlayerKillFullEnabled; break;
    case EVENT_PLAYER_ATTACKING: option = O::screenshotsPvPAttackEnabled; break;
    case EVENT_TREASURE_FOUND: option = O::screenshotsRewardChestEnabled; break;
    case EVENT_SKILL_UP: option = O::screenshotsSkillUpEnabled; break;
    case EVENT_GIFT_OF_LIFE: option = O::screenshotsGiftOfLifeEnabled; break;
    case EVENT_HIGHEST_DAMAGE_DEALT: option = O::screenshotsNewMaxDamageEnabled; break;
    case EVENT_HIGHEST_HEALING_DONE: option = O::screenshotsNewMaxHealingEnabled; break;
    case EVENT_VALUABLE_LOOT: option = O::screenshotsTrackedLootEnabled; break;
    case EVENT_LOW_HEALTH: option = O::screenshotsLowHitPointsEnabled; break;
    default:
        return true;
    }
    if (!automatic)
        return false;
    return optionEnabled(option);
}

void TScreenshotManager::requestScreenshot(const int event)
{
    if ((event != EVENT_BACKLOG || m_pendingEvent == EVENT_NONE) && isScreenshotEnabled(event))
        m_pendingEvent = event;
}

bool TScreenshotManager::handleGameAction(const input::IGameAction& action)
{
    if (const auto* automatic = input::gameActionAs<input::TGameActionTakeAutomaticScreenshot>(&action)) {
        requestScreenshot(automatic->screenshotType());
        return true;
    }
    if (action.gameActionType() == input::EGameAction::TAKE_MANUAL_SCREENSHOT) {
        if (isScreenshotEnabled(EVENT_HOTKEY))
            m_pendingEvent = EVENT_HOTKEY;
        return true;
    }
    return false;
}

void TScreenshotManager::onBacklockUpdateTriggered()
{
    if (m_pendingEvent == EVENT_NONE && isScreenshotEnabled(EVENT_BACKLOG))
        m_pendingEvent = EVENT_BACKLOG;
}

void TScreenshotManager::clearBacklog()
{
    m_backlogTimer.stop();
    m_backlog.clear();
    if (m_pendingEvent == EVENT_BACKLOG)
        m_pendingEvent = EVENT_NONE;
}

void TScreenshotManager::startBacklogTimerIfNeeded()
{
    if (!m_backlogTimer.isActive() && m_gameSessionActive && optionEnabled(O::screenshotsUseBacklog))
        m_backlogTimer.start();
}

void TScreenshotManager::onScreenshotMangerOptionsChanged()
{
    if (!optionEnabled(O::screenshotsUseBacklog)) {
        clearBacklog();
        return;
    }
    if (!m_backlogTimer.isActive() && m_pendingEvent == EVENT_NONE && isScreenshotEnabled(EVENT_BACKLOG))
        m_pendingEvent = EVENT_BACKLOG;
    startBacklogTimerIfNeeded();
}

void TScreenshotManager::setGameSessionActive(const bool active)
{
    clearBacklog();
    m_gameSessionActive = active;
    startBacklogTimerIfNeeded();
}

bool TScreenshotManager::isDeathScreenshotDeferred() const
{
    // CIP asks the session's local player whether the death is still being
    // shown; qtclient's session exposes no such state, as with no session.
    return false;
}

bool TScreenshotManager::takeScreenshotIfEventIsWainting()
{
    bool taken = false;
    bool deferred = false;
    if ((m_pendingEvent == EVENT_DEATH_PVE || m_pendingEvent == EVENT_DEATH_PVP) && m_gameSessionActive)
        deferred = isDeathScreenshotDeferred();
    if (m_heldEvent != EVENT_NONE) {
        grabScreenshot(m_heldEvent);
        m_heldEvent = EVENT_NONE;
        taken = true;
    }
    // Record events are held for one frame so the new value is on screen.
    if (m_pendingEvent == EVENT_HIGHEST_DAMAGE_DEALT || m_pendingEvent == EVENT_HIGHEST_HEALING_DONE) {
        m_heldEvent = m_pendingEvent;
        m_pendingEvent = EVENT_NONE;
        return taken;
    }
    if (!deferred && m_pendingEvent != EVENT_NONE) {
        grabScreenshot(m_pendingEvent);
        m_pendingEvent = EVENT_NONE;
        taken = true;
    }
    return taken;
}

void TScreenshotManager::grabScreenshot(const int event)
{
    QQuickItem* item = optionEnabled(O::screnshotsOnlyGameWindow) ? m_mapWindow.data() : m_rootItem.data();
    m_grabResult = item ? item->grabToImage() : QSharedPointer<QQuickItemGrabResult>();
    if (!m_grabResult)
        return;
    m_outstandingScreenshots.push_front(std::make_unique<TOutstandingScreenshot>(m_grabResult, event));
    TOutstandingScreenshot* outstanding = m_outstandingScreenshots.front().get();
    connect(outstanding, &TOutstandingScreenshot::ready, this, &TScreenshotManager::processReadyScreenshot);
    processReadyScreenshot(outstanding->id());
}

void TScreenshotManager::processReadyScreenshot(const uint screenshotId)
{
    TOutstandingScreenshot* outstanding = nullptr;
    for (const auto& candidate : m_outstandingScreenshots) {
        if (candidate->id() == screenshotId) {
            outstanding = candidate.get();
            break;
        }
    }
    if (!outstanding)
        return;
    QImage image = outstanding->grabResult()->image();
    if (image.isNull())
        return;
    disconnect(outstanding, &TOutstandingScreenshot::ready, this, &TScreenshotManager::processReadyScreenshot);
    image.setText(QStringLiteral("Software"), QStringLiteral("Tibia Client"));
    image.setText(QStringLiteral("Creation Time"),
                  QDateTime::currentDateTime().toString(QStringLiteral("yyyy:MM:dd hh:mm:ss")));
    if (outstanding->eventType() == EVENT_BACKLOG) {
        if (optionEnabled(O::screenshotsUseBacklog)) {
            m_backlog.push_back(image);
            if (m_backlog.size() > BacklogCapacity)
                m_backlog.pop_front();
        }
    } else {
        std::vector<QImage> images(m_backlog.begin(), m_backlog.end());
        images.push_back(image);
        const QString name = eventName(outstanding->eventType());
        QThreadPool::globalInstance()->start([this, images = std::move(images), name]() { saveScreenshots(images, name); });
    }
    m_outstandingScreenshots.remove_if([screenshotId](const auto& candidate) { return candidate->id() == screenshotId; });
}

void TScreenshotManager::saveScreenshots(const std::vector<QImage>& images, const QString& eventName)
{
    const QString part = QStringLiteral("_%1");
    QString fileName = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hhmmsszzz%1%2%3.png"));
    QString characterPart;
    if (m_gameSessionActive) {
        const std::string characterName = game::localPlayerName();
        if (!characterName.empty())
            characterPart = part.arg(QString::fromStdString(characterName));
    }
    fileName = fileName.arg(characterPart).arg(part.arg(eventName));
    if (images.size() == 1) {
        m_fileSystemHelper->writeScreenshot(fileName.arg(QString()), images.front());
    } else {
        fileName = fileName.arg(part);
        for (size_t i = 0; i < images.size(); ++i)
            m_fileSystemHelper->writeScreenshot(fileName.arg(i + 1), images[i]);
    }

    const QString text = qtTrId("screenshot_saved")
                             .arg(eventName)
                             .arg(QDir::toNativeSeparators(m_fileSystemHelper->getScreenshotsDirectory().path()));
    auto message = std::make_shared<input::TGameActionAddTextMessage>(input::EGameAction::ADD_SYSTEM_CHAT_MESSAGE, text, chat::consoleColor(ScreenshotSavedMessageMode));
    auto sound = std::make_shared<input::gameactions::TGameActionSoundTrigger>(input::gameactions::TGameActionSoundTrigger::TSoundEffect{ ScreenshotSavedSoundEffect });
    // Saving runs on the thread pool; actions are published on the manager's thread.
    QMetaObject::invokeMethod(
        this,
        [this, message, sound]() {
            Q_EMIT publishGameAction(message);
            Q_EMIT publishGameAction(sound);
        },
        Qt::QueuedConnection);
}

} // namespace tibia::screenshot
