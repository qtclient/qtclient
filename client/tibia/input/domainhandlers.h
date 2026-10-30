#pragma once

#include "app/launcherenv.h"
#include "gameactionhandlers.h"

#include <cip_gen.h>

#include <QUrl>

#include <functional>

namespace tibia::input {

/// CIP input::TActionButtonGameActionHandler (FUN_100e83f40): owns the
/// ACTION_BUTTON_TRIGGERED registration for the action-bar lifetime.
class TActionButtonGameActionHandler final : public cip::tibia::input::TActionButtonGameActionHandler
{
    Q_OBJECT
public:
    explicit TActionButtonGameActionHandler(ActionHandler handler, QObject* parent = nullptr);
    ~TActionButtonGameActionHandler() override;
    TActionButtonGameActionHandler(const TActionButtonGameActionHandler&) = delete;
    TActionButtonGameActionHandler& operator=(const TActionButtonGameActionHandler&) = delete;
};

/// CIP input::TClipboardGameActionHandler: COPY_* / REQUEST_COPY_* /
/// REQUEST_CONFIRM clipboard actions.
class TClipboardGameActionHandler final : public cip::tibia::input::TClipboardGameActionHandler
{
    Q_OBJECT
public:
    explicit TClipboardGameActionHandler(QObject* parent = nullptr);
    ~TClipboardGameActionHandler() override;
    TClipboardGameActionHandler(const TClipboardGameActionHandler&) = delete;
    TClipboardGameActionHandler& operator=(const TClipboardGameActionHandler&) = delete;
};

/// CIP input::TGenericGameActionHandler: fullscreen / sound / HUD / FPS /
/// hunting-session toggles (cip slots handleToggle*).
class TGenericGameActionHandler final : public cip::tibia::input::TGenericGameActionHandler
{
    Q_OBJECT
public:
    explicit TGenericGameActionHandler(QObject* parent = nullptr);
    ~TGenericGameActionHandler() override;
    TGenericGameActionHandler(const TGenericGameActionHandler&) = delete;
    TGenericGameActionHandler& operator=(const TGenericGameActionHandler&) = delete;

public Q_SLOTS:
    void handleToggleFullscreenAction() override;
    void handleResetHuntingSessionAction() override;
    void handleToggleCreatureHudsVisibleGameAction() override;
    void handleToggleSoundGameAction() override;
    void handleToggleFpsLagIndecatorGameAction() override;
};

/// CIP input::TGatherAllGameActionHandler: GATHER_ALL → Game::gatherAll.
class TGatherAllGameActionHandler final : public cip::tibia::input::TGatherAllGameActionHandler
{
    Q_OBJECT
public:
    explicit TGatherAllGameActionHandler(QObject* parent = nullptr);
    ~TGatherAllGameActionHandler() override;
    TGatherAllGameActionHandler(const TGatherAllGameActionHandler&) = delete;
    TGatherAllGameActionHandler& operator=(const TGatherAllGameActionHandler&) = delete;
};

/// CIP input::TGatherAllProtocolMessageHandler: ClientGatherAll (0xFF) send
/// path used by TGatherAllGameActionHandler / Game::gatherAll.
class TGatherAllProtocolMessageHandler final : public cip::tibia::input::TGatherAllProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TGatherAllProtocolMessageHandler(QObject* parent = nullptr);
};

/// CIP input::TOpenWebsiteGameActionHandler: OPEN_URL_* → portal / absolute URL
/// (product maps CIP tibia.com URLs via app::PortalUrls + LauncherEnv overrides).
class TOpenWebsiteGameActionHandler final : public cip::tibia::input::TOpenWebsiteGameActionHandler
{
    Q_OBJECT
public:
    explicit TOpenWebsiteGameActionHandler(const app::LauncherEnv& env, QObject* parent = nullptr);
    explicit TOpenWebsiteGameActionHandler(const QUrl& loginBase, QObject* parent = nullptr);
    ~TOpenWebsiteGameActionHandler() override;
    TOpenWebsiteGameActionHandler(const TOpenWebsiteGameActionHandler&) = delete;
    TOpenWebsiteGameActionHandler& operator=(const TOpenWebsiteGameActionHandler&) = delete;

private:
    app::LauncherEnv m_env;
};

}
