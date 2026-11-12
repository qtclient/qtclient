#include "screenshot/screenshotciptypes.h"

#include "app/clientoptions.h"

#include <client/const.h>

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

namespace tibia::screenshot {

namespace {
} // namespace

TScreenshotManager* TScreenshotManager::s_instance = nullptr;

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

void TOutstandingScreenshot::markReady(const QImage& image)
{
    m_image = image;
    Q_EMIT ready(m_id);
}

TScreenshotManager::TScreenshotManager(QObject* parent)
    : cip::tibia::screenshot::TScreenshotManager(parent)
{
    s_instance = this;
    m_backlogTimer.setSingleShot(true);
    m_backlogTimer.setInterval(0);
    connect(&m_backlogTimer, &QTimer::timeout, this, &TScreenshotManager::onBacklockUpdateTriggered);
    onScreenshotMangerOptionsChanged();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            [this](const QString& name) {
                if (name == QLatin1String("screenshotsUseBacklog") || name.startsWith(QLatin1String("screenshots")))
                    onScreenshotMangerOptionsChanged();
            });
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

void TScreenshotManager::configure(QQuickWindow* window, QQuickItem* mapWindowPane)
{
    m_window = window;
    m_mapWindowPane = mapWindowPane;
}

void TScreenshotManager::onScreenshotMangerOptionsChanged()
{
    m_useBacklog = app::ClientOptions::load().options().value(QLatin1String("screenshotsUseBacklog")).toBool(false);
}

bool TScreenshotManager::automaticAllowed(const QJsonObject& opts, const int eventType) const
{
    if (!opts.value(QLatin1String("screenshotsAutoScreenshotsEnabled")).toBool(true))
        return false;
    const char* key = nullptr;
    switch (eventType) {
    case tibia::client::CLIENT_EVENT_BOSSDEFEATED: key = "screenshotsBossKillEnabled"; break;
    case tibia::client::CLIENT_EVENT_DEATHPVE: key = "screenshotsDeathPvEEnabled"; break;
    case tibia::client::CLIENT_EVENT_DEATHPVP: key = "screenshotsDeathPvPEnabled"; break;
    case tibia::client::CLIENT_EVENT_PLAYERKILLASSIST: key = "screenshotsPlayerKillAssistEnabled"; break;
    case tibia::client::CLIENT_EVENT_PLAYERKILL: key = "screenshotsPlayerKillFullEnabled"; break;
    case tibia::client::CLIENT_EVENT_PLAYERATTACKING: key = "screenshotsPvPAttackEnabled"; break;
    case tibia::client::CLIENT_EVENT_TREASUREFOUND: key = "screenshotsRewardChestEnabled"; break;
    case tibia::client::CLIENT_EVENT_GIFTOFLIFE: key = "screenshotsGiftOfLifeEnabled"; break;
    case 52: key = "screenshotsTrackedLootEnabled"; break;
    case 200 + tibia::client::CLIENT_EVENT_TYPE_ACHIEVEMENT: key = "screenshotsAchievementEnabled"; break;
    case 200 + tibia::client::CLIENT_EVENT_TYPE_LEVEL: key = "screenshotsLevelUpEnabled"; break;
    case 200 + tibia::client::CLIENT_EVENT_TYPE_SKILL: key = "screenshotsSkillUpEnabled"; break;
    case 200 + tibia::client::CLIENT_EVENT_TYPE_BESTIARY: key = "screenshotsBestiaryFullEnabled"; break;
    case 200 + tibia::client::CLIENT_EVENT_TYPE_BOSSTIARY: key = "screenshotsBossKillEnabled"; break;
    case 101: key = "screenshotsBestiaryPartlyEnabled"; break;
    case 102: key = "screenshotsLowHitPointsEnabled"; break;
    case 103: key = "screenshotsNewMaxDamageEnabled"; break;
    case 104: key = "screenshotsNewMaxHealingEnabled"; break;
    default:
        return true;
    }
    return opts.value(QLatin1String(key)).toBool(true);
}

QString TScreenshotManager::nextPath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/screenshots");
    QDir().mkpath(directory);
    return directory + QStringLiteral("/screenshot-%1.png")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss-zzz")));
}

void TScreenshotManager::captureNow(const Pending& pending, const QJsonObject& opts)
{
    if (!m_window)
        return;
    QImage image = m_window->grabWindow();
    if (pending.automatic && opts.value(QLatin1String("screnshotsOnlyGameWindow")).toBool(false) && m_mapWindowPane) {
        const QRect crop = m_mapWindowPane->mapRectToScene(m_mapWindowPane->boundingRect()).toAlignedRect();
        if (crop.isValid() && image.rect().intersects(crop))
            image = image.copy(crop.intersected(image.rect()));
    }
    if (pending.id != 0) {
        if (TOutstandingScreenshot* outstanding = m_outstanding.value(pending.id)) {
            outstanding->markReady(image);
            processReadyScreenshot(pending.id);
            return;
        }
    }
    image.save(nextPath());
}

void TScreenshotManager::enqueue(const Pending& pending)
{
    m_backlog.append(pending);
    if (!m_backlogTimer.isActive())
        m_backlogTimer.start();
}

void TScreenshotManager::drainBacklog()
{
    if (m_backlog.isEmpty() || !m_window)
        return;
    const QJsonObject opts = app::ClientOptions::load().options();
    const QVector<Pending> batch = m_backlog;
    m_backlog.clear();
    for (const Pending& pending : batch)
        captureNow(pending, opts);
}

void TScreenshotManager::onBacklockUpdateTriggered()
{
    drainBacklog();
}

void TScreenshotManager::processReadyScreenshot(const uint screenshotId)
{
    TOutstandingScreenshot* outstanding = m_outstanding.take(screenshotId);
    if (!outstanding)
        return;
    const QImage image = outstanding->image();
    outstanding->deleteLater();
    if (!image.isNull())
        image.save(nextPath());
}

bool TScreenshotManager::takeScreenshotIfEventIsWainting()
{
    if (m_backlog.isEmpty())
        return false;
    drainBacklog();
    return true;
}

bool TScreenshotManager::handleScreenshotAction(const input::TGameAction& action)
{
    const bool automatic = action.type == input::EGameAction::TAKE_AUTOMATIC_SCREENSHOT;
    const QJsonObject opts = app::ClientOptions::load().options();
    if (automatic && !automaticAllowed(opts, action.parameter))
        return true;

    Pending pending;
    pending.automatic = automatic;
    pending.eventType = action.parameter;
    pending.id = m_nextId++;

    auto* outstanding = new TOutstandingScreenshot(this);
    outstanding->setId(pending.id);
    m_outstanding.insert(pending.id, outstanding);

    if (m_useBacklog)
        enqueue(pending);
    else
        captureNow(pending, opts);
    return true;
}

} // namespace tibia::screenshot
