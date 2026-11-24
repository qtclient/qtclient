#include "client/gameapplication.h"

#include "platform/platform.h"

#include "app/bootstrap.h"
#include "app/clientoptions.h"
#include "client/gameclient.h"
#include "client/preventcloseeventfilter.h"
#include "config/clientoptions.h"
#include "config/clientsettings.h"
#include "gamewindow/gamewindowcontroller.h"
#include "gamewindow/messagedialogcontroller.h"
#include "crashhandler.h"
#include "downloadpool.h"
#include "logging.h"
#include "qnetworkaccessmanagerwrapper.h"
#include "shared/tibiafilesystemhelper.h"

#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSysInfo>
#include <QVariant>

#include <google/protobuf/stubs/common.h>

#include <stdexcept>

extern "C" const char qtclient_host_build_id[];

namespace tibia::client {

TGameApplicationBase::TGameApplicationBase(int& argc, char** argv)
    : QGuiApplication(argc, argv)
    , m_pTibiaFileSystemHelper(std::make_shared<shared::TTibiaFileSystemHelper>())
{
    if (qEnvironmentVariable("QT_D3D_NO_VBLANK_THREAD").isEmpty())
        qputenv("QT_D3D_NO_VBLANK_THREAD", "1");
}

TGameApplicationBase::~TGameApplicationBase()
{
    // Host: the QML engine goes with the application (CipSoft hands it to
    // deleteLater); the protobuf library is shut down.
    google::protobuf::ShutdownProtobufLibrary();
}

void TGameApplicationBase::setQmlEngine(std::unique_ptr<QQmlEngine> Engine)
{
    m_pEngine = std::move(Engine);
}

TGameApplication::TGameApplication(int& argc, char** argv)
    : TGameApplicationBase(argc, argv)
    , m_pDownloadPool(std::make_shared<shared::TDownloadPool>(
          static_cast<int>(config::clientSettingAtLeast(QStringLiteral("NETWORK"), QStringLiteral("maxConcurrentDownloads"), 5, 1)),
          std::make_shared<shared::QNetworkAccessManagerWrapper>(std::make_shared<QNetworkAccessManager>())))
    , m_pPreventCloseEventFilter(new TPreventCloseEventFilter(this))
{
}

TGameApplication::~TGameApplication()
{
    if (m_pClientOptions)
        m_pClientOptions->saveToJsonFile();
    // Host: what main made goes before the QML engine it was made in: a
    // client still there (the event loop did not run), then the window.
    delete m_pGameClient.data();
    delete m_pClientWindow;
    m_pClientWindow = nullptr;
    google::protobuf::ShutdownProtobufLibrary();
}

TGameClient* TGameApplication::gameClient() const
{
    return m_pGameClient.data();
}

void TGameApplication::initialize(const QUrl& LoginUrl)
{
    m_firstStart = !QFile::exists(m_pTibiaFileSystemHelper->getConfigFilePath(QStringLiteral("clientoptions.json")));
    // CIP: the base setup makes the client options. Host: the process setup
    // that reads them ran before the application (app::configureProcess).
    m_pClientOptions = config::TClientOptions::instance();
    setUpCrashHandler(LoginUrl);
    m_clientWindowChangedReactionTimer.setSingleShot(true);
    m_clientWindowChangedReactionTimer.setInterval(50);
    connect(&m_clientWindowChangedReactionTimer, &QTimer::timeout, this, &TGameApplication::onDelayedCliendWindowChanged);
    connect(this, &QCoreApplication::aboutToQuit, this, &TGameApplication::onAboutToQuit);
    connect(m_pPreventCloseEventFilter, &TPreventCloseEventFilter::preventedClose, this, &TGameApplication::onRequestClose);
    connect(m_pClientOptions, &config::TClientOptions::failedToSaveClientOptions, this,
            &TGameApplication::showErrorAsPopupAndCriticalLog);
    m_pClientOptions->saveToJsonFile();
    m_startOptions = readStartOptions();
}

TGameApplication::TStartOptions TGameApplication::readStartOptions()
{
    QCommandLineParser parser;
    const QCommandLineOption packageIsUpToDate(QStringLiteral("package-is-up-to-date"));
    parser.addOption(packageIsUpToDate);
    parser.parse(QCoreApplication::arguments());
    TStartOptions options;
    options.packageIsUpToDate = parser.isSet(packageIsUpToDate);
    return options;
}

bool TGameApplication::showClientWindow(const THostWindowSetup& HostWindowSetup, const TStartClient& StartClient)
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
    // Host: qtclient's client strings come from the bundled client.en.qm.
    app::installTranslations();

    auto* component = new QQmlComponent(qmlEngine(), this);
    const QUrl url(QStringLiteral("clientwindow.qml"));
    component->loadUrl(url);
    if (component->isError()) {
        qCritical() << "Component creation error for " << url.url() << "\n" << component->errors();
        throw std::runtime_error(component->errorString().toStdString());
    }
    if (!component->isReady()) {
        qCritical() << "Qml Component is not ready" << component->status() << component->errors();
        return false;
    }
    m_pClientWindow = qobject_cast<QQuickWindow*>(component->create());
    if (!m_pClientWindow) {
        qCritical() << "Component creation error: " << component->errors();
        return false;
    }
    qmlEngine()->rootContext()->setContextProperty(QStringLiteral("clientRootQmlObject"), m_pClientWindow->contentItem());
    m_pClientWindow->setColor(QColor(QStringLiteral("#041020")));
    if (HostWindowSetup && !HostWindowSetup(m_pClientWindow))
        return false;
    return connectClientWindow(m_startOptions.startMode, StartClient);
}

bool TGameApplication::connectClientWindow(const int StartMode, const TStartClient& StartClient)
{
    QQuickWindow* window = m_pClientWindow;
    // Host: the browser's window fills the page instead.
    m_clientWindowPlacedByPlatform = tibia::platform::adaptMainWindow(window);
    if (!m_clientWindowPlacedByPlatform) {
        applyClientWindowOptions();
        connect(m_pClientOptions, &config::TClientOptions::clientWindowOptionsChanged, this,
                &TGameApplication::applyClientWindowOptions);
    }
    connect(window, &QWindow::xChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window, &QWindow::yChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window, &QWindow::widthChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window, &QWindow::heightChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window, &QWindow::screenChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window->screen(), &QScreen::virtualGeometryChanged, this, &TGameApplication::screenGeometryChanged);
    connect(window, &QWindow::windowStateChanged, this, &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window->screen(), &QScreen::virtualGeometryChanged, this,
            &TGameApplication::startClientWindowChangedReactionTimer);
    connect(window, &QWindow::widthChanged, this, &TGameApplication::windowSizeChanged);
    connect(window, &QWindow::heightChanged, this, &TGameApplication::windowSizeChanged);
    m_pPlaceholder = window->findChild<QQuickItem*>(QStringLiteral("placeholder"));
    window->installEventFilter(m_pPreventCloseEventFilter);
    return createGameClient(StartMode, StartClient);
}

bool TGameApplication::createGameClient(const int StartMode, const TStartClient& StartClient)
{
    // Host: qtclient's client takes the placeholder with its game window
    // (StartClient) and makes its own crash report sender.
    Q_UNUSED(StartMode);
    m_pGameClient = new TGameClient(qmlEngine(), m_pTibiaFileSystemHelper, m_pClientOptions, m_pDownloadPool);
    TGameClient* gameClient = m_pGameClient.data();
    connect(gameClient, &TGameClient::showErrorAsPopupAndCriticalLog, this,
            &TGameApplication::showErrorAsPopupAndCriticalLog);
    connect(gameClient, &TGameClient::clientReadyToClose, this, &TGameApplication::onClientPluginReadyToClose,
            Qt::QueuedConnection);
    connect(gameClient, &TGameClient::numberOfSidebarsChanged, this, &TGameApplication::onNumberOfSidebarsChanged);
    connect(gameClient, &TGameClient::newLeftAndRightStatusActionBarWidth, this,
            &TGameApplication::onLeftAndRightStatusActionBarWidthChanged);
    m_pClientWindow->raise();
    m_pClientWindow->requestActivate();
    const bool started = !StartClient || StartClient(*gameClient, m_pClientWindow, m_pPlaceholder);
    const QList<QString> pendingErrorMessages = m_pendingErrorMessages;
    for (const QString& errorMessage : pendingErrorMessages)
        showErrorAsPopupAndCriticalLog(errorMessage);
    m_pendingErrorMessages.clear();
    return started;
}

void TGameApplication::applyClientWindowOptions()
{
    setHasBorderInFullScreen();
    const config::TClientWindowOptions options = m_pClientOptions->clientWindowOptions();
    if (QScreen* screen = screenForClientWindowOptions(options))
        m_pClientWindow->setScreen(screen);
    if (m_startOptions.startMode != 0)
        return;
    if (m_pGameClient)
        m_pGameClient->setSidebarTriggerCheckBoundsEnabled(false);
    m_pClientWindow->setWindowState(Qt::WindowNoState);
    QRect geometry = options.geometry;
    if (geometry.width() > 0 && geometry.height() > 0) {
        // A window off every screen opens near the top left corner.
        if (!geometry.intersects(m_pClientWindow->screen()->availableVirtualGeometry()))
            geometry.moveTo(20, 20);
        m_pClientWindow->setGeometry(geometry);
    }
    if (options.maximized)
        m_pClientWindow->setWindowState(Qt::WindowMaximized);
    if (options.borderless) {
        m_pClientWindow->setWindowState(Qt::WindowFullScreen);
        if (m_pGameClient)
            m_pGameClient->onFullscreenModeEntered();
    }
    if (m_pGameClient)
        m_pGameClient->setSidebarTriggerCheckBoundsEnabled(true);
}

void TGameApplication::storeClientWindowOptions()
{
    // Host: the browser's window geometry is the page's.
    if (m_clientWindowPlacedByPlatform)
        return;
    config::TClientWindowOptions options = m_pClientOptions->clientWindowOptions();
    const Qt::WindowState state = m_pClientWindow->windowState();
    QRect geometry = m_pClientWindow->geometry();
    if (m_startOptions.startMode != 0)
        geometry.setBottomRight(geometry.topLeft() + options.geometry.bottomRight() - options.geometry.topLeft());
    if (QScreen* screen = m_pClientWindow->screen())
        options.screenName = screen->name();
    switch (state) {
    case Qt::WindowNoState:
        options.geometry = geometry;
        options.maximized = false;
        break;
    case Qt::WindowMaximized:
        options.maximized = true;
        break;
    case Qt::WindowMinimized:
    case Qt::WindowFullScreen:
        return;
    default:
        break;
    }
    disconnect(m_pClientOptions, &config::TClientOptions::clientWindowOptionsChanged, this,
               &TGameApplication::applyClientWindowOptions);
    m_pClientOptions->setClientWindowOptions(options);
    connect(m_pClientOptions, &config::TClientOptions::clientWindowOptionsChanged, this,
            &TGameApplication::applyClientWindowOptions);
}

QScreen* TGameApplication::screenForClientWindowOptions(const config::TClientWindowOptions& Options) const
{
    const QList<QScreen*> screens = m_pClientWindow->screen()->virtualSiblings();
    for (QScreen* screen : screens) {
        if (screen->name() == Options.screenName)
            return screen;
    }
    return nullptr;
}

void TGameApplication::setHasBorderInFullScreen()
{
    using THasBorderInFullScreen = void (*)(QWindow*, bool);
    const auto hasBorderInFullScreen = reinterpret_cast<THasBorderInFullScreen>(
        QGuiApplication::platformFunction(QByteArrayLiteral("WindowsSetHasBorderInFullScreen")));
    if (hasBorderInFullScreen && m_pClientWindow)
        hasBorderInFullScreen(m_pClientWindow, true);
}

void TGameApplication::setUpLogging()
{
    const shared::TTibiaFileSystemHelper fileSystem;
    shared::setUpLogging(fileSystem.getLogDirectory().filePath(QStringLiteral("client.log")), 1024 * 1024);
    shared::setLogMessageCallback(
        [](const QString& message) { shared::TCrashHandler::instance().addLogMessage(message); });
}

void TGameApplication::setUpCrashHandler(const QUrl& loginUrl)
{
    const shared::TTibiaFileSystemHelper fileSystem;
    initializeCrashHandler(fileSystem.getCrashDumpDirectory().absolutePath());
    setCrashReportLoginHost(loginUrl.host());
    setCrashHandlerPassesExceptionOn(false);
}

void TGameApplication::initializeCrashHandler(const QString& dumpPath)
{
    shared::TCrashHandler& crashHandler = shared::TCrashHandler::instance();
    constexpr int clientVersion = TGameClientBase::ClientVersion;
    const QString cipVersion =
        QStringLiteral("%1.%2").arg(clientVersion / 100).arg(clientVersion % 100, 2, 10, QLatin1Char('0'));
    // Host: CipSoft's build revision is its git sha; qtclient's is the host build id.
    const QString version = QStringLiteral("%1.%2").arg(cipVersion, QLatin1StringView(qtclient_host_build_id));
    crashHandler.initialize(QStringLiteral("Tibia Client"), version, dumpPath);
    crashHandler.setAnnotation(QStringLiteral("HardwareID"), hardwareId());
    crashHandler.setAnnotation(QStringLiteral("OperatingSystem"), QSysInfo::prettyProductName());
    // Host: CipSoft annotates its active graphics engine; qtclient's is the renderer option.
    crashHandler.setAnnotation(QStringLiteral("EngineID"), QString::number(app::ClientOptions::load().rendererIndex()));
}

void TGameApplication::setCrashReportLoginHost(const QString& loginHost)
{
    shared::TCrashHandler::instance().setAnnotation(QStringLiteral("LoginHost"), loginHost);
}

void TGameApplication::setCrashHandlerPassesExceptionOn(const bool passExceptionOn)
{
    shared::TCrashHandler::instance().setPassExceptionOn(passExceptionOn);
}

QString TGameApplication::hardwareId()
{
    static QString hardwareId;
    if (!hardwareId.isEmpty())
        return hardwareId;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QStringLiteral("91bb3378-6faa-40dc-8f8b-93b2f1d55e48").toLocal8Bit());
    for (const QByteArray& address : tibia::platform::hardwareAddresses())
        hash.addData(address);
    const QByteArray hex = hash.result().toHex();
    if (hex.isEmpty())
        hardwareId.clear();
    else
        hardwareId = QString::fromLatin1(hex);
    return hardwareId;
}

void TGameApplication::showErrorAsPopupAndCriticalLog(QString ErrorMessage)
{
    qCritical().noquote() << ErrorMessage;
    if (!m_pClientWindow || !m_pClientWindow->isVisible()) {
        m_pendingErrorMessages.append(ErrorMessage);
        return;
    }
    auto* errorDialog = new gamewindow::TMessageDialogController(qmlEngine(), m_pClientWindow->contentItem());
    // Answered, the dialog closes and goes.
    connect(errorDialog, &gamewindow::TMessageDialogController::buttonClicked, errorDialog, [errorDialog] {
        errorDialog->dialog()->close();
        errorDialog->deleteLater();
    });
    errorDialog->setButtons({ new gamewindow::TMessageDialogButton(
        gamewindow::TMessageDialogController::Button::YES, qtTrId("ok"), gamewindow::TMessageDialogButton::RETURN_KEYS) });
    errorDialog->showMessage(qtTrId("error"), ErrorMessage);
}

void TGameApplication::onClientPluginReadyToClose()
{
    m_pPreventCloseEventFilter->setPreventClose(false);
    if (m_pGameClient) {
        disconnect(m_pClientOptions, &config::TClientOptions::failedToSaveClientOptions, m_pGameClient.data(),
                   &TGameClient::showErrorAsPopupAndCriticalLog);
        shared::uninstallQtMessageHandler();
        m_pClientWindow->removeEventFilter(m_pPreventCloseEventFilter);
        storeClientWindowOptions();
        m_pClientWindow->close();
    }
    QCoreApplication::quit();
}

void TGameApplication::onRequestClose()
{
    if (!m_pGameClient)
        return;
    // Host: a relaunch (graphics engine restart, the launcher) quits without
    // asking.
    if (gamewindow::TGameWindowController::processExitAllowed()) {
        onClientPluginReadyToClose();
        return;
    }
    m_pGameClient->exitClient();
}

void TGameApplication::onAboutToQuit()
{
    if (!m_pGameClient)
        return;
    m_pGameClient->deleteLater();
}

void TGameApplication::startClientWindowChangedReactionTimer()
{
    if (!m_clientWindowChangedReactionTimer.isActive())
        m_clientWindowChangedReactionTimer.start();
}

void TGameApplication::screenGeometryChanged()
{
    // A screen change repaints the client window by toggling its opacity.
    if (!m_pClientWindow)
        return;
    m_pClientWindow->setOpacity(0.0);
    m_pClientWindow->setOpacity(1.0);
}

void TGameApplication::onDelayedCliendWindowChanged()
{
    storeClientWindowOptions();
}

void TGameApplication::onNumberOfSidebarsChanged(const std::size_t NewNumber)
{
    // clientwindow.qml's minimum width grows with each sidebar.
    m_pClientWindow->setProperty("numberOfSidebars", QVariant(static_cast<uint>(NewNumber)));
}

void TGameApplication::onLeftAndRightStatusActionBarWidthChanged(const int NewWidth)
{
    m_pClientWindow->setProperty("leftAndRightStatusActionBarWidth", QVariant(NewWidth));
}

void TGameApplication::windowSizeChanged()
{
    if (m_pGameClient)
        m_pGameClient->onWindowSizeChanged();
}

}
