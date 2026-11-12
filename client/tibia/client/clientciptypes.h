#pragma once

// CIP-named client surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstddef>

namespace tibia {
namespace client {

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

public Q_SLOTS:
    void showErrorAsPopupAndCriticalLog(QString ErrorMessage);
    void onClientPluginReadyToClose();
    void onRequestClose();
    void onAboutToQuit();
    void startClientWindowChangedReactionTimer();
    void screenGeometryChanged();
    void onDelayedCliendWindowChanged();
    void onNumberOfSidebarsChanged(std::size_t NewNumber);
    void onLeftAndRightStatusActionBarWidthChanged(int NewWidth);
    void windowSizeChanged();
};

class TGameClientBase final : public cip::tibia::client::TGameClientBase
{
    Q_OBJECT
public:
    using cip::tibia::client::TGameClientBase::TGameClientBase;
};

class TPreventCloseEventFilter final : public cip::tibia::client::TPreventCloseEventFilter
{
    Q_OBJECT
public:
    using cip::tibia::client::TPreventCloseEventFilter::TPreventCloseEventFilter;

    bool eventFilter(QObject* watched, QEvent* event) override;
};

}
}
