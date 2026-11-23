#include "launcherapplication.h"

#include "hostprocess.h"

#include <QScreen>

#include "brand.h"
#include "cloudflarecdntrace.h"
#include "downloadpool.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "installationmanager.h"
#include "launcherconfigurationhandler.h"
#include "launcherfilesystemhelper.h"
#include "launchermainwindow.h"
#include "launcherselfupdater.h"
#include "package.h"
#include "systemsurvey/systemsurveyasjsondocumentcreator.h"
#include "systemsurvey/systemsurveycreator.h"
#include "systemsurvey/systemsurveytojsonconverter.h"
#include "systemsurvey/systemsurveyuploader.h"
#include "systemsurvey/systemsurveyvalueretriever.h"
#include "jsondocumenthttppostsender.h"
#include "logging.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QSplashScreen>

#ifndef Q_OS_WIN
#include <sys/file.h>
#endif

namespace launcher {

// --- TApplicationLock --------------------------------------------------------------------

TApplicationLock::TApplicationLock(const QString& directory)
    : m_file(QDir(directory).absoluteFilePath(QStringLiteral(".running")))
{
    if (!m_file.open(QIODevice::ReadWrite))
        return;
#ifndef Q_OS_WIN
    if (flock(m_file.handle(), LOCK_EX | LOCK_NB) != 0)
        m_file.close();
#endif
}

TApplicationLock::~TApplicationLock()
{
    release();
}

void TApplicationLock::release()
{
    if (!m_file.isOpen())
        return;
#ifndef Q_OS_WIN
    flock(m_file.handle(), LOCK_UN);
#endif
    m_file.close();
    m_file.remove();
}

// --- TLauncherApplication ----------------------------------------------------------------

TLauncherApplication::TLauncherApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_firstLaunchOption(QStringList{QStringLiteral("l"), QStringLiteral("first-launch")},
                          QStringLiteral("Automatically download default packages when starting up."))
    , m_skipSelfUpdateOption(QStringList{QStringLiteral("s"), QStringLiteral("skip-selfupdate")},
                             QStringLiteral("Do not perform self-update check on launch."))
    , m_showUiOption(QStringList{QStringLiteral("u"), QStringLiteral("show-ui")},
                     QStringLiteral("Show list of available packages after initial checks."))
    , m_patchOption(QStringList{QStringLiteral("p"), QStringLiteral("patch")},
                    QStringLiteral("Start update of <patchexecutable> without user interaction."),
                    QStringLiteral("patchexecutable"))
{
    // Host: identity comes from the tip pack brand (CipSoft defaults otherwise).
    LauncherBrand::load(argc, argv);
    const LauncherBrand& brand = LauncherBrand::instance();
    setApplicationName(brand.productName);
    setApplicationDisplayName(applicationName());
    setOrganizationName(brand.productOrg);
    setApplicationVersion(launcherVersion());

    m_networkAccessManager = std::make_shared<QNetworkAccessManager>();
    m_downloadPool = std::make_shared<tibia::shared::TDownloadPool>(
        6, std::make_shared<tibia::shared::QNetworkAccessManagerWrapper>(m_networkAccessManager));
    m_filesystemHelper = std::make_shared<TLauncherFilesystemHelper>();
    tibia::shared::TDownloadPool::setPreferIPv4(m_filesystemHelper->ipV4Mode());
    m_cdnTrace = std::make_shared<TCloudflareCdnTrace>(m_filesystemHelper->cdnDiagnosticsTraceUrl(), m_downloadPool,
                                                       m_filesystemHelper->retries(),
                                                       m_filesystemHelper->configurationTransportTimeout());
    m_configurationHandler = std::make_unique<TLauncherConfigurationHandler>(m_downloadPool, m_filesystemHelper);
    m_selfUpdater = std::make_unique<TLauncherSelfUpdater>(m_networkAccessManager, m_filesystemHelper);

    const QDir dataDirectory = TLauncherFilesystemHelper::dataDirectory();
    m_lock = std::make_unique<TApplicationLock>(dataDirectory.canonicalPath());

    m_exitTimer.setSingleShot(true);
    m_exitTimer.setInterval(m_filesystemHelper->clientStartTimeout());
    connect(&m_exitTimer, &QTimer::timeout, this, []() { QCoreApplication::exit(0); });

    connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::mainConfigurationLoaded, this,
            &TLauncherApplication::onMainConfigurationLoaded);
    connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::mainConfigurationDownloadFailed, this,
            &TLauncherApplication::onMainConfigurationDownloadFailed);
    connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::diskAccessFailed, this,
            &TLauncherApplication::onDiskAccessFailed);
    connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::packageConfigurationDownloadsFinished, this,
            &TLauncherApplication::onPackageConfigurationDownloadsFinished);
    connect(m_selfUpdater.get(), &TLauncherSelfUpdater::updateDownloadFinishedAndRestartRequired, this,
            &TLauncherApplication::onSelfUpdateFinished);
    connect(m_selfUpdater.get(), &TLauncherSelfUpdater::updateFailed, this,
            &TLauncherApplication::startPackageManagement);
    connect(m_selfUpdater.get(), &TLauncherSelfUpdater::diskAccessFailed, this,
            &TLauncherApplication::onDiskAccessFailed);

    tibia::shared::setUpLogging(
        TLauncherFilesystemHelper::logDirectory().filePath(QStringLiteral("tibia-launcher.log")), 10 * 1024 * 1024);
    m_parser.addOption(m_firstLaunchOption);
    m_parser.addOption(m_skipSelfUpdateOption);
    m_parser.addOption(m_showUiOption);
    m_parser.addOption(m_patchOption);
    // Host flags (--no-mods) are not CipSoft options; parse() ignores unknown ones.
    m_parser.parse(arguments());
}

TLauncherApplication::~TLauncherApplication()
{
    m_mainWindow.reset();
    m_splash.reset();
}

bool TLauncherApplication::isOnlyInstance() const
{
    return m_lock->isLocked();
}

void TLauncherApplication::start()
{
    m_configurationHandler->downloadMainConfiguration();
}

void TLauncherApplication::startSystemSurvey()
{
    // Host: the url comes from mod configuration, not the tip's "hardwaresurvey".
    const QUrl url = m_filesystemHelper->systemSurveyUrl();
    if (!url.isValid())
        return;
    auto creator = std::make_shared<tibia::systemsurvey::TSystemSurveyCreator>(
        std::make_shared<tibia::systemsurvey::TSystemSurveyValueRetriever>());
    auto documentCreator = std::make_shared<tibia::systemsurvey::TSystemSurveyAsJsonDocumentCreator>(
        creator, std::make_shared<tibia::systemsurvey::TSystemSurveyToJsonConverter>());
    m_systemSurveyUploader = std::make_unique<tibia::systemsurvey::TSystemSurveyUploader>(
        url, documentCreator, std::make_shared<tibia::shared::TJsonDocumentHttpPostSender>());
    m_systemSurveyUploader->uploadSystemSurvey();
}

void TLauncherApplication::onMainConfigurationLoaded()
{
    startSystemSurvey();
    m_configurationHandler->downloadPackageConfigurations(true);
    TLauncherFilesystemHelper::linkLauncherIntoDataDirectory();
}

void TLauncherApplication::onMainConfigurationDownloadFailed()
{
    hideSplash();
    QMessageBox::information(nullptr, QStringLiteral("%1 cannot be started").arg(applicationName()),
                             QStringLiteral("%1 is currently down for maintenance. Please try again later.")
                                 .arg(applicationName()));
    QCoreApplication::exit(1);
}

void TLauncherApplication::onDiskAccessFailed(const QString& errorMessage)
{
    hideSplash();
    QMessageBox::critical(nullptr, QStringLiteral("%1 cannot start").arg(applicationName()),
                          QStringLiteral("A critical error occurred while accessing a file:\n") + errorMessage
                              + QStringLiteral("\nPlease reinstall %1.").arg(applicationName()));
    QCoreApplication::exit(1);
}

void TLauncherApplication::onPackageConfigurationDownloadsFinished()
{
    const std::shared_ptr<TPackage> launcherPackage = m_configurationHandler->launcherPackage();
    if (!m_parser.isSet(m_skipSelfUpdateOption) && !m_selfUpdater->isUpToDate(launcherPackage.get())) {
        m_selfUpdater->update(launcherPackage);
        showSplash(SplashUpdatingLauncher);
        return;
    }
    m_selfUpdater->removeBackupFiles();
    startPackageManagement();
}

void TLauncherApplication::onSelfUpdateFinished()
{
    qDebug() << "Self update finished. Restarting" << QCoreApplication::applicationFilePath();
    m_lock->release();
    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty())
        arguments.removeFirst();
    arguments.append(QStringLiteral("--%1").arg(m_skipSelfUpdateOption.names().constLast()));
    const bool started = host::startDetached(QCoreApplication::applicationFilePath(), arguments, QDir::currentPath());
    if (started)
        qDebug() << "Followup process successfully started.";
    else
        qDebug() << "Followup process failed to start.";
    QCoreApplication::exit(started ? 0 : 1);
}

void TLauncherApplication::startPackageManagement()
{
    m_installationManager = std::make_unique<TInstallationManager>(m_configurationHandler->configuration(),
                                                                   m_networkAccessManager, m_filesystemHelper);
    // Packages whose directory holds a FORCE_PATCH marker are updated now.
    std::vector<std::shared_ptr<TPackage>> forcedPackages;
    if (m_configurationHandler->configuration()) {
        for (const std::shared_ptr<TPackage>& package : m_configurationHandler->configuration()->packages()) {
            QFile marker(TLauncherFilesystemHelper::packageDirectory(*package).absoluteFilePath(
                TLauncherFilesystemHelper::forcePatchMarkerFileName()));
            if (marker.exists()) {
                marker.remove();
                forcedPackages.push_back(package);
            }
        }
    }
    connect(m_installationManager.get(), &TInstallationManager::allStartedUpdatesFinished, this,
            &TLauncherApplication::onAllStartedUpdatesFinished);
    connect(m_installationManager.get(), &TInstallationManager::diskAccessFailed, this,
            &TLauncherApplication::onDiskAccessFailed);
    hideSplash();
    if (m_filesystemHelper->eulaAccepted() && !m_filesystemHelper->prerequisitesInstalled())
        installPrerequisites();

    if (m_parser.isSet(m_showUiOption)) {
        showMainWindow();
        m_installationManager->installAutomaticPackages();
        return;
    }
    if (!m_parser.isSet(m_patchOption) && forcedPackages.empty()) {
        if (!m_parser.isSet(m_firstLaunchOption)) {
            const std::shared_ptr<TPackage> defaultPackage = m_installationManager->defaultPackage();
            if (defaultPackage && defaultPackage->localStatus() == TPackage::InstalledAndUsable) {
                m_installationManager->loadInstalledConfiguration(*defaultPackage);
                launchPackage(defaultPackage);
                return;
            }
        }
        showMainWindow();
        return;
    }
    const std::shared_ptr<TPackage> package =
        m_installationManager->packageByExecutable(m_parser.value(m_patchOption));
    showMainWindow();
    if (package)
        forcedPackages.push_back(package);
    if (forcedPackages.empty()) {
        QMessageBox::information(m_mainWindow.get(), QStringLiteral("Client patching failed"),
                                 QStringLiteral("Could not determine patch information. Your client might no longer "
                                                "be supported."));
        return;
    }
    for (const std::shared_ptr<TPackage>& forced : forcedPackages)
        m_installationManager->installOrUpdate(forced);
}

static std::vector<std::shared_ptr<TPackage>> availablePackages(const TInstallationManager& manager)
{
    // Packages without a build for this machine are not offered.
    std::vector<std::shared_ptr<TPackage>> packages;
    for (const std::shared_ptr<TPackage>& package : manager.packages()) {
        if (package->configurationForCurrentPlatform())
            packages.push_back(package);
    }
    return packages;
}

void TLauncherApplication::showMainWindow()
{
    if (!m_mainWindow) {
        m_mainWindow = std::make_unique<TLauncherMainWindow>(
            availablePackages(*m_installationManager), m_installationManager.get(),
            m_configurationHandler->showPackageNames(), m_networkAccessManager, m_filesystemHelper->eulaAccepted(),
            m_downloadPool, m_cdnTrace, m_filesystemHelper);
        if (m_configurationHandler->configuration())
            m_mainWindow->setHintBaseUrl(m_configurationHandler->configuration()->hintsUrl());
        m_configurationHandler->downloadHints();
        m_mainWindow->setHint(m_configurationHandler->hint());
        connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::hintAreaDataReceived,
                m_mainWindow.get(), &TLauncherMainWindow::setHint);
        connect(m_mainWindow.get(), &TLauncherMainWindow::defaultChanged, this,
                &TLauncherApplication::onDefaultChanged);
        connect(m_mainWindow.get(), &TLauncherMainWindow::clientPackageButtonClicked, this,
                &TLauncherApplication::onClientPackageButtonClicked);
        connect(m_mainWindow.get(), &TLauncherMainWindow::eulaAccepted, this, &TLauncherApplication::onEulaAccepted);
#ifdef Q_OS_WASM
        // Host: on the web the page is the desktop. The window is shown normal
        // (Qt for WebAssembly shows top-level windows full screen by default)
        // and kept centred, as CipSoft centres it on the screen.
        const auto centre = [window = m_mainWindow.get()]() {
            window->adjustSize();
            const QRect page = window->screen()->availableGeometry();
            window->move(page.center() - QPoint(window->width() / 2, window->height() / 2));
        };
        centre();
        connect(m_mainWindow->screen(), &QScreen::availableGeometryChanged, m_mainWindow.get(), centre);
#endif
    }
#ifdef Q_OS_WASM
    m_mainWindow->showNormal();
#else
    m_mainWindow->show();
    // Host: Finder-launched CipSoft is activated by LaunchServices; a tip started
    // from the terminal often stays behind the shell unless we raise it.
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
#endif
}

void TLauncherApplication::onAllStartedUpdatesFinished()
{
    if (m_mainWindow && m_configurationHandler->preventDefaultAutostart())
        return;
    if (m_parser.isSet(m_showUiOption)) {
        const std::shared_ptr<TPackage> defaultPackage = m_installationManager->defaultPackage();
        if (defaultPackage && defaultPackage->localStatus() == TPackage::InstalledAndUsable) {
            launchPackage(defaultPackage);
            return;
        }
    }
    const QString patch = m_parser.isSet(m_patchOption) ? m_parser.value(m_patchOption) : QString();
    if (!patch.isEmpty()) {
        if (const std::shared_ptr<TPackage> package = m_installationManager->packageByExecutable(patch))
            launchPackage(package);
    }
}

void TLauncherApplication::onClientPackageButtonClicked(const std::shared_ptr<TPackage>& package)
{
    if (package->localStatus() == TPackage::InstalledAndUsable) {
        m_installationManager->loadInstalledConfiguration(*package);
        launchPackage(package);
        return;
    }
    if (package->localStatus() == TPackage::NotInstalled || package->localStatus() == TPackage::PartiallyInstalled)
        m_installationManager->installOrUpdate(package);
}

void TLauncherApplication::onDefaultChanged(const std::shared_ptr<TPackage>& package, bool isDefault)
{
    m_installationManager->setDefaultPackage(isDefault ? package : nullptr);
}

void TLauncherApplication::onEulaAccepted()
{
    m_filesystemHelper->setEulaAccepted(true);
    installPrerequisites();
}

void TLauncherApplication::installPrerequisites()
{
    // CipSoft installs BattlEye here (Epic Games Store builds only); out of scope.
}

void TLauncherApplication::launchPackage(const std::shared_ptr<TPackage>& package)
{
    const QString executable = m_installationManager->executablePath(*package);
    QStringList parameters = m_installationManager->executableParameters(*package);
    qDebug() << "Launching executable of Package " << package->name() << " : " << executable;
    if (package->isUpToDate())
        parameters.append(QStringLiteral("--package-is-up-to-date"));
    // Host: the client finds the launcher and the pack the same way as CipSoft's.
    if (LauncherBrand::instance().noMods)
        parameters.append(QStringLiteral("--no-mods"));
    if (executable.isEmpty())
        return;
    const QString workingDirectory = QFileInfo(executable).absoluteDir().absolutePath();
    const TPackageConfiguration* configuration = package->configurationForCurrentPlatform();
    const QString packageRevision = configuration ? configuration->packageVersion().trimmed() : QString();
    hostResolveLoginRevision(packageRevision, [this, executable, parameters, workingDirectory](const QString& revision) {
        // Host: the qtclient launcher → client contract (login revision, launcher path, CDN).
        QHash<QString, QString> environment;
        environment.insert(QStringLiteral("QTCLIENT_LAUNCHER"), QStringLiteral("1"));
        environment.insert(QStringLiteral("QTCLIENT_CLIENT_REVISION"), revision);
        environment.insert(QStringLiteral("QTCLIENT_LAUNCHER_BIN"), QCoreApplication::applicationFilePath());
        if (const QString cdn = TLauncherFilesystemHelper::cdnBase(); !cdn.isEmpty())
            environment.insert(QStringLiteral("QTCLIENT_ASSET_BASE"), cdn);
        if (!host::startDetached(executable, parameters, workingDirectory, environment)) {
            QCoreApplication::exit(1);
            return;
        }
        m_clientStartTimer.start();
        m_exitTimer.start();
        if (m_mainWindow)
            m_mainWindow->close();
    });
}

void TLauncherApplication::hostResolveLoginRevision(const QString& fallback, std::function<void(QString)> then)
{
    // The portal accepts the live latest/release.json "revision" at login; a
    // package version ("15.33.<sha>") would be refused with errorCode 7.
    const QUrl manifest = m_filesystemHelper->releaseManifestUrl();
    if (!manifest.isValid() || manifest.isEmpty()) {
        then(fallback);
        return;
    }
    auto finished = std::make_shared<bool>(false);
    auto connections = std::make_shared<std::vector<QMetaObject::Connection>>();
    const auto finish = [finished, connections, fallback, then](const QString& revision) {
        if (*finished)
            return;
        *finished = true;
        for (const QMetaObject::Connection& connection : *connections)
            QObject::disconnect(connection);
        then(revision.isEmpty() ? fallback : revision);
    };
    connections->push_back(connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::success, this,
                                   [manifest, finish](const QUrl& url, const QByteArray& data) {
                                       if (url != manifest)
                                           return;
                                       finish(QJsonDocument::fromJson(data)
                                                  .object()
                                                  .value(QStringLiteral("revision"))
                                                  .toString()
                                                  .trimmed());
                                   }));
    connections->push_back(connect(m_downloadPool.get(), &tibia::shared::TDownloadPool::failed, this,
                                   [manifest, finish](const QUrl& url, QNetworkReply::NetworkError, const QString& error) {
                                       if (url != manifest)
                                           return;
                                       finish(QString());
                                   }));
    m_downloadPool->download(manifest, m_filesystemHelper->retries(), m_filesystemHelper->configurationTransportTimeout());
}

void TLauncherApplication::showSplash(ESplashState state)
{
    QString image;
    switch (state) {
    case SplashLaunchingClient:
        image = QStringLiteral("TibiaLogo_LaunchingClient.png");
        break;
    case SplashUpdatingLauncher:
        image = QStringLiteral("TibiaLogo_UpdatingLauncher.png");
        break;
    default:
        image = QStringLiteral("TibiaLogo_QT.png");
    }
    const QPixmap pixmap(LauncherBrand::instance().imagePath(image));
    if (!m_splash)
        m_splash = std::make_unique<QSplashScreen>(pixmap);
    else
        m_splash->setPixmap(pixmap);
    m_splash->show();
}

void TLauncherApplication::hideSplash()
{
    if (m_splash)
        m_splash->hide();
}

} // namespace launcher
