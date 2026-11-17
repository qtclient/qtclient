#include "gamewindowcontroller.h"

#include <QQmlComponent>
#include <QQmlEngine>

#include "app/clientoptions.h"
#include "randomhints.h"
#include "app/launcherenv.h"
#include "game/boostedcreaturerequestuploader.h"
#include "game/liveinformationrequestuploader.h"
#include "game/showoffrequestuploader.h"
#include "cyclopedia/bosstiarystorage.h"
#include "gamewindow/calendardialogcontroller.h"
#include "gamewindow/mousecursorcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/newsdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include "news/eventqmlentry.h"
#include "news/eventsuploader.h"
#include "news/newsdiskio.h"
#include "news/newsrequestuploader.h"
#include "news/newsstorage.h"
#include "clockservice.h"
#include "jsondocumenthttppostsender.h"
#include "shared/tibiafilesystemhelper.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>
#include <atomic>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTime>
#include <QTimeZone>
#include <tibia/game/gamesession.h>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

Q_LOGGING_CATEGORY(lcGameWindow, "tibia.gamewindow")

namespace tibia::gamewindow {

namespace {

using EResizingMode = cip::tibia::qmlcomponents::enums::EGameWindowResizingMode;
using A = input::EGameAction;

constexpr auto UpperPaneHeight = "gameWindowUpperPaneHeight";
constexpr auto LowerPaneHeight = "gameWindowLowerPaneHeight";
constexpr auto ResizingMode = "gameWindowResizingMode";
constexpr auto LoginEmail = "loginEmailAddress";
constexpr auto RememberEmail = "rememberEmail";
/// CIP option showEMailAsPlainText (0x80).
constexpr auto ShowEmailAsPlainText = "showEMailAsPlainText";

/// The OS Caps Lock state with a key event (CIP asks the platform).
bool capsLockActive(const QKeyEvent* event)
{
#if defined(Q_OS_MACOS)
    // NSEventModifierFlagCapsLock.
    return (event->nativeModifiers() & (1u << 16)) != 0;
#elif defined(Q_OS_WIN)
    Q_UNUSED(event);
    return (GetKeyState(VK_CAPITAL) & 1) != 0;
#else
    Q_UNUSED(event);
    return false;
#endif
}

std::atomic_bool g_processExitAllowed{ false };

/// CIP: the server save; events start and end with it.
const QTime ServerSaveTime(10, 0);

}

bool TGameWindowController::eventFilter(QObject* watched, QEvent* event)
{
    // CIP: every key release on the start screen refreshes the Caps Lock hint.
    if (event->type() == QEvent::KeyRelease) {
        const bool active = capsLockActive(static_cast<QKeyEvent*>(event));
        if (m_isCapsLockActive != active) {
            m_isCapsLockActive = active;
            Q_EMIT isCapsLockActiveChanged();
        }
    }
    return cip::tibia::gamewindow::TGameWindowController::eventFilter(watched, event);
}

QString TGameWindowController::boostedCreatureName() const
{
    const QString name = cyclopedia::bossName(m_boostedCreatureRace);
    return name.isEmpty() ? m_boostedCreatureName : name;
}

QString TGameWindowController::boostedBossName() const
{
    const QString name = cyclopedia::bossName(m_boostedBossRace);
    return name.isEmpty() ? m_boostedBossName : name;
}

void TGameWindowController::linkClicked(const QString& Url)
{
    input::publish(std::make_shared<input::TGameActionOpenUrl>(Url));
}

void TGameWindowController::linkHoverChanged(const bool IsOverLink)
{
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setPointingHand(IsOverLink);
}

bool TGameWindowController::showEmailAsPlainText() const
{
    return app::ClientOptions::load().options().value(QLatin1String(ShowEmailAsPlainText)).toBool(false);
}

void TGameWindowController::setShowEmailAsPlainText(const bool v)
{
    if (showEmailAsPlainText() == v)
        return;
    app::ClientOptions::store(QLatin1String(ShowEmailAsPlainText), v);
    Q_EMIT showEmailChanged();
}

TGameWindowController::TGameWindowController(QQuickItem* container, QObject* parent)
    : cip::tibia::gamewindow::TGameWindowController(parent)
    , m_container(container)
    , m_fileSystemHelper(std::make_shared<shared::TTibiaFileSystemHelper>())
    , m_clientServicesSender(std::make_shared<shared::TJsonDocumentHttpPostSender>())
    , m_newsStorage(std::make_shared<news::TNewsStorage>(shared::TClockService::process()))
    , m_newsDiskIO(std::make_shared<news::TNewsDiskIOAsync>(m_fileSystemHelper))
    , m_eventsStorage(std::make_unique<news::TEventsStorage>())
    , m_boostedUploader(std::make_unique<game::TBoostedCreatureRequestUploader>())
    , m_liveInfoUploader(std::make_unique<game::TLiveInformationRequestUploader>())
    , m_showOffUploader(std::make_unique<game::TShowOffRequestUploader>())
    , m_newsDialog(std::make_unique<TNewsDialogController>(m_newsStorage.get()))
    , m_newsButton(std::make_unique<TNewsButtonController>())
    , m_calendarDialog(std::make_unique<TCalendarDialogController>())
{
    // CIP (TGameClient): the storage saves through the disk cache and starts
    // from what it holds.
    connect(m_newsStorage.get(), &news::TNewsStorage::startSavingNewsDataToDisk, m_newsDiskIO.get(),
            &news::TNewsDiskIO::saveNewsDataToDisk);
    connect(m_newsDiskIO.get(), &news::TNewsDiskIO::loadingNewsDataSucceeded, m_newsStorage.get(),
            &news::TNewsStorage::loadFromDisk);
    m_newsDiskIO->loadNewsDataFromDiskIfPossible();
    // CIP launcher authenticates before the game window; local client-start
    // shows the login mask until setAuthenticated(true).
    m_isAuthenticated = false;
    // CIP: /images/skin/classic/client-dragon-logo-regular.png — product logo in
    // the same dragonLogo AnimatedImage (visible with the login mask).
    m_dragonLogoSource = QStringLiteral("qrc:/images/skin/classic/client-dragon-logo-regular.png");
    m_createAccountEnabled = true;
    const QJsonObject options = app::ClientOptions::load().options();
    m_upperPaneHeight = static_cast<uint>(options.value(QLatin1String(UpperPaneHeight)).toInt());
    m_lowerPaneHeight = static_cast<uint>(options.value(QLatin1String(LowerPaneHeight)).toInt());
    m_resizingMode = static_cast<EResizingMode>(options.value(QLatin1String(ResizingMode)).toInt());
    m_statusBarVisible = options.value(QLatin1String("statusBarEnabled")).toBool(true);
    // CIP Misc stayLoggedInByDefault (sessionRemainLoggedIn) seeds the login
    // "remember email" checkbox when rememberEmail has never been stored.
    if (options.contains(QLatin1String(RememberEmail)))
        m_rememberEmail = options.value(QLatin1String(RememberEmail)).toBool(true);
    else
        m_rememberEmail = options.value(QLatin1String("sessionRemainLoggedIn")).toBool(true);
    m_initialLoginEmail = options.value(QLatin1String(LoginEmail)).toString();
    m_showCreateAccountOption = false;
    syncAnthemButton();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("statusBarEnabled")) {
            m_statusBarVisible = app::ClientOptions::load().options().value(name).toBool(true);
            Q_EMIT statusBarVisibilityChanged();
            return;
        }
        if (name == QLatin1String("soundAnthemEnabled") || name == QLatin1String("soundMusicVolume")
            || name == QLatin1String("soundMasterVolume"))
            syncAnthemButton();
    });

    connect(this, &TGameWindowController::newLeftAndRightStatusActionBarWidth, this, [this](const int width) {
        if (m_container) {
            if (QQuickWindow* window = m_container->window())
                window->setProperty("leftAndRightStatusActionBarWidth", width);
        }
    });

    connect(m_boostedUploader.get(), &game::TBoostedCreatureRequestUploader::boostedCreatureReceived, this,
            &TGameWindowController::onBoostedCreatureReceived);
    connect(m_liveInfoUploader.get(), &game::TLiveInformationRequestUploader::newLiveInformationReceived, this,
            &TGameWindowController::onLiveInformationReceived);
    connect(m_showOffUploader.get(), &game::TShowOffRequestUploader::showOffReceived, this,
            &TGameWindowController::onShowOffReceived);
    // qtclient: CIP's interval is not recoverable; once a minute catches the server save.
    m_eventsSchedulePreviewTimer.setInterval(60 * 1000);
    connect(&m_eventsSchedulePreviewTimer, &QTimer::timeout, this,
            &TGameWindowController::onEventsSchedulePreviewUpdateTimerTimeout);
    m_eventsSchedulePreviewTimer.start();

    for (const A type : { A::OPEN_NEWS_DIALOG, A::OPEN_NEWS_CATEGORY, A::REFRESH_COMPENDIUM_NEWS }) {
        input::setActionHandler(type, [this](const input::TGameAction& action) {
            if (m_newsDialog)
                m_newsDialog->handleGameAction(action);
            return true;
        });
    }
}

TGameWindowController::~TGameWindowController()
{
    input::setActionHandler(A::OPEN_NEWS_DIALOG, nullptr);
    input::setActionHandler(A::OPEN_NEWS_CATEGORY, nullptr);
    input::setActionHandler(A::REFRESH_COMPENDIUM_NEWS, nullptr);
    // CIP also releases its native window helper and the keep-renderer prompt here; qtclient's
    // prompt lives in app/sessionbootstrap and has no native helper.
    clearEventModels();
}

QQuickItem* TGameWindowController::gameWindowChild(const QString& name) const
{
    return m_gameWindowItem ? m_gameWindowItem->findChild<QQuickItem*>(name) : nullptr;
}

std::unique_ptr<QObject> TGameWindowController::createGameWindow(QQmlEngine* engine)
{
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/gamewindow.qml")));
    std::unique_ptr<QObject> object(component.create());
    auto* item = qobject_cast<QQuickItem*>(object.get());
    if (!item) {
        qCritical().noquote() << component.errorString();
        return nullptr;
    }
    item->setParentItem(m_container);
    item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    if (m_container)
        m_container->setProperty("backgroundImageVisible", true);
    connect(this, &TGameWindowController::bindFocusToChatInput, item, [item] { QMetaObject::invokeMethod(item, "bindFocusToChatInput"); });
    connect(this, &TGameWindowController::releaseFocusFromChatInput, item,
            [item] { QMetaObject::invokeMethod(item, "releaseFocusFromChatInput"); });
    setGameWindowItem(item);
    return object;
}

void TGameWindowController::setStatusBarController(QObject* controller)
{
    m_statusBarController = controller;
}

void TGameWindowController::setGameWindowItem(QQuickItem* gameWindowItem)
{
    m_gameWindowItem = gameWindowItem;
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this,
            &TGameWindowController::onClientOptionsChangedAfterShowGameWindows, Qt::UniqueConnection);
    onClientOptionsChangedAfterShowGameWindows();
}

void TGameWindowController::onClientOptionsChangedAfterShowGameWindows()
{
    const QJsonObject options = app::ClientOptions::load().options();
    if (m_container)
        m_container->setProperty("backgroundImageSmooth", options.value(QLatin1String("antialiasingMode")).toInt() == 1);
    if (m_gameWindowItem) {
        if (auto* cooldownBar = gameWindowChild(QStringLiteral("cooldownBar")))
            cooldownBar->setVisible(options.value(QLatin1String("cooldownBarEnabled")).toBool(true));
    }
    // The pane layout getters read their options.
    m_upperPaneHeight = static_cast<uint>(options.value(QLatin1String(UpperPaneHeight)).toInt());
    m_lowerPaneHeight = static_cast<uint>(options.value(QLatin1String(LowerPaneHeight)).toInt());
    m_resizingMode = static_cast<EResizingMode>(options.value(QLatin1String(ResizingMode)).toInt());
    Q_EMIT layoutChanged();
}

void TGameWindowController::configureClientServices(const QUrl& url, const QString& clientRevision)
{
    const QUrl service = url.isEmpty() ? QUrl() : url;
    m_boostedUploader->setServiceUrl(service);
    m_boostedUploader->setClientRevision(clientRevision);
    m_liveInfoUploader->setServiceUrl(service);
    m_liveInfoUploader->setClientRevision(clientRevision);
    m_showOffUploader->setServiceUrl(service);
    m_showOffUploader->setClientRevision(clientRevision);
    // CIP builds the client services uploaders with their URL.
    m_eventsUploader = std::make_unique<news::TEventsUploader>(service, m_clientServicesSender, m_fileSystemHelper,
                                                               shared::TClockService::process());
    connect(m_eventsUploader.get(), &news::TEventsUploader::updated, m_eventsStorage.get(),
            &news::TEventsStorage::onEventsUpdated);
    connect(m_eventsUploader.get(), &news::TEventsUploader::updated, this, &TGameWindowController::onEventScheduleUpdated);
    m_newsUploader = std::make_unique<news::TNewsRequestUploader>(service, m_newsStorage, m_clientServicesSender,
                                                                  m_fileSystemHelper, shared::TClockService::process());
    connect(m_newsUploader.get(), &news::TNewsRequestUploader::newsQueryFailed, m_newsDialog.get(),
            &TNewsDialogController::onNewsQueryFailed);
    m_newsViewsUploader = std::make_unique<news::TNewsViewsUploader>(service, m_clientServicesSender);
    m_newsDialog->setNewsUploaders(m_newsUploader.get(), m_newsViewsUploader.get());
    requestStartScreenServices();
}

void TGameWindowController::requestStartScreenServices()
{
    // CipSoft GameWindowController starts these on the start screen (boosted /
    // calendar / live info / showoff); news is the same multiplex (`type:"news"`).
    loadHints();
#if defined(Q_OS_WASM) || defined(__EMSCRIPTEN__)
    // iPhone Safari jetsams when login chrome fetches boosted/showoff/events on
    // a fresh Module. Defer multiplex until account login (setAuthenticated).
#else
    m_liveInfoUploader->requestUpdate();
    if (m_eventsUploader)
        m_eventsUploader->requestUpdate();
    if (m_newsUploader)
        m_newsUploader->requestUpdate();
    m_boostedUploader->requestUpdate();
    m_showOffUploader->requestUpdate();
#endif
}

void TGameWindowController::requestDeferredStartScreenServices()
{
    m_liveInfoUploader->requestUpdate();
    if (m_eventsUploader)
        m_eventsUploader->requestUpdate();
    if (m_newsUploader)
        m_newsUploader->requestUpdate();
    m_boostedUploader->requestUpdate();
    m_showOffUploader->requestUpdate();
}

void TGameWindowController::loadHints()
{
    const app::LauncherEnv env = app::LauncherEnv::load(QDir::currentPath());
    m_cdnBase = env.assetBase.toString();
    m_hintsUrl = QUrl(m_cdnBase + QStringLiteral("/hints.json"));
    m_hintsDownloadedAt = 0;
    updateHint();
}

void TGameWindowController::updateHint()
{
    const int now = static_cast<int>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000);
    if (static_cast<uint>(now - m_hintsDownloadedAt) < 3600) {
        selectRandomHintFromCache();
        return;
    }
    // CIP queues the download in its TDownloadPool.
    auto* net = new QNetworkAccessManager(this);
    QNetworkReply* reply = net->get(QNetworkRequest(m_hintsUrl));
    const QUrl url = m_hintsUrl;
    connect(reply, &QNetworkReply::finished, this, [this, reply, net, url] {
        if (reply->error() == QNetworkReply::NoError)
            onHintDownloadSuccess(url, reply->readAll());
        else
            onHintDownloadFailed(url);
        reply->deleteLater();
        net->deleteLater();
    });
}

void TGameWindowController::onHintDownloadSuccess(const QUrl& Url, const QByteArray& Data)
{
    if (Url != m_hintsUrl)
        return;
    m_hints = tibia::shared::parseHints(Data);
    selectRandomHintFromCache();
    m_hintsDownloadedAt = static_cast<int>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000);
}

void TGameWindowController::onHintDownloadFailed(const QUrl& url)
{
    if (url != m_hintsUrl)
        return;
    setHintAreaRichText(QString());
    setHintAreaTitle(QString());
}

void TGameWindowController::selectRandomHintFromCache()
{
    const std::optional<tibia::shared::Hint> hint = tibia::shared::selectHint(m_hints, QDateTime::currentDateTimeUtc());
    if (!hint)
        return;
    setHintAreaTitle(hint->title);
    // qtclient: hint images are served from our CDN.
    setHintAreaRichText(tibia::shared::hintHtml(*hint, m_cdnBase));
}

void TGameWindowController::setHintAreaTitle(const QString& title)
{
    const QString value = title.isEmpty() ? qtTrId("hint_area_title") : title;
    if (value == m_hintAreaTitle)
        return;
    m_hintAreaTitle = value;
    Q_EMIT hintAreaTitleChanged();
}

void TGameWindowController::setHintAreaRichText(const QString& richText)
{
    const QString value = richText.isEmpty() ? qtTrId("default_hint") : richText;
    if (value == m_hintAreaRichText)
        return;
    m_hintAreaRichText = value;
    Q_EMIT hintAreaRichTextChanged();
}

void TGameWindowController::onBoostedCreatureReceived()
{
    const QJsonObject reply = m_boostedUploader->reply();
    const int race = reply.value(QStringLiteral("creatureraceid")).toInt(reply.value(QStringLiteral("raceid")).toInt());
    const QString name = reply.value(QStringLiteral("boostname")).toString();
    const int bossRace = reply.value(QStringLiteral("bossraceid")).toInt();
    const QString bossName = reply.value(QStringLiteral("bossname")).toString();
    if (race == m_boostedCreatureRace && name == m_boostedCreatureName && bossRace == m_boostedBossRace && bossName == m_boostedBossName)
        return;
    m_boostedCreatureRace = race;
    m_boostedCreatureName = name;
    m_boostedBossRace = bossRace;
    m_boostedBossName = bossName;
    qCInfo(lcGameWindow) << "boosted creature" << m_boostedCreatureRace << m_boostedCreatureName
                         << "boss" << m_boostedBossRace << m_boostedBossName;
    Q_EMIT boostedCreatureRaceChanged();
}

void TGameWindowController::onLiveInformationReceived()
{
    const QJsonObject reply = m_liveInfoUploader->reply();
    const int players = reply.value(QStringLiteral("playersonline")).toInt();
    const int twitchStreams = reply.value(QStringLiteral("twitchstreams")).toInt();
    const int twitchViewers = reply.value(QStringLiteral("twitchviewer")).toInt();
    const int youTubeStreams = reply.value(QStringLiteral("gamingyoutubestreams")).toInt();
    const int youTubeViewers = reply.value(QStringLiteral("gamingyoutubeviewer")).toInt();
    if (players == m_onlinePlayerCount && twitchStreams == m_twitchStreamCount && twitchViewers == m_twitchViewerCount
        && youTubeStreams == m_youTubeStreamCount && youTubeViewers == m_youTubeViewerCount)
        return;
    m_onlinePlayerCount = players;
    m_twitchStreamCount = twitchStreams;
    m_twitchViewerCount = twitchViewers;
    m_youTubeStreamCount = youTubeStreams;
    m_youTubeViewerCount = youTubeViewers;
    qCInfo(lcGameWindow) << "live info players" << m_onlinePlayerCount;
    Q_EMIT liveInformationChanged();
}

void TGameWindowController::onShowOffReceived()
{
    qCInfo(lcGameWindow) << "showoff" << m_showOffUploader->title()
                         << m_showOffUploader->imageUrl();
}

void TGameWindowController::clearEventModels()
{
    const QList<QObject*> current = m_eventsScheduleCurrent;
    const QList<QObject*> upcoming = m_eventsScheduleUpcoming;
    m_eventsScheduleCurrent.clear();
    m_eventsScheduleUpcoming.clear();
    m_eventsScheduleCurrentSeasonalTooltip.clear();
    m_eventsScheduleUpcomingSeasonalTooltip.clear();
    Q_EMIT eventScheduleChanged();
    qDeleteAll(current);
    qDeleteAll(upcoming);
}

void TGameWindowController::onEventScheduleUpdated(const std::vector<news::TEventInformation>& events)
{
    updateEventsSchedulePreview(events);
    if (m_calendarDialog)
        m_calendarDialog->setEvents(m_eventsStorage->events());
}

void TGameWindowController::updateEventsSchedulePreview(const std::vector<news::TEventInformation>& events)
{
    clearEventModels();
    if (&m_events != &events)
        m_events = events;
    m_newEventToday = false;
    const QDateTime now = QDateTime::currentDateTimeUtc().toTimeZone(news::serverTimeZone());
    const QDate today = now.date();
    const bool afterServerSave = ServerSaveTime <= now.time();
    const QString format = QStringLiteral("<u>%1:</u><br>\n%2");
    QString currentSeasonal;
    QString upcomingSeasonal;
    QString current;
    QString upcoming;
    const auto append = [&format](QString& tooltip, const news::TEventInformation& event) {
        if (!tooltip.isEmpty())
            tooltip.append(QStringLiteral("<br><br>\n"));
        tooltip.append(format.arg(event.name).arg(event.description));
    };

    QDate nextStart;
    for (const news::TEventInformation& event : m_events) {
        if (afterServerSave && event.startDate == today)
            m_newEventToday = true;
        // An event starting today runs from the server save on.
        const bool started = event.startDate != today || afterServerSave;
        if ((today < event.startDate || !started) && (!nextStart.isValid() || nextStart == event.startDate)) {
            nextStart = event.startDate;
            if (!event.isSeasonal) {
                append(upcoming, event);
                m_eventsScheduleUpcoming.append(new news::TEventQMLEntry(event, false));
            } else {
                append(upcomingSeasonal, event);
            }
        }
        const bool ended = event.endDate == today && afterServerSave;
        if (!started || ended)
            continue;
        for (QDate day = event.startDate; day <= event.endDate; day = day.addDays(1)) {
            if (day != today)
                continue;
            if (!event.isSeasonal) {
                append(current, event);
                m_eventsScheduleCurrent.append(new news::TEventQMLEntry(event, false));
            } else {
                append(currentSeasonal, event);
            }
        }
    }

    const auto byPriority = [](const QObject* left, const QObject* right) {
        return news::TEventQMLEntry::lessThan(static_cast<const news::TEventQMLEntry*>(left),
                                              static_cast<const news::TEventQMLEntry*>(right));
    };
    std::sort(m_eventsScheduleUpcoming.begin(), m_eventsScheduleUpcoming.end(), byPriority);
    std::sort(m_eventsScheduleCurrent.begin(), m_eventsScheduleCurrent.end(), byPriority);
    m_eventsScheduleCurrentSeasonalTooltip = currentSeasonal;
    m_eventsScheduleUpcomingSeasonalTooltip = upcomingSeasonal;
    m_eventsScheduleCurrentTooltip = current;
    m_eventsScheduleUpcomingTooltip = upcoming;
    Q_EMIT eventScheduleChanged();
}

void TGameWindowController::onEventsSchedulePreviewUpdateTimerTimeout()
{
    const QTime time = QDateTime::currentDateTimeUtc().toTimeZone(news::serverTimeZone()).time();
    if (ServerSaveTime <= time && m_beforeServerSave)
        updateEventsSchedulePreview(m_events);
    m_beforeServerSave = time < ServerSaveTime;
}

void TGameWindowController::requestOpenCalendar()
{
    input::publish(input::makeAction(A::OPEN_CALENDAR_DIALOG));
}

void TGameWindowController::setAuthenticated(const bool authenticated)
{
    if (m_isAuthenticated == authenticated)
        return;
    m_isAuthenticated = authenticated;
    Q_EMIT authenticatedStateChanged();
#if defined(Q_OS_WASM) || defined(__EMSCRIPTEN__)
    if (authenticated)
        requestDeferredStartScreenServices();
#endif
}

void TGameWindowController::showGameScreen()
{
    setAuthenticated(true);
    setScreen(QStringLiteral("INGAME"), false, Qt::black);
    // CIP: the start screen's key filter goes; the game screen is announced.
    QCoreApplication::instance()->removeEventFilter(this);
    Q_EMIT gameScreenNowDisplayed();
}

void TGameWindowController::showStartScreen()
{
    setScreen(QString(), true, QColor(QStringLiteral("#041020")));
    QCoreApplication::instance()->installEventFilter(this);
    requestStartScreenServices();
    Q_EMIT startScreenNowDisplayed();
}

void TGameWindowController::setScreen(const QString& state, const bool backgroundImageVisible, const QColor& windowColor)
{
    if (m_container) {
        m_container->setProperty("backgroundImageVisible", backgroundImageVisible);
        if (QQuickWindow* window = m_container->window())
            window->setColor(windowColor);
    }
    if (m_gameWindowState == state)
        return;
    m_gameWindowState = state;
    Q_EMIT gameWindowStateChanged();
}

void TGameWindowController::leftAndRightStatusActionBarWidthChanged(const int newWidth)
{
    Q_EMIT newLeftAndRightStatusActionBarWidth(newWidth);
}

void TGameWindowController::setUpperPaneHeight(const uint height)
{
    if (m_upperPaneHeight == height)
        return;
    m_upperPaneHeight = height;
    app::ClientOptions::store(QLatin1String(UpperPaneHeight), static_cast<int>(height));
    Q_EMIT layoutChanged();
}

void TGameWindowController::setLowerPaneHeight(const uint height)
{
    if (m_lowerPaneHeight == height)
        return;
    m_lowerPaneHeight = height;
    app::ClientOptions::store(QLatin1String(LowerPaneHeight), static_cast<int>(height));
    Q_EMIT layoutChanged();
}

void TGameWindowController::setResizingMode(const EResizingMode mode)
{
    if (m_resizingMode == mode)
        return;
    m_resizingMode = mode;
    app::ClientOptions::store(QLatin1String(ResizingMode), static_cast<int>(mode));
    Q_EMIT layoutChanged();
}

void TGameWindowController::setRememberEmail(const bool v)
{
    if (m_rememberEmail == v)
        return;
    m_rememberEmail = v;
    app::ClientOptions::store(QLatin1String(RememberEmail), v);
    if (!v) {
        m_initialLoginEmail.clear();
        app::ClientOptions::store(QLatin1String(LoginEmail), QString());
        Q_EMIT initialLoginEmailChanged();
    }
    Q_EMIT rememberEmailChanged();
}

void TGameWindowController::rememberLoginEmailIfEnabled(const QString& email)
{
    const QString trimmed = email.trimmed();
    if (!m_rememberEmail || trimmed.isEmpty())
        return;
    if (m_initialLoginEmail == trimmed)
        return;
    m_initialLoginEmail = trimmed;
    app::ClientOptions::store(QLatin1String(LoginEmail), trimmed);
    Q_EMIT initialLoginEmailChanged();
}

void TGameWindowController::loginPressed(const QString& EMailAddress, const QString& Password)
{
    Q_EMIT loginRequested(EMailAddress, Password);
}

void TGameWindowController::forgotEMailAddressOrPasswordPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_LOST_ACCOUNT));
}

void TGameWindowController::handleCreateNewAccount()
{
    Q_EMIT createAccountRequested();
}

void TGameWindowController::createNewAccountPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_CREATE_ACCOUNT));
}

void TGameWindowController::goToLogin()
{
    setShowCreateAccountOption(false);
}

void TGameWindowController::goToCreateNewAccount()
{
    // CIP: the create-account option goes without its change signal.
    m_showCreateAccountOption = false;
    Q_EMIT createAccountRequested();
}

void TGameWindowController::tibiaLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_TIBIA_PAGE));
}

void TGameWindowController::cipSoftLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_CIPSOFT));
}

void TGameWindowController::twitchLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_TWITCH_TIBIA));
}

void TGameWindowController::youTubeLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_YOUTUBE_TIBIA));
}

void TGameWindowController::closeOpenMultiActionButtons()
{
    input::publish(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
}

void TGameWindowController::setShowCreateAccountOption(const bool show)
{
    if (m_showCreateAccountOption == show)
        return;
    m_showCreateAccountOption = show;
    Q_EMIT showCreateAccountOptionChanged();
}

void TGameWindowController::allowProcessExit()
{
    g_processExitAllowed.store(true, std::memory_order_relaxed);
}

bool TGameWindowController::processExitAllowed()
{
    return g_processExitAllowed.load(std::memory_order_relaxed);
}

void TGameWindowController::syncAnthemButton()
{
    const QJsonObject options = app::ClientOptions::load().options();
    // CIP: anthemMuted is !soundAnthemEnabled; musicMuted is a master or music volume of 0.
    const bool anthemMuted = !options.value(QLatin1String("soundAnthemEnabled")).toBool(true);
    const bool musicMuted = options.value(QLatin1String("soundMasterVolume")).toInt(100) == 0
                         || options.value(QLatin1String("soundMusicVolume")).toInt(50) == 0;
    if (anthemMuted == m_anthemMuted && musicMuted == m_musicMuted)
        return;
    m_anthemMuted = anthemMuted;
    m_musicMuted = musicMuted;
    Q_EMIT anthemEnabledChanged();
}

void TGameWindowController::muteAnthemPressed()
{
    // CIP: the login screen's anthem button toggles soundAnthemEnabled (musicMuted disables it).
    const bool enabled = app::ClientOptions::load().options().value(QLatin1String("soundAnthemEnabled")).toBool(true);
    app::ClientOptions::store(QStringLiteral("soundAnthemEnabled"), !enabled);
}

void TGameWindowController::manageAccountPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_ACCESS_ACCOUNT));
}

void TGameWindowController::manageClientsPressed()
{
    Q_EMIT openLauncherRequested();
}

void TGameWindowController::quitPressed()
{
    Q_EMIT gameExitRequested();
}

void TGameWindowController::handleGameExitRequest()
{
    // Exit / Logout / Cancel only while a character is in the world.
    // The login mask and the character list close with the window.
    if (!tibia::game::isOnline()) {
        Q_EMIT quitGameExitRequested();
        return;
    }
    using Button = TMessageDialogController::Button;
    if (!m_quitDialog) {
        m_quitDialog = new TMessageDialogController(this);
        m_quitDialog->setCloseOnButtonClicked(true);
        m_quitDialog->setButtons({
            new TMessageDialogButton(qtTrId("quit_game_exit_button"), Button::OK, QKeySequence(), this),
            new TMessageDialogButton(qtTrId("quit_game_logout_button"), Button::USER_DEFINED, QKeySequence(), this),
            new TMessageDialogButton(qtTrId("cancel"), Button::CANCEL, QKeySequence(QStringLiteral("Esc")), this),
        });
        connect(m_quitDialog, &TMessageDialogController::acceptClicked, this, [this](bool) {
            m_allowWindowClose = true;
            Q_EMIT quitGameExitRequested();
        });
        connect(m_quitDialog, &TMessageDialogController::buttonClicked, this,
                [this](Button id, bool) {
                    if (id == Button::USER_DEFINED)
                        Q_EMIT quitGameLogoutRequested();
                });
    }
    m_quitDialog->showMessage(qtTrId("warning"), qtTrId("quit_game_logout_warning"));
}

void TGameWindowController::optionsPressed()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_OPTIONS_MENU_DIALOG));
    action->parameter = 0;
    input::publish(action);
}

}
