#pragma once

#include <QUrl>

// CIP-named client surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWindow>

#include <cstddef>

namespace tibia {
namespace client {

class TGameClient;
class TPreventCloseEventFilter;

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// CIP `client::IGamesessionManager` moc surface. Signal
/// `changePrimaryGameSession(shared_ptr<IGameSession>)` is cip_skipped — qtclient
/// redeclares name-only floor. Does **not** own login / swap GameClient
/// sessions; fans `TGameNotify` game start/end only.
class IGamesessionManager : public cip::tibia::client::IGamesessionManager
{
    Q_OBJECT
public:
    explicit IGamesessionManager(QObject* parent = nullptr);
    ~IGamesessionManager() override;

    /// Process-lifetime qtclient twin (moc/RTTI keep). Not GameClient's live session owner.
    static IGamesessionManager* instance();

Q_SIGNALS:
    /// CIP moc signal (shared_ptr<IGameSession> skipped from cip_gen); qtclient name parity.
    void changePrimaryGameSession();

private:
    void bindLiveNotifyFans();

    struct LiveFans;
    LiveFans* m_liveFans = nullptr;
};

class TAntiCheatController final : public cip::tibia::client::TAntiCheatController
{
    Q_OBJECT
public:
    using cip::tibia::client::TAntiCheatController::TAntiCheatController;
};

/// CIP chain is QApplication; qtclient must not construct a second QApplication
/// (login/host already owns one). Moc surface lives on QObject — same pattern
/// as TAuthenticationProcessController vs QStateMachine.
class TGameApplicationBase : public QObject
{
    Q_OBJECT
public:
    explicit TGameApplicationBase(QObject* parent = nullptr);
};

/// CIP `client::TGameApplication` app-shell slots. Keep-alive under
/// ownProductSurface; does not replace the process QApplication or GameClient
/// fail/close paths. `showErrorAsPopupAndCriticalLog` logs only — popup UI
/// stays on the product/login shell.
class TGameApplication final : public TGameApplicationBase
{
    Q_OBJECT
public:
    explicit TGameApplication(QObject* parent = nullptr);
    /// The game client the application slots reach (CipSoft's application
    /// creates it; qtclient's client owns its application).
    void setGameClient(TGameClient* GameClient) { m_pGameClient = GameClient; }
    /// The client window (clientwindow.qml) the window slots act on; its size
    /// changes reach windowSizeChanged, and its close events are held back
    /// for the client's exit confirmation (onRequestClose).
    void setClientWindow(QWindow* ClientWindow);

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
    void showErrorAsPopupAndCriticalLog(QString ErrorMessage);
    /// CIP: the client is ready to close: close events pass again, the client
    /// window closes and the application quits.
    void onClientPluginReadyToClose();
    /// CIP: the client window's close was held back: the client asks for
    /// confirmation.
    void onRequestClose();
    void onAboutToQuit();
    void startClientWindowChangedReactionTimer();
    void screenGeometryChanged();
    void onDelayedCliendWindowChanged();
    void onNumberOfSidebarsChanged(std::size_t NewNumber);
    void onLeftAndRightStatusActionBarWidthChanged(int NewWidth);
    void windowSizeChanged();

private:
    TGameClient* m_pGameClient = nullptr;
    QPointer<QWindow> m_clientWindow;
    /// Holds back the client window's close events (child of the application).
    TPreventCloseEventFilter* m_pPreventCloseEventFilter = nullptr;
};

}
}
