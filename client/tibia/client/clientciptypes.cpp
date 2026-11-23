#include "client/clientciptypes.h"

#include "platform/platform.h"

#include <tibia/game/tgamenotify.h>

#include "app/clientoptions.h"
#include "client/gameclient.h"
#include "client/preventcloseeventfilter.h"
#include "gamewindow/gamewindowcontroller.h"
#include "crashhandler.h"
#include "logging.h"
#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEvent>
#include <QSysInfo>
#include <QVariant>

#include <algorithm>
#include <vector>

extern "C" const char qtclient_host_build_id[];

namespace tibia::client {

struct IGamesessionManager::LiveFans {
    std::vector<QMetaObject::Connection> typed;
};

IGamesessionManager::IGamesessionManager(QObject* parent)
    : cip::tibia::client::IGamesessionManager(parent)
    , m_liveFans(new LiveFans)
{
    bindLiveNotifyFans();
}

IGamesessionManager::~IGamesessionManager()
{
    if (m_liveFans) {
        for (const QMetaObject::Connection& c : m_liveFans->typed)
            QObject::disconnect(c);
        delete m_liveFans;
        m_liveFans = nullptr;
    }
}

void IGamesessionManager::bindLiveNotifyFans()
{
    // Name-only floor: CIP passes shared_ptr<IGameSession>. Tip does not take
    // session ownership or call loginWorld / forceLogout.
    auto* fans = m_liveFans;
    if (!fans)
        return;
    game::TGameNotify* notify = game::TGameNotify::instance();
    fans->typed.push_back(connect(notify, &game::TGameNotify::gameStarted, this, [this] {
        Q_EMIT changePrimaryGameSession();
    }));
    fans->typed.push_back(connect(notify, &game::TGameNotify::gameEnded, this, [this] {
        Q_EMIT changePrimaryGameSession();
    }));
}

IGamesessionManager* IGamesessionManager::instance()
{
    static IGamesessionManager keep;
    return &keep;
}

TGameApplicationBase::TGameApplicationBase(QObject* parent)
    : QObject(parent)
{
}

TGameApplication::TGameApplication(QObject* parent)
    : TGameApplicationBase(parent)
    , m_pPreventCloseEventFilter(new TPreventCloseEventFilter(this))
{
    connect(m_pPreventCloseEventFilter, &TPreventCloseEventFilter::preventedClose, this,
            &TGameApplication::onRequestClose);
}

void TGameApplication::setClientWindow(QWindow* ClientWindow)
{
    if (m_clientWindow) {
        disconnect(m_clientWindow, nullptr, this, nullptr);
        m_clientWindow->removeEventFilter(m_pPreventCloseEventFilter);
    }
    m_clientWindow = ClientWindow;
    if (!m_clientWindow)
        return;
    m_clientWindow->installEventFilter(m_pPreventCloseEventFilter);
    connect(m_clientWindow, &QWindow::widthChanged, this, &TGameApplication::windowSizeChanged);
    connect(m_clientWindow, &QWindow::heightChanged, this, &TGameApplication::windowSizeChanged);
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
    constexpr int clientVersion = TGameClient::ClientVersion;
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
    // CIP slot: critical log + error popup. Tip logs only — opening a blocking
    // dialog from the keep-alive twin can race the login/character shell.
    qCritical().noquote() << ErrorMessage;
}

void TGameApplication::onClientPluginReadyToClose()
{
    m_pPreventCloseEventFilter->setPreventClose(false);
    if (m_pGameClient && m_clientWindow) {
        m_clientWindow->removeEventFilter(m_pPreventCloseEventFilter);
        m_clientWindow->close();
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
}

void TGameApplication::startClientWindowChangedReactionTimer()
{
}

void TGameApplication::screenGeometryChanged()
{
    // A screen change repaints the client window by toggling its opacity.
    if (m_clientWindow) {
        m_clientWindow->setOpacity(0.0);
        m_clientWindow->setOpacity(1.0);
    }
}

void TGameApplication::onDelayedCliendWindowChanged()
{
}

void TGameApplication::onNumberOfSidebarsChanged(const std::size_t NewNumber)
{
    // clientwindow.qml's minimum width grows with each sidebar.
    m_clientWindow->setProperty("numberOfSidebars", QVariant(static_cast<uint>(NewNumber)));
}

void TGameApplication::onLeftAndRightStatusActionBarWidthChanged(const int NewWidth)
{
    m_clientWindow->setProperty("leftAndRightStatusActionBarWidth", QVariant(NewWidth));
}

void TGameApplication::windowSizeChanged()
{
    if (m_pGameClient)
        m_pGameClient->onWindowSizeChanged();
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    // CIP TGameApplication keep-alive under the session/feature parent.
    // Not the process QApplication — moc slot floor only.
    if (parent)
        new TGameApplication(parent);
    else
        new TGameApplication(nullptr);
    // Process-lifetime qtclient twin (linked below). Not GameClient session owner.
    (void)IGamesessionManager::instance();
}

namespace {
const bool kQtclientIGamesessionManagerLinked = (IGamesessionManager::instance() != nullptr);
}

} // namespace tibia::client
