#include "gamecore.h"
#include "tibia/runtime/host.h"

#include <client/client.h>
#include <tibia/config/runtimegameconfig.h>
#include <tibia/assets/loader.h>
#include <tibia/assets/things.h>
#include "clockservice.h"
#include <tibia/schedule/schedule.h>
#include "shared/tibiafilesystemhelper.h"
#include <tibia/renderer/texturedeletionqueue.h>
#include "mods/registry.h"
#include "network/gameserverconnection.h"
#include "sound/soundstorage.h"
#include "assets/assetcatalog.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QObject>
#include <QStandardPaths>

#include <csignal>
#include <map>
#include <tibia/game/gamesession.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/tgamenotify.h>

#ifndef TIBIA_CLIENT_PACKAGE_DIR
#define TIBIA_CLIENT_PACKAGE_DIR "Tibia.app"
#endif

namespace tibia::client::gamecore {

namespace {

bool running = false;
bool forceExit = false;
volatile std::sig_atomic_t exitSignaled = 0;
std::function<void()> forceExitHandler;

void onExitSignal(int)
{
    exitSignaled = 1;
}

/// Runs on the GUI thread, after the event that noticed the signal has returned.
/// The handler shows Exit / Logout / Cancel. Quitting here would close the
/// window from the signal and skip that prompt. Doing this from afterAnimating
/// is inside QPlatformWindow::deliverUpdateRequest; closing the window there
/// frees it and the timer's hasPendingUpdateRequest then segfaults.
void finishExit()
{
    if (forceExitHandler)
        forceExitHandler();
    else
        QCoreApplication::quit();
}


}

bool init(const QString& argv0, const QString& workDir)
{
    if (running)
        return true;

    const QByteArray argv0Bytes = argv0.toLocal8Bit();
    std::vector<std::string> args{ argv0Bytes.toStdString() };
    tibia::files::data().init(argv0Bytes.constData());

    const QString previousDir = QDir::currentPath();
    QDir::setCurrent(workDir);
    const bool found = tibia::files::data().discoverWorkDir("data/setup.otml");
    QDir::setCurrent(previousDir);
    if (!found) {
        qCritical().noquote() << "engine: no data/setup.otml under" << workDir;
        return false;
    }
    // Virtual paths resolve under workDir. CIP assets use one Resources tree
    // (see assetsRoot). workDir "/" must become "/data", not "//data"
    // (wasm MEMFS will not open the latter).
    std::string dataRoot = tibia::files::data().getWorkDir();
    while (!dataRoot.empty() && (dataRoot.back() == '/' || dataRoot.back() == '\\'))
        dataRoot.pop_back();
    tibia::files::data().addSearchPath(dataRoot, true);
    tibia::files::data().addSearchPath(dataRoot + "/data", true);


    std::signal(SIGTERM, onExitSignal);
    std::signal(SIGINT, onExitSignal);
    g_client.init(args);

    tibia::files::data().setupUserWriteDir("tibia/");
    tibia::network::installGameServerConnection();

    // Exit is qtclient schedule / Qt (SIGINT→finishExit, forceExitHandler) — not OTC g_app/onExit.

    running = true;
    return true;
}

void terminate()
{
    if (!running)
        return;
    running = false;

    if (tibia::game::isOnline())
        tibia::game::hubDisconnectNow();

    g_client.terminate();
}

bool isRunning()
{
    return running;
}

bool isForceExit()
{
    return forceExit;
}

void setForceExitHandler(std::function<void()> handler)
{
    forceExitHandler = std::move(handler);
}

void poll()
{
    if (!running)
        return;
    // Main/Qt tick — qtclient renderer drains deferred OTC Texture GL deletes here.
    tibia::renderer::flushTextureDeletions();
    if (exitSignaled) {
        exitSignaled = 0;
        // Queue it: this poll often runs from afterAnimating, still inside the
        // window update timer (see finishExit). Do not mark a force-exit —
        // that used to close the window without Exit / Logout / Cancel.
        // Cancel leaves the process up, so a later signal can ask again.
        if (QCoreApplication* app = QCoreApplication::instance())
            QMetaObject::invokeMethod(app, finishExit, Qt::QueuedConnection);
        else
            finishExit();
    }
}

qint64 nowMicros()
{
    return shared::TClockService::process()->nowMicros();
}

void advanceTo(const qint64 micros)
{
    if (!running)
        return;
    // Never step the engine clock backward. Predicted time is now+offset; when the
    // frame-duration offset shrinks, predict() can fall below the previous frame's
    // predicted stamp. Allowing that rewind made DurationTween / foot clocks jump
    // and compressed auto-walk steps while key-hold (real-time v9) stayed smooth.
    shared::TClockService::setFrameMicros(
        std::max<qint64>({ micros, shared::TClockService::process()->nowMicros(), shared::TClockService::frameMicros() }));
}

void resumeRealTime()
{
    shared::TClockService::updateFrame();
}

void setClientVersion(const int version)
{
    // gamelib getClientProtocolVersion: only 980-1002 clients speak an older protocol.
    static const std::map<int, int> olderProtocol{
        { 980, 971 }, { 981, 973 }, { 982, 974 }, { 983, 975 }, { 984, 976 },
        { 985, 977 }, { 986, 978 }, { 1001, 979 }, { 1002, 980 },
    };
    const auto it = olderProtocol.find(version);
    // gamelib: the newest entry of its supported-clients list.
    if (tibia::config::lastSupportedVersion() < version)
        tibia::config::setLastSupportedVersion(static_cast<uint16_t>(version));
    tibia::game::setClientRevision(static_cast<uint16_t>(version));
    tibia::game::setProtocolVersion(static_cast<uint16_t>(it != olderProtocol.end() ? it->second : version));
}

namespace {

/// CipSoft `TTibiaFileSystemHelper` / :
/// QDir(applicationDirPath); cdUp; cd("./Resources");
/// Same as `applicationDirPath../Resources`. Package trees use
/// `Contents/Resources` (macOS) or `Resources` (win/linux) under the install root.
bool catalogAt(const QString& assetsDir)
{
    return QFileInfo::exists(QDir(assetsDir).filePath(QStringLiteral("catalog-content.json")));
}

bool resourcesDirUsable(const QString& cand)
{
    if (cand.isEmpty())
        return false;
    // Catalog only — never accept a bare existing dir (an empty
    // `applicationDirPath../Resources` must not win over the AppData package).
    return catalogAt(QDir(cand).filePath(QStringLiteral("assets")));
}

bool tryPackageResources(const QString& appData, QString* out)
{
    if (appData.isEmpty() || !out)
        return false;
    // Stock host compiles TIBIA_CLIENT_PACKAGE_DIR=Tibia.app; IdleOT qtclient
    // directoryname is IdleOT.app (portal PACKAGE_CLIENT). Probe both.
    const char* packageNames[] = {
        TIBIA_CLIENT_PACKAGE_DIR,
        "IdleOT.app",
    };
    for (const char* packageName : packageNames) {
        const QString packageResources = QDir(appData).filePath(
            QLatin1String("packages/") + QLatin1String(packageName)
            + QLatin1String("/Contents/Resources"));
        if (resourcesDirUsable(packageResources)) {
            *out = QDir(packageResources).absolutePath();
            return true;
        }
    }
    return false;
}

QString resolveCipResourcesDir()
{
    // One Cip Resources layout everywhere — only the install root differs:
    // bundled app, AppData qtclient package, or monorepo extract (`data/` == Resources).
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        const QString viaApp = QDir(QDir(appDir).absoluteFilePath(QStringLiteral("..")))
                                   .filePath(QStringLiteral("Resources"));
        if (resourcesDirUsable(viaApp))
            return QDir(viaApp).absolutePath();
    }

    QString found;
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (tryPackageResources(appData, &found))
        return found;

    QString work = QString::fromStdString(tibia::files::data().getWorkDir());
    while (work.endsWith(QLatin1Char('/')) || work.endsWith(QLatin1Char('\\')))
        work.chop(1);
    if (!work.isEmpty()) {
        // Monorepo local: apps/qtclient/data is the CipSoft Resources tree
        // (assets/ + sounds/ siblings) when the app-bundle symlink is missing.
        const QString dataResources = QDir(work).filePath(QStringLiteral("data"));
        if (catalogAt(QDir(dataResources).filePath(QStringLiteral("assets"))))
            return QDir(dataResources).absolutePath();
    }
    return {};
}

} // namespace

QString cipResourcesDir()
{
    return resolveCipResourcesDir();
}

std::string assetsRoot(const int /*version*/)
{
    const QString resources = resolveCipResourcesDir();
    if (resources.isEmpty())
        return {};
    // Search root = Resources so CIP short names `assets/…` and `sounds/…` resolve.
    tibia::files::data().addSearchPath(resources.toStdString(), true);
    return "assets/";
}

std::string soundsRoot(const int /*version*/)
{
    const QString resources = resolveCipResourcesDir();
    if (!resources.isEmpty())
        tibia::files::data().addSearchPath(resources.toStdString(), true);
    return "sounds/";
}

namespace {

/// The catalog's map (map.dat) and full map entries, as CIP's asset handler hands
/// them to the Cyclopedia map's static data.
void loadCyclopediaMapData(const std::string& directory)
{
    auto& storage = tibia::cyclopedia::TCyclopediaMapStaticDataStorage::instance();
    try {
        tibia::assets::TAssetCatalog catalog;
        catalog.load(QJsonDocument::fromJson(
            QByteArray::fromStdString(tibia::files::data().readFileContents(directory + "catalog-content.json"))));
        storage.setFullMapFileName(catalog.fileOf(tibia::assets::TAsset::FullMap));
        const QString mapFile = catalog.fileOf(tibia::assets::TAsset::Map);
        if (mapFile.isEmpty())
            return;
        storage.initialize(QByteArray::fromStdString(tibia::files::data().readFileContents(directory + mapFile.toStdString())));
    } catch (const std::exception& e) {
        qWarning("cyclopedia map: %s", e.what());
    }
}

}

bool loadAssets(const int version, QString* error)
{
    const std::string dir = assetsRoot(version);
    QStringList errors;
    if (!tibia::assets::load(dir))
        errors << QStringLiteral("Couldn't load assets from %1").arg(QString::fromStdString(dir));
    if (!tibia::assets::loadStaticData(dir))
        errors << QStringLiteral("Couldn't load static data from %1").arg(QString::fromStdString(dir));
    loadCyclopediaMapData(dir);
    if (!tibia::assets::resolveProficiencies(dir))
        qWarning().noquote() << "gamecore: no proficiencies-<hash>.json in" << QString::fromStdString(dir);

    const std::string soundsDir = soundsRoot(version);
    if (auto* storage = sound::TSoundStorage::instance(); storage && storage->loadBank(soundsDir))
        qInfo().noquote() << "gamecore: loaded sound bank from" << QString::fromStdString(soundsDir);
    // Missing bank is optional (make extract); stay quiet on CipSoft.
    if (error)
        *error = errors.join(u'\n');
    return errors.isEmpty();
}

}
