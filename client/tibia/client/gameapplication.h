#pragma once

#include <QGuiApplication>
#include <QList>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <cstddef>
#include <functional>
#include <memory>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;
class QScreen;

namespace tibia {
namespace config {
class TClientOptions;
struct TClientWindowOptions;
}
namespace shared {
class TTibiaFileSystemHelper;
}
}

namespace tibia {
namespace client {

class TGameClient;
class TPreventCloseEventFilter;

/// CIP client::TGameApplicationBase: the process's application with what the
/// client shares for the whole run: the QML engine, the file system helper
/// and the client options.
///
/// Host: CipSoft's base is a QApplication; qtclient's tip has no Qt Widgets,
/// so it is a QGuiApplication. The process setup CipSoft runs in its
/// initialization (Quick Controls style, graphics API, render loop, scene
/// graph atlas, surface format) runs in app::configureProcess before the
/// application exists, the QML engine is app::createEngine's (handed over
/// with setQmlEngine), the client settings are the process's QSettings and
/// the translations are app::installTranslations.
class TGameApplicationBase : public QGuiApplication
{
    Q_OBJECT
public:
    /// CIP: the application on the command line, with the file system helper;
    /// Direct3D's vblank thread is turned off unless the environment says
    /// otherwise. The client options come with TGameApplication::initialize.
    TGameApplicationBase(int& argc, char** argv);
    ~TGameApplicationBase() override;

    /// Host: the QML engine app::createEngine made; the application owns it.
    void setQmlEngine(std::unique_ptr<QQmlEngine> Engine);
    QQmlEngine* qmlEngine() const { return m_pEngine.get(); }
    std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper() const { return m_pTibiaFileSystemHelper; }
    /// Host: qtclient's client options are the process-wide config object.
    config::TClientOptions* clientOptions() const { return m_pClientOptions; }

protected:
    std::unique_ptr<QQmlEngine> m_pEngine;
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pTibiaFileSystemHelper;
    config::TClientOptions* m_pClientOptions = nullptr;
};

/// CIP client::TGameApplication: the application that shows the client
/// window (clientwindow.qml), makes the game client in it and closes the
/// process when the client is ready to close. It keeps the client window's
/// screen and geometry in the client options, holds back the window's close
/// events for the client's exit confirmation and shows the errors the client
/// and the client options report.
class TGameApplication final : public TGameApplicationBase
{
    Q_OBJECT
public:
    TGameApplication(int& argc, char** argv);
    ~TGameApplication() override;

    /// CIP: what the application reads from its command line: the launcher
    /// found the package up to date. Host: the BattlEye option is not read
    /// (BattlEye is out of scope).
    struct TStartOptions
    {
        int startMode = 0;
        bool packageIsUpToDate = false;
    };

    /// CIP: after the base setup: whether the client options file is missing
    /// (the first start), the crash handler, the client window's change
    /// timer, the application's own connections (quit, the held back close,
    /// the client options' save errors), the client options saved once, and
    /// the command line.
    void initialize(const QUrl& LoginUrl);
    /// CIP: the default locale and clientwindow.qml in the QML engine; the
    /// window's content item is the `clientRootQmlObject` context property,
    /// its colour the client's dark blue; then the window is connected and
    /// the game client is made in it. Host: `HostWindowSetup` runs qtclient's
    /// own window steps (frame timing, platform window, splash) between the
    /// window and the client and may stop the start (false); `StartClient`
    /// starts the made client in the window's placeholder (qtclient's client
    /// builds its game window there; false stops). CipSoft's File menu needs
    /// Qt Widgets and is not made.
    using THostWindowSetup = std::function<bool(QQuickWindow* ClientWindow)>;
    using TStartClient =
        std::function<bool(TGameClient& GameClient, QQuickWindow* ClientWindow, QQuickItem* Placeholder)>;
    bool showClientWindow(const THostWindowSetup& HostWindowSetup, const TStartClient& StartClient);

    QQuickWindow* clientWindow() const { return m_pClientWindow; }
    TGameClient* gameClient() const;
    bool isFirstStart() const { return m_firstStart; }

    /// CIP: the process log `client.log` in the log directory (1 MiB, one
    /// backup); every message is also kept for crash reports.
    static void setUpLogging();
    /// CIP: installs the crash handler into the crashdump directory,
    /// annotates the login host and lets crashes end the process normally.
    static void setUpCrashHandler(const QUrl& loginUrl);
    /// CIP: the crash handler for "Tibia Client" <version>.<build>, writing
    /// into `dumpPath`, annotated with the hardware id, operating system and
    /// graphics engine.
    static void initializeCrashHandler(const QString& dumpPath);
    static void setCrashReportLoginHost(const QString& loginHost);
    static void setCrashHandlerPassesExceptionOn(bool passExceptionOn);
    /// CIP: the SHA-1 of a fixed salt and the network interfaces' hardware
    /// addresses (by interface index), hex; computed once.
    static QString hardwareId();

public Q_SLOTS:
    /// CIP: the error goes to the critical log; with the client window
    /// visible it shows in an error dialog (one OK button, the dialog goes
    /// once answered), else it waits until the client is made.
    void showErrorAsPopupAndCriticalLog(QString ErrorMessage);
    /// CIP: the client is ready to close: close events pass again; with a
    /// client the save errors no longer reach it, Qt's messages leave the
    /// process log, the window's geometry is stored and the window closes;
    /// then the application quits.
    void onClientPluginReadyToClose();
    /// CIP: the client window's close was held back: the client asks for
    /// confirmation.
    void onRequestClose();
    /// CIP: the client goes once the event loop ended.
    void onAboutToQuit();
    /// CIP: a move, resize, state or screen change of the client window
    /// stores the window's geometry once the changes settled.
    void startClientWindowChangedReactionTimer();
    /// CIP: a change of the screens' layout repaints the client window.
    void screenGeometryChanged();
    void onDelayedCliendWindowChanged();
    void onNumberOfSidebarsChanged(std::size_t NewNumber);
    void onLeftAndRightStatusActionBarWidthChanged(int NewWidth);
    void windowSizeChanged();

private:
    /// CIP: the package-is-up-to-date flag from the command line.
    static TStartOptions readStartOptions();
    /// CIP: the window follows the client options and their changes, the
    /// window's changes start the change timer, its size reaches the client,
    /// its close events are held back; then the game client is made.
    bool connectClientWindow(int StartMode, const TStartClient& StartClient);
    /// CIP: the game client with the QML engine, the file system helper and
    /// the client options; its errors, its readiness to close, its sidebar
    /// count and status bar width reach the application; the window comes to
    /// the front, the client starts and the errors from before are shown.
    bool createGameClient(int StartMode, const TStartClient& StartClient);
    /// CIP: the client window takes the screen, geometry, maximized and
    /// borderless (full screen) state of the client options.
    void applyClientWindowOptions();
    /// CIP: the client options take the client window's screen, geometry and
    /// maximized state (a minimized or full screen window keeps the stored
    /// ones).
    void storeClientWindowOptions();
    /// CIP: the screen beside the window's screen with the stored name.
    QScreen* screenForClientWindowOptions(const config::TClientWindowOptions& Options) const;
    /// CIP: on Windows a full screen client window keeps its border.
    void setHasBorderInFullScreen();

    /// Host: guarded, the application deletes a client still there when it goes.
    QPointer<TGameClient> m_pGameClient;
    QQuickWindow* m_pClientWindow = nullptr;
    QQuickItem* m_pPlaceholder = nullptr;
    /// Holds back the client window's close events (child of the application).
    TPreventCloseEventFilter* m_pPreventCloseEventFilter = nullptr;
    TStartOptions m_startOptions;
    /// Errors reported before the client window was visible.
    QList<QString> m_pendingErrorMessages;
    QTimer m_clientWindowChangedReactionTimer;
    bool m_firstStart = false;
    /// Host: the browser's window fills the page; its geometry is not the
    /// client options'.
    bool m_clientWindowPlacedByPlatform = false;
};

}
}
