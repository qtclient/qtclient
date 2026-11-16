#include "launcherapplication.h"

#include "brand.h"
#include "cloudflarecdntrace.h"
#include "host/downloadpool.h"
#include "installationmanager.h"
#include "launcherconfigurationhandler.h"
#include "launcherfilesystemhelper.h"
#include "launchermainwindow.h"
#include "launcherselfupdater.h"
#include "package.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QProcess>
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

// --- logging -------------------------------------------------------------------------------

namespace {

QFile* g_logFile = nullptr;
qint64 g_logMaxSize = 0;
QMutex g_logMutex;

void writeLog(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    const QByteArray line = QStringLiteral("[ %1 ] %2\n")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss,zzz")),
                                     message)
                                .toUtf8();
    fputs(line.constData(), stdout);
    QMutexLocker locker(&g_logMutex);
    if (!g_logFile)
        return;
    if (g_logFile->size() + line.size() > g_logMaxSize) {
        // Rotate: keep one previous log.
        const QString path = g_logFile->fileName();
        g_logFile->close();
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
        g_logFile->open(QIODevice::WriteOnly | QIODevice::Append);
    }
    g_logFile->write(line);
    g_logFile->flush();
    if (type == QtFatalMsg)
        abort();
}

} // namespace

void TLauncherApplication::setUpLogging(const QString& logFile, qint64 maxSize)
{
    g_logFile = new QFile(logFile);
    g_logFile->open(QIODevice::WriteOnly | QIODevice::Append);
    g_logMaxSize = maxSize;
    qInstallMessageHandler(writeLog);
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
    m_downloadPool = std::make_shared<host::TDownloadPool>(m_networkAccessManager, 6);
    m_filesystemHelper = std::make_shared<TLauncherFilesystemHelper>();
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

    setUpLogging(TLauncherFilesystemHelper::logDirectory().filePath(QStringLiteral("tibia-launcher.log")),
                 10 * 1024 * 1024);
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
    qInstallMessageHandler(nullptr);
}

bool TLauncherApplication::isOnlyInstance() const
{
    return m_lock->isLocked();
}

void TLauncherApplication::start()
{
    m_configurationHandler->downloadMainConfiguration();
}

void TLauncherApplication::onMainConfigurationLoaded()
{
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
    const bool started =
        QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments, QDir::currentPath());
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
    const QString patch = m_parser.isSet(m_patchOption) ? m_parser.value(m_patchOption) : QString();
    if (patch.isEmpty()) {
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
    const std::shared_ptr<TPackage> package = m_installationManager->packageByExecutable(patch);
    showMainWindow();
    if (!package) {
        QMessageBox::information(m_mainWindow.get(), QStringLiteral("Client patching failed"),
                                 QStringLiteral("Could not determine patch information. Your client might no longer "
                                                "be supported."));
        return;
    }
    m_installationManager->installOrUpdate(package);
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
            m_downloadPool, m_cdnTrace);
        m_configurationHandler->downloadHints();
        m_mainWindow->setHint(m_configurationHandler->hint());
        connect(m_configurationHandler.get(), &TLauncherConfigurationHandler::hintAreaDataReceived,
                m_mainWindow.get(), &TLauncherMainWindow::setHint);
        connect(m_mainWindow.get(), &TLauncherMainWindow::defaultChanged, this,
                &TLauncherApplication::onDefaultChanged);
        connect(m_mainWindow.get(), &TLauncherMainWindow::clientPackageButtonClicked, this,
                &TLauncherApplication::onClientPackageButtonClicked);
        connect(m_mainWindow.get(), &TLauncherMainWindow::eulaAccepted, this, &TLauncherApplication::onEulaAccepted);
    }
    m_mainWindow->show();
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
    // Host: the qtclient launcher → client contract (login revision, launcher path, CDN).
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QTCLIENT_LAUNCHER"), QStringLiteral("1"));
    if (const TPackageConfiguration* configuration = package->configurationForCurrentPlatform())
        environment.insert(QStringLiteral("QTCLIENT_CLIENT_REVISION"), configuration->packageVersion().trimmed());
    environment.insert(QStringLiteral("QTCLIENT_LAUNCHER_BIN"), QCoreApplication::applicationFilePath());
    if (!qEnvironmentVariableIsEmpty("QTCLIENT_CDN_URL"))
        environment.insert(QStringLiteral("QTCLIENT_ASSET_BASE"), qEnvironmentVariable("QTCLIENT_CDN_URL"));
    QProcess process;
    process.setProgram(executable);
    process.setArguments(parameters);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
        QCoreApplication::exit(1);
        return;
    }
    m_clientStartTimer.start();
    m_exitTimer.start();
    if (m_mainWindow)
        m_mainWindow->close();
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
