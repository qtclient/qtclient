#include "gamewindowcontroller.h"

#include "app/clientoptions.h"
#include "app/hints.h"
#include "app/launcherenv.h"
#include "game/boostedcreaturerequestuploader.h"
#include "game/liveinformationrequestuploader.h"
#include "game/showoffrequestuploader.h"
#include "gamewindow/calendardialogcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "gamewindow/newsdialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include "news/eventqmlentry.h"
#include "news/eventsuploader.h"
#include "news/newsrequestuploader.h"
#include "news/newsstorage.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <atomic>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>

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

std::atomic_bool g_processExitAllowed{ false };

QString joinEventNames(const QJsonArray& events, const qint64 now, const bool current)
{
    QStringList names;
    for (const auto& value : events) {
        const QJsonObject e = value.toObject();
        const qint64 start = e.value(QStringLiteral("startdate")).toInteger();
        const qint64 end = e.value(QStringLiteral("enddate")).toInteger();
        const bool isCurrent = start <= now && now <= end;
        if (isCurrent != current)
            continue;
        const QString name = e.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            names.append(name);
    }
    return names.join(QStringLiteral(", "));
}

QColor parseEventColor(const QJsonObject& e, const char* key, const QColor& fallback)
{
    const QString raw = e.value(QLatin1String(key)).toString();
    if (raw.isEmpty())
        return fallback;
    const QColor color(raw.startsWith(QLatin1Char('#')) ? raw : QLatin1Char('#') + raw);
    return color.isValid() ? color : fallback;
}

bool eventIsSeasonal(const QJsonObject& e)
{
    const QJsonValue v = e.value(QStringLiteral("isseasonal"));
    if (v.isBool())
        return v.toBool();
    return v.toInt() != 0;
}

}

TGameWindowController::TGameWindowController(QQuickItem* container, QObject* parent)
    : cip::tibia::gamewindow::TGameWindowController(parent)
    , m_container(container)
    , m_newsStorage(std::make_unique<news::TNewsStorage>())
    , m_boostedUploader(std::make_unique<game::TBoostedCreatureRequestUploader>())
    , m_liveInfoUploader(std::make_unique<game::TLiveInformationRequestUploader>())
    , m_showOffUploader(std::make_unique<game::TShowOffRequestUploader>())
    , m_eventsUploader(std::make_unique<news::TEventsUploader>())
    , m_newsUploader(std::make_unique<news::TNewsRequestUploader>(m_newsStorage.get()))
    , m_newsDialog(std::make_unique<TNewsDialogController>(m_newsStorage.get()))
    , m_newsButton(std::make_unique<TNewsButtonController>())
    , m_calendarDialog(std::make_unique<TCalendarDialogController>())
{
    m_newsDialog->setNewsUploader(m_newsUploader.get());
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
        if (name == QLatin1String("sessionRemainLoggedIn")) {
            // Misc "Stay logged in by default" mirrors onto the login checkbox.
            setRememberEmail(app::ClientOptions::load().options().value(name).toBool(true));
            return;
        }
        if (name == QLatin1String("soundAnthemEnabled") || name == QLatin1String("soundMusicVolume")
            || name == QLatin1String("soundMasterVolume"))
            syncAnthemButton();
    });

    connect(m_boostedUploader.get(), &game::TBoostedCreatureRequestUploader::boostedCreatureReceived, this,
            &TGameWindowController::onBoostedCreatureReceived);
    connect(m_liveInfoUploader.get(), &game::TLiveInformationRequestUploader::newLiveInformationReceived, this,
            &TGameWindowController::onLiveInformationReceived);
    connect(m_showOffUploader.get(), &game::TShowOffRequestUploader::showOffReceived, this,
            &TGameWindowController::onShowOffReceived);
    connect(m_eventsUploader.get(), &news::TEventsUploader::scheduleUpdated, this, &TGameWindowController::onEventScheduleUpdated);
    connect(m_newsUploader.get(), &news::TNewsRequestUploader::newsQueryFailed, m_newsDialog.get(),
            &TNewsDialogController::onNewsQueryFailed);

    using ENewsTab = cip::tibia::gamewindow::TNewsDialogController::EDialogTab;
    input::setActionHandler(A::OPEN_NEWS_DIALOG, [this](const input::TGameAction&) {
        if (m_newsDialog)
            m_newsDialog->showDialog();
        return true;
    });
    input::setActionHandler(A::OPEN_NEWS_CATEGORY, [this](const input::TGameAction& action) {
        if (!m_newsDialog)
            return true;
        switch (action.parameter) {
        case int(ENewsTab::ClientFeatures):
        case int(ENewsTab::UsefulInfo):
        case int(ENewsTab::PlayerGuide):
        case int(ENewsTab::MajorUpdates):
        case int(ENewsTab::Support):
            m_newsDialog->showDialog(static_cast<ENewsTab>(action.parameter));
            break;
        default:
            m_newsDialog->showDialog();
            break;
        }
        return true;
    });
    input::setActionHandler(A::REFRESH_COMPENDIUM_NEWS, [this](const input::TGameAction&) {
        if (m_newsUploader)
            m_newsUploader->requestUpdate();
        return true;
    });
}

TGameWindowController::~TGameWindowController()
{
    input::setActionHandler(A::OPEN_NEWS_DIALOG, nullptr);
    input::setActionHandler(A::OPEN_NEWS_CATEGORY, nullptr);
    input::setActionHandler(A::REFRESH_COMPENDIUM_NEWS, nullptr);
    clearEventModels();
}

void TGameWindowController::setStatusBarController(QObject* controller)
{
    m_statusBarController = controller;
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
    m_eventsUploader->setServiceUrl(service);
    m_eventsUploader->setClientRevision(clientRevision);
    m_newsUploader->setServiceUrl(service);
    m_newsUploader->setClientRevision(clientRevision);
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
    m_eventsUploader->requestUpdate();
    m_newsUploader->requestUpdate();
    m_boostedUploader->requestUpdate();
    m_showOffUploader->requestUpdate();
#endif
}

void TGameWindowController::requestDeferredStartScreenServices()
{
    m_liveInfoUploader->requestUpdate();
    m_eventsUploader->requestUpdate();
    m_newsUploader->requestUpdate();
    m_boostedUploader->requestUpdate();
    m_showOffUploader->requestUpdate();
}

void TGameWindowController::loadHints()
{
    const app::LauncherEnv env = app::LauncherEnv::load(QDir::currentPath());
    auto* net = new QNetworkAccessManager(this);
    QNetworkReply* reply = net->get(QNetworkRequest(QUrl(env.assetBase.toString() + QStringLiteral("/hints.json"))));
    const QString cdn = env.assetBase.toString();
    connect(reply, &QNetworkReply::finished, this, [this, reply, cdn] {
        const app::Hint hint = app::randomHint(app::parseHints(reply->readAll()));
        reply->deleteLater();
        if (hint.richText.isEmpty())
            return;
        m_hintAreaTitle = hint.title.isEmpty() ? QStringLiteral("Hint") : hint.title;
        m_hintAreaRichText = app::hintHtml(hint, cdn);
        Q_EMIT hintAreaTitleChanged();
        Q_EMIT hintAreaRichTextChanged();
    });
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
    qDeleteAll(m_eventsScheduleCurrent);
    m_eventsScheduleCurrent.clear();
    qDeleteAll(m_eventsScheduleUpcoming);
    m_eventsScheduleUpcoming.clear();
}

void TGameWindowController::onEventScheduleUpdated()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QJsonArray events = m_eventsUploader->eventList();
    const QString currentTooltip = joinEventNames(events, now, true);
    const QString upcomingTooltip = joinEventNames(events, now, false);
    if (currentTooltip == m_eventsScheduleCurrentTooltip && upcomingTooltip == m_eventsScheduleUpcomingTooltip)
        return;
    clearEventModels();

    QStringList seasonalCurrent;
    QStringList seasonalUpcoming;
    for (const auto& value : events) {
        const QJsonObject e = value.toObject();
        const QString name = e.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        const qint64 start = e.value(QStringLiteral("startdate")).toInteger();
        const qint64 end = e.value(QStringLiteral("enddate")).toInteger();
        const bool isCurrent = start <= now && now <= end;
        const bool startsOrEnds = start <= now && now <= end
            ? (QDateTime::fromSecsSinceEpoch(start).date() == QDateTime::fromSecsSinceEpoch(now).date()
               || QDateTime::fromSecsSinceEpoch(end).date() == QDateTime::fromSecsSinceEpoch(now).date())
            : (QDateTime::fromSecsSinceEpoch(start).date() == QDateTime::fromSecsSinceEpoch(now).date());
        const QColor primary = parseEventColor(e, "colorlight", QColor(QStringLiteral("#c9a227")));
        const QColor secondary = parseEventColor(e, "colordark", QColor(QStringLiteral("#6b5a1e")));
        auto* entry = new news::TEventQMLEntry(name, primary, secondary, startsOrEnds, this);
        if (isCurrent)
            m_eventsScheduleCurrent.append(entry);
        else if (start > now)
            m_eventsScheduleUpcoming.append(entry);
        else
            delete entry;
        if (eventIsSeasonal(e)) {
            if (isCurrent)
                seasonalCurrent.append(name);
            else if (start > now)
                seasonalUpcoming.append(name);
        }
    }

    m_eventsScheduleCurrentTooltip = currentTooltip;
    m_eventsScheduleUpcomingTooltip = upcomingTooltip;
    m_eventsScheduleCurrentSeasonalTooltip = seasonalCurrent.join(QStringLiteral(", "));
    m_eventsScheduleUpcomingSeasonalTooltip = seasonalUpcoming.join(QStringLiteral(", "));
    m_newEventToday = !m_eventsScheduleCurrent.isEmpty();
    if (m_calendarDialog)
        m_calendarDialog->setEvents(events);
    qCInfo(lcGameWindow) << "events current" << m_eventsScheduleCurrent.size() << m_eventsScheduleCurrentTooltip
                         << "upcoming" << m_eventsScheduleUpcoming.size() << m_eventsScheduleUpcomingTooltip;
    Q_EMIT eventScheduleChanged();
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
}

void TGameWindowController::showStartScreen()
{
    setScreen(QString(), true, QColor(QStringLiteral("#041020")));
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
    if (!m_container)
        return;
    if (QQuickWindow* window = m_container->window())
        window->setProperty("leftAndRightStatusActionBarWidth", newWidth);
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
    const QString email = EMailAddress.trimmed();
    if (email.isEmpty() || Password.isEmpty())
        return;
    rememberLoginEmailIfEnabled(email);
    Q_EMIT accountLoginRequested(email, Password);
}

void TGameWindowController::forgotEMailAddressOrPasswordPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_LOST_ACCOUNT));
}

void TGameWindowController::handleCreateNewAccount()
{
    setShowCreateAccountOption(false);
    Q_EMIT createAccountDialogRequested();
}

void TGameWindowController::createNewAccountPressed()
{
    handleCreateNewAccount();
}

void TGameWindowController::goToLogin()
{
    setShowCreateAccountOption(false);
}

void TGameWindowController::goToCreateNewAccount()
{
    handleCreateNewAccount();
}

void TGameWindowController::tibiaLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_TIBIA_PAGE));
}

void TGameWindowController::cipSoftLogoPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_CIPSOFT));
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
    // Checked (muted look) when anthem is off or master volume is 0 — playFile is silent then.
    const bool anthemMuted = !options.value(QLatin1String("soundAnthemEnabled")).toBool(true)
        || options.value(QLatin1String("soundMasterVolume")).toInt(100) <= 0;
    const bool musicMuted = options.value(QLatin1String("soundMusicVolume")).toInt(50) <= 0;
    if (anthemMuted == m_anthemMuted && musicMuted == m_musicMuted)
        return;
    m_anthemMuted = anthemMuted;
    m_musicMuted = musicMuted;
    Q_EMIT anthemEnabledChanged();
}

void TGameWindowController::muteAnthemPressed()
{
    const QJsonObject options = app::ClientOptions::load().options();
    if (options.value(QLatin1String("soundMusicVolume")).toInt(50) <= 0)
        return;
    const bool anthemOn = options.value(QLatin1String("soundAnthemEnabled")).toBool(true);
    const int master = options.value(QLatin1String("soundMasterVolume")).toInt(100);
    // Unmute: turn the anthem back on and restore master if a global mute left it at 0
    // (playFile refuses masterGain <= 0, so toggling anthem alone was silent).
    if (!anthemOn || master <= 0) {
        QJsonObject opts;
        opts.insert(QStringLiteral("soundAnthemEnabled"), true);
        if (master <= 0) {
            const int restored = options.value(QLatin1String("soundMasterVolumeOld")).toInt(100);
            opts.insert(QStringLiteral("soundMasterVolume"), restored > 0 ? restored : 100);
        }
        QJsonObject changes;
        changes.insert(QStringLiteral("options"), opts);
        app::ClientOptions::storeAll(changes);
        return;
    }
    app::ClientOptions::store(QStringLiteral("soundAnthemEnabled"), false);
}

void TGameWindowController::manageAccountPressed()
{
    input::publish(input::makeAction(input::EGameAction::OPEN_URL_ACCESS_ACCOUNT));
}

void TGameWindowController::manageClientsPressed()
{
    app::LauncherEnv::showLauncher();
}

void TGameWindowController::quitPressed()
{
    // CIP shows Exit / Logout / Cancel only while a character is in the world.
    // The login mask and the character list close with the window.
    if (!g_game.isOnline()) {
        Q_EMIT quitGameExitRequested();
        return;
    }
    showQuitGameDialog();
}

void TGameWindowController::showQuitGameDialog()
{
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
    Q_EMIT optionsMenuRequested(0);
}

}
