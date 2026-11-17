#include "app/bootstrap.h"
#include "mods/modloader.h"
#include "mods/registry.h"

#include "app/clientoptions.h"
#include "app/crashlog.h"
#include "app/launcherenv.h"
#include "app/portalurls.h"
#if defined(__EMSCRIPTEN__)
#include "browser/qtclient_opfs_backend.h"
#endif
#ifdef TIBIA_WITH_GAMECORE
#include "app/qtplatformwindow.h"
#include "client/clientciptypes.h"
#include "client/gameclient.h"
#include "config/hotkeyoptions.h"
#endif
#include "controllers/characterselectioncontroller.h"
#include "gamewindow/gamewindowcontroller.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QSaveFile>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickGraphicsConfiguration>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QUrlQuery>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include <tibia/game/gamesession.h>

#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/val.h>
#endif

#if defined(TIBIA_DESKTOP_WEBENGINE)
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
Q_IMPORT_QML_PLUGIN(QtWebEngineQuickPlugin)
#endif

Q_IMPORT_QML_PLUGIN(QtQuick_LegacyControlsPlugin)
#if defined(TIBIA_WEBENGINE_STUBS)
Q_IMPORT_QML_PLUGIN(QtWebEnginePlugin)
Q_IMPORT_QML_PLUGIN(QtWebChannelPlugin)
#endif
#ifdef TIBIA_QT_STATIC
Q_IMPORT_QML_PLUGIN(QtQuickControls2UniversalStylePlugin)
#endif

using namespace tibia;

namespace {

/// --renderer must be known before QGuiApplication exists (the graphics API is
/// process-wide), so it is read straight from argv.
int rendererFromArgs(const int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strncmp(arg, "--renderer=", 11) == 0)
            return app::parseRendererIndex(QString::fromLocal8Bit(arg + 11), -1);
        if (std::strcmp(arg, "--renderer") == 0 && i + 1 < argc)
            return app::parseRendererIndex(QString::fromLocal8Bit(argv[i + 1]), -1);
    }
    return app::parseRendererIndex(qEnvironmentVariable("QTCLIENT_RENDERER"), -1);
}

QStringList commandLine()
{
    QStringList args = QCoreApplication::arguments();
#ifdef Q_OS_WASM
    // The page URL carries the options: tibia.html?preview&renderer=software
    // Portal-only keys (launcher → game handoff) are not client flags.
    const auto search = QString::fromStdString(emscripten::val::global("location")["search"].as<std::string>());
    const QUrlQuery query(search.mid(1));
    for (const auto& [key, value] : query.queryItems(QUrl::FullyDecoded)) {
        if (key == QLatin1String("game"))
            continue;
        args << (value.isEmpty() ? QStringLiteral("--") + key : QStringLiteral("--%1=%2").arg(key, value));
    }
#endif
    return args;
}

constexpr auto ClientWindowSection = "clientWindowOptions";
constexpr auto ClientWindowBorderlessKey = "clientWindowIsBorderless";

void applyClientWindowBorderless(QQuickWindow* window, const bool borderless)
{
    if (!window)
        return;
    // CipSoft's borderless window is the full-screen window state
    // (clientWindowIsBorderless; TOGGLE_FULLSCREEN flips it).
    const bool isFullScreen = window->visibility() == QWindow::FullScreen;
    if (borderless == isFullScreen)
        return;
    if (borderless)
        window->showFullScreen();
    else
        window->showNormal();
}

void applyClientWindowBorderlessFromOptions(QQuickWindow* window)
{
    if (!window)
        return;
    const QJsonObject opts = app::ClientOptions::load().root().value(QLatin1String(ClientWindowSection)).toObject();
    applyClientWindowBorderless(window, opts.value(QLatin1String(ClientWindowBorderlessKey)).toBool(false));
}

void restoreClientWindowGeometry(QQuickWindow* window)
{
    if (!window)
        return;
    const QJsonObject opts = app::ClientOptions::load().root().value(QLatin1String(ClientWindowSection)).toObject();
    const int width = opts.value(QLatin1String("clientWindowWidth")).toInt(0);
    const int height = opts.value(QLatin1String("clientWindowHeight")).toInt(0);
    const int x = opts.value(QLatin1String("clientWindowPositionX")).toInt(std::numeric_limits<int>::min());
    const int y = opts.value(QLatin1String("clientWindowPositionY")).toInt(std::numeric_limits<int>::min());
    const bool maximized = opts.value(QLatin1String("clientWindowIsMaximized")).toBool(false);
    applyClientWindowBorderless(window, opts.value(QLatin1String(ClientWindowBorderlessKey)).toBool(false));
    if (width > 0 && height > 0)
        window->resize(width, height);
    if (x != std::numeric_limits<int>::min() && y != std::numeric_limits<int>::min()) {
        if (QScreen* screen = window->screen()) {
            const QRect available = screen->availableGeometry();
            const int clampedX = std::clamp(x, available.left(), available.right() - 100);
            const int clampedY = std::clamp(y, available.top(), available.bottom() - 100);
            window->setPosition(clampedX, clampedY);
        } else {
            window->setPosition(x, y);
        }
    }
    if (maximized)
        window->showMaximized();
}

void saveClientWindowGeometry(QQuickWindow* window)
{
    if (!window)
        return;
    const QRect geo = window->geometry();
    QJsonObject section = app::ClientOptions::load().root().value(QLatin1String(ClientWindowSection)).toObject();
    section.insert(QStringLiteral("clientWindowWidth"), geo.width());
    section.insert(QStringLiteral("clientWindowHeight"), geo.height());
    section.insert(QStringLiteral("clientWindowPositionX"), geo.x());
    section.insert(QStringLiteral("clientWindowPositionY"), geo.y());
    section.insert(QStringLiteral("clientWindowIsMaximized"),
                   static_cast<bool>(window->windowStates() & Qt::WindowMaximized));
    // Preserve clientWindowIsBorderless — geometry save must not wipe the option.
    if (QScreen* screen = window->screen())
        section.insert(QStringLiteral("clientWindowScreenName"), screen->name());
    QJsonObject changes;
    changes.insert(QString::fromLatin1(ClientWindowSection), section);
    app::ClientOptions::storeAll(changes);
}

QStringList dataDirs(const QString& explicitDir)
{
#ifndef TIBIA_CLIENT_PACKAGE_DIR
#define TIBIA_CLIENT_PACKAGE_DIR "Tibia.app"
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList dirs{
        explicitDir,
        qEnvironmentVariable("QTCLIENT_DATA_DIR"),
        QDir::current().filePath(QStringLiteral("data")),
        appDir + QStringLiteral("/../Resources/data"),
        appDir + QStringLiteral("/data"),
        QStringLiteral("/data"),
#ifdef TIBIA_SOURCE_DATA_DIR
        QStringLiteral(TIBIA_SOURCE_DATA_DIR),
#endif
    };
    // Tip package (portal OPFS / desktop AppData): RCC + fonts live under
    // Contents/Resources/assets, not ./data. Stock host looks for Tibia.app;
    // IdleOT qtclient directoryname is IdleOT.app — probe both (same as gamecore).
    auto appendPackageDirs = [&dirs](const QString& appData) {
        if (appData.isEmpty())
            return;
        const char* packageNames[] = {
            TIBIA_CLIENT_PACKAGE_DIR,
            "IdleOT.app",
        };
        for (const char* packageName : packageNames) {
            const QString pkg = QDir(appData).filePath(
                QLatin1String("packages/") + QLatin1String(packageName));
            dirs << pkg;
            dirs << QDir(pkg).filePath(QStringLiteral("Contents/Resources"));
            dirs << QDir(pkg).filePath(QStringLiteral("Contents/Resources/assets"));
        }
    };
    appendPackageDirs(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return dirs;
}

/// Compiles every CIP QML file (and override) the app ships; the count of
/// files that fail is the exit code.
int checkQml(QQmlEngine& engine)
{
    int failed = 0;
    int total = 0;
    QDirIterator it(QStringLiteral(":/qt/qml"), { QStringLiteral("*.qml") }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (path.endsWith(QLatin1String(".override.qml")))
            continue;
        ++total;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc") + path), QQmlComponent::PreferSynchronous);
        if (component.isError()) {
            ++failed;
            qWarning().noquote() << component.errorString().trimmed();
        }
    }
    qInfo("qml check: %d of %d files compile", total - failed, total);
    return failed;
}

authentication::TCharacterList previewCharacters()
{
    const auto entry = [](const char* name, int level, const char* vocation, uint16_t outfit, bool main = false) {
        authentication::TCharacter c;
        c.name = QString::fromLatin1(name);
        c.worldId = 1;
        c.level = level;
        c.vocation = QString::fromLatin1(vocation);
        c.outfitId = outfit;
        c.headColor = 78;
        c.torsoColor = 69;
        c.legsColor = 58;
        c.detailColor = 76;
        c.isMainCharacter = main;
        return c;
    };
    return {
        entry("Aria Stormblade", 312, "Elite Knight", 131, true),
        entry("Brom Ironhide", 87, "Royal Paladin", 129),
        entry("Cyra Nightshade", 154, "Master Sorcerer", 130),
        entry("Dorn Mossbeard", 42, "Elder Druid", 144),
        entry("Eli Ashwalker", 8, "None", 128),
    };
}

authentication::TWorldList previewWorlds()
{
    authentication::TWorld world;
    world.id = 1;
    world.name = QCoreApplication::applicationName();
    world.pvpType = 1;
    return { { world.id, world } };
}

}

namespace {

/// A pack identity with a displayName replaces CipSoft's AppData identity.
void applyPackIdentity(const tibia::mods::ModIdentity& id)
{
    if (id.displayName.empty())
        return;
    QCoreApplication::setApplicationName(QString::fromStdString(id.displayName));
    QCoreApplication::setOrganizationName(
        QString::fromStdString(id.organization.empty() ? id.displayName : id.organization));
    QCoreApplication::setOrganizationDomain(QString::fromStdString(
        id.organizationDomain.empty() ? std::string("tibia.local") : id.organizationDomain));
}

}

int main(int argc, char** argv)
{
#if defined(__EMSCRIPTEN__)
    // /home/web_user (AppData, tip packages) and /mods are the origin's OPFS,
    // served by the portal's qtclient-fs Worker. Mount before anything opens a file.
    if (!qtclientMountOpfs())
        qFatal("tibia.wasm: could not mount OPFS at /home/web_user (see the console)");
#endif
    // QTCLIENT_TEST_PATHS=1 (render check, smoke runs): settings and data go to
    // Qt's test locations (~/.qttest), never the user's Application Support or
    // Preferences.
    if (qEnvironmentVariableIntValue("QTCLIENT_TEST_PATHS") == 1) {
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    }
    tibia::app::installCrashLog();

    // --no-mods → CipSoft; else all qtclient mods/<id>.
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--no-mods") == 0)
            tibia::mods::ModLoader::setSkipLoad(true);
    }

    // Identity before anything reads AppData: the pack's identity from its
    // mod.json (as loadAll() will register it), else CipSoft's. Options and
    // QSettings read before QGuiApplication must use the same folders the app
    // saves to once packs load.
    QCoreApplication::setApplicationName(QStringLiteral("Tibia"));
    QCoreApplication::setOrganizationName(QStringLiteral("Tibia"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("tibia.local"));
    applyPackIdentity(tibia::mods::ModLoader::identityBeforeApplication(
        argc > 0 ? QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath() : QString()));

    app::configureProcess(app::ClientOptions::readStoredFile(), rendererFromArgs(argc, argv));

#if defined(TIBIA_DESKTOP_WEBENGINE)
    QtWebEngineQuick::initialize();
#endif

    QGuiApplication application(argc, argv);
    app::applyBundledMoltenVkIcd();
    // Runtime packs after QApp.
    if (tibia::mods::ModLoader::skipLoad())
        qInfo("--no-mods: stock CipSoft client (no pack)");
    else
        tibia::mods::ModLoader::loadAll();
    tibia::mods::ensureModsRegistered();
    if (const auto* id = tibia::mods::primaryIdentity())
        applyPackIdentity(*id);
    application.setApplicationDisplayName(QCoreApplication::applicationName());

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ QStringLiteral("renderer"), QStringLiteral("auto, software, d3d11, vulkan, opengl, metal, d3d12 or a CIP renderer index."), QStringLiteral("renderer") });
    parser.addOption({ QStringLiteral("data"), QStringLiteral("Directory with graphics_resources.rcc."), QStringLiteral("dir") });
    parser.addOption({ QStringLiteral("preview"), QStringLiteral("Show character selection with sample characters.") });
    parser.addOption({ QStringLiteral("screenshot"), QStringLiteral("Save the window to file once it has rendered, then quit."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("check-qml"), QStringLiteral("Compile every bundled QML file and exit (non-zero on errors).") });
    parser.addOption({ QStringLiteral("check-hotkeys"),
                       QStringLiteral("Round-trip THotkeyOptions load/switchSet/setMapping/save/reload and exit (non-zero on failure).") });
    parser.addOption({ QStringLiteral("account"), QStringLiteral("Account email for a login without the launcher (or QTCLIENT_ACCOUNT)."), QStringLiteral("email") });
    parser.addOption({ QStringLiteral("password"), QStringLiteral("Account password (or QTCLIENT_PASSWORD)."), QStringLiteral("password") });
    parser.addOption({ QStringLiteral("character"), QStringLiteral("Enter the game with this character without showing the list."), QStringLiteral("name") });
    parser.addOption({ QStringLiteral("play-for"), QStringLiteral("Log out and quit this many seconds after entering the world."), QStringLiteral("seconds") });
    parser.addOption({ QStringLiteral("walk"), QStringLiteral("Walk these directions (n,ne,e,se,s,sw,w,nw, comma-separated; dx:dy walks to a field relative to the character) two seconds after entering the world, each as soon as the previous walk ends (smoke tests)."), QStringLiteral("directions") });
    parser.addOption({ QStringLiteral("say"), QStringLiteral("Say this a second after entering the world (smoke tests; repeatable)."), QStringLiteral("text") });
    parser.addOption({ QStringLiteral("record"), QStringLiteral("Record the game session to this file."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("replay"), QStringLiteral("Play a recorded game session instead of logging in."), QStringLiteral("file") });
    parser.addOption({ QStringLiteral("screenshot-delay"), QStringLiteral("Milliseconds after entering the world before --screenshot (default 3000)."), QStringLiteral("ms") });
    parser.addOption({ QStringLiteral("no-mods"), QStringLiteral("Do not load mods/<id> packs (stock qtclient).") });
    // CIP client Play: launcher asserts the package/assets stamps already match.
    parser.addOption({ QStringLiteral("package-is-up-to-date"),
                       QStringLiteral("Tip Play: package and assets stamps already match (CIP).") });
    // Launcher settings may carry arguments of the Lua client; they are not fatal.
    if (!parser.parse(commandLine()))
        qWarning().noquote() << parser.errorText();
    if (parser.isSet(QStringLiteral("help")))
        parser.showHelp();

    const auto env = app::LauncherEnv::load(QDir::currentPath());
#ifdef TIBIA_WITH_GAMECORE
    // CIP TGameApplication: the crash handler is in place before the game starts.
    client::TGameApplication::setUpCrashHandler(env.loginUrl());
#endif
    // CIP links (dialogs, translations, Wheel planner) → this deployment's origins.
    app::PortalUrls::installCipUrlHandler(env);
    if (!parser.isSet(QStringLiteral("preview")) && !parser.isSet(QStringLiteral("replay"))
        && !parser.isSet(QStringLiteral("check-qml")) && !parser.isSet(QStringLiteral("check-hotkeys"))
        && !parser.isSet(QStringLiteral("account"))
        && app::LauncherEnv::startLauncherIfUpdateNeeded())
        return 0;

    const QString graphics = app::mountGraphicsResources(dataDirs(parser.value(QStringLiteral("data"))));
    if (graphics.isEmpty())
        qWarning("graphics_resources.rcc not found; run make extract or pass --data");
    app::installTranslations();
    app::installUiFonts(dataDirs(parser.value(QStringLiteral("data"))));

    auto engine = app::createEngine();
    // Neutral placeholders; packs replace via ModContext / suite wire.
    engine->rootContext()->setContextProperty(QStringLiteral("modOptions"), QVariant::fromValue<QObject*>(nullptr));
    engine->rootContext()->setContextProperty(QStringLiteral("modHudLayout"), QVariant::fromValue<QObject*>(nullptr));
    if (parser.isSet(QStringLiteral("check-qml")))
        return checkQml(*engine) == 0 ? 0 : 1;
#ifdef TIBIA_WITH_GAMECORE
    if (parser.isSet(QStringLiteral("check-hotkeys"))) {
        const QString error = tibia::config::THotkeyOptions::selfCheckRoundTrip();
        if (!error.isEmpty()) {
            qCritical().noquote() << "check-hotkeys:" << error;
            return 1;
        }
        qInfo("check-hotkeys: ok (clientoptions.json hotkeyOptions)");
        return 0;
    }
#endif

    QQmlComponent windowComponent(engine.get(), QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/clientwindow.qml")));
    std::unique_ptr<QObject> windowObject(windowComponent.create());
    auto* window = qobject_cast<QQuickWindow*>(windowObject.get());
    if (!window) {
        qCritical().noquote() << windowComponent.errorString();
        return 1;
    }
    // QRhi::Timestamps for lastCompletedGpuTime (raycast pass GPU). Must be set before the
    // scene graph creates the RHI. weather.perf also uses rhi->finish stalls when debug.
    if (!qEnvironmentVariableIsEmpty("QTCLIENT_PERF") || !qEnvironmentVariableIsEmpty("QTCLIENT_GPU_PERF")) {
        QQuickGraphicsConfiguration config = window->graphicsConfiguration();
        config.setTimestamps(true);
        window->setGraphicsConfiguration(config);
    }
#ifdef Q_OS_WASM
    // clientwindow.qml pins width/height to CIP minimum onCompleted. On WASM the
    // QScreen is the host element — keep the window filling it so the world is
    // not left-aligned with a black gutter.
    const auto fillHost = [window] {
        if (QScreen* screen = window->screen())
            window->resize(screen->size());
    };
    fillHost();
    if (QScreen* screen = window->screen())
        QObject::connect(screen, &QScreen::geometryChanged, window, fillHost);
#else
    restoreClientWindowGeometry(window);
    QObject::connect(&application, &QCoreApplication::aboutToQuit, window, [window] { saveClientWindowGeometry(window); });
    QObject::connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, window,
                     [window](const QString& name) {
                         if (name == QLatin1String(ClientWindowSection)
                             || name == QLatin1String(ClientWindowBorderlessKey))
                             applyClientWindowBorderlessFromOptions(window);
                     });
#endif
#ifdef TIBIA_WITH_GAMECORE
    app::qtWindow().attach(window);
#endif
    qInfo().noquote() << "renderer:" << window->rendererInterface()->graphicsApi() << "graphics:" << graphics;
    app::alignTextHinting(engine.get());

    QQuickItem* placeholder = window->findChild<QQuickItem*>(QStringLiteral("placeholder"));
    placeholder->setProperty("windowTitle", QCoreApplication::applicationName());

    if (env.isRelease() && !env.ssoHosted()) {
        qCritical().noquote() << QStringLiteral("Start %1 from the %1 launcher. It installs and updates the game.").arg(QCoreApplication::applicationName());
        return 1;
    }

    const bool preview = parser.isSet(QStringLiteral("preview"));
    gamewindow::TCharacterSelectionController previewController;
    gamewindow::TCharacterSelectionController* controller = &previewController;
    QObject::connect(&previewController, &gamewindow::TCharacterSelectionController::requestCharacterLogin,
                     [](const gamewindow::TCharacterLoginData& c) { qInfo().noquote() << "character selected:" << c.characterName << c.worldName; });
    QObject::connect(&previewController, &gamewindow::TCharacterSelectionController::abort, &application, &QCoreApplication::quit);
    if (preview) {
        previewController.setPremium(1, 365);
        previewController.setCharacters(previewCharacters(), previewWorlds());
        previewController.setLastSelectedCharacterName(QStringLiteral("Aria Stormblade"));
    }

#ifdef TIBIA_WITH_GAMECORE
    client::TGameClient gameClient;
    gameClient.setWindow(window);
    if (!preview)
        controller = gameClient.characterSelection();

    client::TGameClient::TGameWindowSetup setup;
    setup.engine = engine.get();
    setup.window = window;
    setup.placeholder = placeholder;
    setup.env = env;
    setup.preview = preview;
    setup.characterSelection = controller;
    setup.argv0 = QCoreApplication::applicationFilePath();
    setup.workDir = QDir::currentPath();
    if (!preview) {
        client::TGameClient::Options& clientOptions = setup.options;
        clientOptions.env = env;
        clientOptions.account = parser.value(QStringLiteral("account"));
        if (clientOptions.account.isEmpty())
            clientOptions.account = qEnvironmentVariable("QTCLIENT_ACCOUNT");
        clientOptions.password = parser.value(QStringLiteral("password"));
        if (clientOptions.password.isEmpty())
            clientOptions.password = qEnvironmentVariable("QTCLIENT_PASSWORD");
        clientOptions.autoCharacter = parser.value(QStringLiteral("character"));
        clientOptions.playForSeconds = parser.value(QStringLiteral("play-for")).toInt();
        clientOptions.say = parser.values(QStringLiteral("say"));
        clientOptions.walk = parser.value(QStringLiteral("walk")).split(QLatin1Char(','), Qt::SkipEmptyParts);
        clientOptions.recordTo = parser.value(QStringLiteral("record"));
        if (clientOptions.recordTo.isEmpty())
            clientOptions.recordTo = qEnvironmentVariable("QTCLIENT_RECORD");
        clientOptions.replay = parser.value(QStringLiteral("replay"));
    }
    if (!gameClient.createGameWindow(setup))
        return 1;
#endif

    if (const QString shot = parser.value(QStringLiteral("screenshot")); !shot.isEmpty()) {
        const auto takeShot = [window, shot] {
            const bool saved = window->grabWindow().save(shot);
            qInfo().noquote() << (saved ? "screenshot:" : "screenshot failed:") << shot;
            QCoreApplication::exit(saved ? 0 : 1);
        };
#ifdef TIBIA_WITH_GAMECORE
        // Logging in or replaying: shoot the world once it has had time to
        // arrive, or half a second after the last --say (numbers last a second).
        if (!preview && (parser.isSet(QStringLiteral("character")) || parser.isSet(QStringLiteral("replay")))) {
            const qsizetype says = parser.values(QStringLiteral("say")).size();
            int delay = says > 0 ? int(says) * 1000 + 500 : 3000;
            if (parser.isSet(QStringLiteral("screenshot-delay")))
                delay = parser.value(QStringLiteral("screenshot-delay")).toInt();
            QObject::connect(&gameClient, &client::TGameClient::mainWindowInitialized, window, [window, takeShot, delay] {
                QTimer::singleShot(delay, window, takeShot);
            }, Qt::SingleShotConnection);
        } else
#endif
            QTimer::singleShot(2000, window, takeShot);
    }

    qInfo("tibia.boot: entering application.exec");
#ifdef Q_OS_WASM
    // Portal /play overlay: qtLoad never resolves under JSPI (main stays in
    // exec). Hand the login-ready signal explicitly instead.
    EM_ASM({
        try {
            if (typeof window.__qtclientGameUiReady === 'function')
                window.__qtclientGameUiReady();
        } catch (e) {}
    });
#endif
    return application.exec();
}
