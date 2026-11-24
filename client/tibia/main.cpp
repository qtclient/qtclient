#include "app/bootstrap.h"
#include "mods/modloader.h"
#include "mods/registry.h"

#include "app/launcherenv.h"
#include "app/loginsplash.h"
#include "app/portalurls.h"
#include "platform/platform.h"
#ifdef TIBIA_WITH_GAMECORE
#include "app/qtplatformwindow.h"
#include "app/smokeactions.h"
#include "client/gameapplication.h"
#include "client/gameclient.h"
#include "config/clientoptions.h"
#include "config/hotkeyoptions.h"
#endif
#include "controllers/characterselectioncontroller.h"
#include "gamewindow/gamewindowcontroller.h"
#ifndef TIBIA_WITH_GAMECORE
#include "config/clientoptions.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/mousecursorcontroller.h"
#include "imageproviders/imagestripimageprovider.h"
#endif
#include "shared/tibiafilesystemhelper.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSettings>
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
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <client/const.h>
#include <client/declarations.h>
#include <client/outfit.h>
#include <client/position.h>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include "game/objectreference.h"


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

QStringList commandLine()
{
    return QCoreApplication::arguments() + tibia::platform::extraArguments();
}

QStringList dataDirs(const QString& explicitDir)
{
    // CipSoft ships graphics_resources.rcc next to the executable (bin/,
    // Contents/MacOS/) and the assets under Resources (applicationDirPath/..,
    // plus Resources when it exists): fonts in assets/fonts.
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir resources(appDir);
    resources.cdUp();
    resources.cd(QStringLiteral("./Resources"));
    return QStringList{
        explicitDir,
        qEnvironmentVariable("QTCLIENT_DATA_DIR"),
        appDir,
        resources.absolutePath(),
        // Host: monorepo builds run outside a package (`make extract` → data/).
        QDir::current().filePath(QStringLiteral("data")),
#ifdef TIBIA_SOURCE_DATA_DIR
        QStringLiteral(TIBIA_SOURCE_DATA_DIR),
#endif
    } + tibia::platform::extraDataRoots();
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
            qCritical() << "Component creation error for " << component.url().url() << "\n" << component.errors();
        }
    }
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
    tibia::platform::initializeProcess();
    // QTCLIENT_TEST_PATHS=1 (render check, smoke runs): settings and data go to
    // Qt's test locations (~/.qttest), never the user's Application Support or
    // Preferences.
    if (qEnvironmentVariableIntValue("QTCLIENT_TEST_PATHS") == 1) {
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    }

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
#ifdef TIBIA_WITH_GAMECORE
    // CIP TGameApplication: the process log starts once the identity names AppData.
    client::TGameApplication::setUpLogging();
#endif

#if defined(TIBIA_DESKTOP_WEBENGINE)
    // CIP: Chromium logs errors only; the web engine starts before the
    // application, and its context warnings stay out of the log.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--log-level=3");
    QtWebEngineQuick::initialize();
    QLoggingCategory::setFilterRules(QStringLiteral("js=false\nqt.webenginecontext.warning=false\n"));
#endif
    // CIP: the windows do not share one OpenGL context.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#ifndef TIBIA_WITH_GAMECORE
    // Standalone QML shell: CipSoft's controls style (the game application
    // runs the rest of the process setup).
    qputenv("QT_QUICK_CONTROLS_STYLE", "Universal");
#endif

#ifdef TIBIA_WITH_GAMECORE
    client::TGameApplicationBase::prepareProcess();
    // CIP: the application shows the client window and owns the game client.
    client::TGameApplication application(argc, argv);
#else
    QGuiApplication application(argc, argv);
#endif
    app::applyBundledMoltenVkIcd();
    // Runtime packs after QApp.
    if (!tibia::mods::ModLoader::skipLoad())
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
    parser.addOption({ QStringLiteral("smoke-action"),
                       QStringLiteral("After entering the world (login or replay), publish this game action through the action router (EGameAction name, optional :<delay ms> after the previous step, default 1000; repeatable, in order; CONFIRM_TUTORIAL_HINT confirms the hint shown). Smoke tests."),
                       QStringLiteral("action[:ms]") });
    parser.addOption({ QStringLiteral("smoke-confirm-hint"),
                       QStringLiteral("Confirm every tutorial hint half a second after it shows (smoke tests).") });
    // CIP client Play: launcher asserts the package/assets stamps already match.
    parser.addOption({ QStringLiteral("package-is-up-to-date"),
                       QStringLiteral("Tip Play: package and assets stamps already match (CIP).") });
    // Launcher settings may carry arguments of the Lua client; they are not fatal.
    parser.parse(commandLine());
    if (parser.isSet(QStringLiteral("help")))
        parser.showHelp();

    const auto env = app::LauncherEnv::load(QDir::currentPath());
#ifdef TIBIA_WITH_GAMECORE
    // CIP: the application's setup (graphics, client options, crash
    // handler, its connections, the command line) before the game starts.
    application.initialize();
#endif
    // CIP links (dialogs, translations, Wheel planner) → this deployment's origins.
    app::PortalUrls::installCipUrlHandler(env);
    if (!parser.isSet(QStringLiteral("preview")) && !parser.isSet(QStringLiteral("replay"))
        && !parser.isSet(QStringLiteral("check-qml")) && !parser.isSet(QStringLiteral("check-hotkeys"))
        && !parser.isSet(QStringLiteral("account"))
        && app::LauncherEnv::startLauncherIfUpdateNeeded())
        return 0;

    app::mountGraphicsResources(dataDirs(parser.value(QStringLiteral("data"))));
#ifndef TIBIA_WITH_GAMECORE
    app::installTranslations();
#endif
    app::installUiFonts(dataDirs(parser.value(QStringLiteral("data"))));

    auto engine = app::createEngine();
#ifndef TIBIA_WITH_GAMECORE
    // Host: without a game client the shell makes what the client's setup
    // would (the image providers, the mouse cursor and text copy controllers).
    imageproviders::registerImageProviders(engine.get());
    gamewindow::TMouseCursorController::createInstance(
        engine.get(), std::shared_ptr<config::TClientOptions>(config::TClientOptions::instance(), [](config::TClientOptions*) {}));
    gamewindow::TTextCopyController::createInstance(engine.get());
#endif
    // Neutral placeholders; packs replace via ModContext / suite wire.
    engine->rootContext()->setContextProperty(QStringLiteral("modOptions"), QVariant::fromValue<QObject*>(nullptr));
    engine->rootContext()->setContextProperty(QStringLiteral("modHudLayout"), QVariant::fromValue<QObject*>(nullptr));
    if (parser.isSet(QStringLiteral("check-qml")))
        return checkQml(*engine) == 0 ? 0 : 1;
#ifdef TIBIA_WITH_GAMECORE
    if (parser.isSet(QStringLiteral("check-hotkeys"))) {
        const QString error = tibia::config::THotkeyOptions::selfCheckRoundTrip();
        if (!error.isEmpty()) {
            return 1;
        }
        return 0;
    }
#endif

    const bool preview = parser.isSet(QStringLiteral("preview"));
    gamewindow::TCharacterSelectionController previewController;
    gamewindow::TCharacterSelectionController* controller = &previewController;
    QObject::connect(&previewController, &gamewindow::TCharacterSelectionController::abort, &application, &QCoreApplication::quit);
    if (preview) {
        previewController.setPremium(1, 365);
        previewController.setCharacters(previewCharacters(), previewWorlds());
        previewController.setLastSelectedCharacterName(QStringLiteral("Aria Stormblade"));
    }

    QQuickWindow* window = nullptr;
    // qtclient's own window steps once the client window exists.
    const auto setUpHostWindow = [&](QQuickWindow* clientWindow, QQmlEngine* qmlEngine) {
        window = clientWindow;
        // WebGL: no vertex attribute array outlives a frame (grabToImage's mid-frame finish).
        tibia::platform::resetVertexAttribArraysAfterFrames(window);
        // QRhi::Timestamps for lastCompletedGpuTime (raycast pass GPU). Must be set before the
        // scene graph creates the RHI. weather.perf also uses rhi->finish stalls when debug.
        if (!qEnvironmentVariableIsEmpty("QTCLIENT_PERF") || !qEnvironmentVariableIsEmpty("QTCLIENT_GPU_PERF")) {
            QQuickGraphicsConfiguration config = window->graphicsConfiguration();
            config.setTimestamps(true);
            window->setGraphicsConfiguration(config);
        }
#ifdef TIBIA_WITH_GAMECORE
        app::qtWindow().attach(window);
#endif
        app::alignTextHinting(qmlEngine);

        QQuickItem* placeholder = window->findChild<QQuickItem*>(QStringLiteral("placeholder"));
        placeholder->setProperty("windowTitle", QCoreApplication::applicationName());
        // Tip CDN splash (images/launcher/BackgroundImage.jpg); RCC title.jpg on miss.
        app::applyLoginSplash(window, placeholder, env.assetBase);
        return !(env.isRelease() && !env.ssoHosted());
    };

#ifdef TIBIA_WITH_GAMECORE
    application.setQmlEngine(std::move(engine));
    const auto startClient = [&](client::TGameClient& gameClient, QQuickWindow* clientWindow, QQuickItem* placeholder) {
        gameClient.setWindow(clientWindow);
        client::TGameClient::TGameWindowSetup setup;
        setup.engine = application.qmlEngine();
        setup.window = clientWindow;
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
        return gameClient.initializeWithStartMode(0, setup);
    };
    if (!application.showClientWindow(
            [&](QQuickWindow* clientWindow) { return setUpHostWindow(clientWindow, application.qmlEngine()); },
            startClient))
        return 1;
    // Host: scripted smoke steps through the action router, so smoke runs
    // never need input on the desktop.
    if (!preview && (parser.isSet(QStringLiteral("smoke-action")) || parser.isSet(QStringLiteral("smoke-confirm-hint")))) {
        std::vector<app::TSmokeActions::TStep> steps;
        QString error;
        if (!app::TSmokeActions::parse(parser.values(QStringLiteral("smoke-action")), steps, error)) {
            qCritical().noquote() << error;
            return 1;
        }
        client::TGameClient* gameClient = application.gameClient();
        auto* smoke = new app::TSmokeActions(gameClient->gameActionRouter(), std::move(steps),
                                             parser.isSet(QStringLiteral("smoke-confirm-hint")), 500, gameClient);
        QObject::connect(gameClient, &client::TGameClient::worldEntered, smoke, &app::TSmokeActions::start,
                         Qt::SingleShotConnection);
    }
#else
    Q_UNUSED(controller);
    Q_UNUSED(preview);
    QQmlComponent windowComponent(engine.get(), QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/clientwindow.qml")));
    std::unique_ptr<QObject> windowObject(windowComponent.create());
    auto* shellWindow = qobject_cast<QQuickWindow*>(windowObject.get());
    if (!shellWindow) {
        qCritical() << "Component creation error: " << windowComponent.errors();
        return 1;
    }
    tibia::platform::adaptMainWindow(shellWindow);
    if (!setUpHostWindow(shellWindow, engine.get()))
        return 1;
#endif

    if (const QString shot = parser.value(QStringLiteral("screenshot")); !shot.isEmpty()) {
        const auto takeShot = [window, shot] {
            const bool saved = window->grabWindow().save(shot);
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
            QObject::connect(application.gameClient(), &client::TGameClient::worldEntered, window, [window, takeShot, delay] {
                QTimer::singleShot(delay, window, takeShot);
            }, Qt::SingleShotConnection);
        } else
#endif
            QTimer::singleShot(2000, window, takeShot);
    }

    tibia::platform::notifyUiReady();
    // An uncaught C++ exception leaving a Qt event handler unwinds
    // QEventLoop::exec and leaves the WASM module frozen (Safari /play after
    // world enter). Catch here so the console shows `what()` and play.diag can
    // report it — never let the exception escape main.
    try {
        return application.exec();
    } catch (const std::exception& e) {
        tibia::platform::reportFatalException(e.what());
        return 1;
    } catch (...) {
        tibia::platform::reportFatalException(nullptr);
        return 1;
    }
}
