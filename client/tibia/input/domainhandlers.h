#pragma once

#include "app/launcherenv.h"
#include "gameactionhandlers.h"

#include <cip_gen.h>

#include <QUrl>

#include <functional>

namespace tibia {
namespace input {

/// CIP input::TActionButtonGameActionHandler : owns the
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

public Q_SLOTS:
    /// CIP handleGameAction: acts on the actions of its types; connected to the
    /// router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
};

/// CIP input::TGatherAllGameActionHandler: GATHER_ALL → tibia::game::gatherAll.
class TGatherAllGameActionHandler final : public cip::tibia::input::TGatherAllGameActionHandler
{
    Q_OBJECT
public:
    explicit TGatherAllGameActionHandler(QObject* parent = nullptr);
    ~TGatherAllGameActionHandler() override;
    TGatherAllGameActionHandler(const TGatherAllGameActionHandler&) = delete;
    TGatherAllGameActionHandler& operator=(const TGatherAllGameActionHandler&) = delete;

public Q_SLOTS:
    /// CIP handleGameAction: acts on the actions of its types; connected to the
    /// router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
};

/// CIP input::TGatherAllProtocolMessageHandler: ClientGatherAll (0xFF) send
/// path used by TGatherAllGameActionHandler / tibia::game::gatherAll.
class TGatherAllProtocolMessageHandler final : public cip::tibia::input::TGatherAllProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TGatherAllProtocolMessageHandler(QObject* parent = nullptr);
};

/// CIP input::TOpenWebsiteGameActionHandler: opens the website page of an
/// OPEN_URL_* action in the system browser. CipSoft reads each page from the
/// [URLS] group of the client settings (tibiaPageUrl, getPremiumUrl, ...), falling
/// back to http://www.tibia.com/. Host concern: qtclient has no [URLS] blob, so
/// app::PortalUrls (portal paths + QTCLIENT_URL_* overrides) stands in for it.
class TOpenWebsiteGameActionHandler final : public cip::tibia::input::TOpenWebsiteGameActionHandler
{
    Q_OBJECT
public:
    explicit TOpenWebsiteGameActionHandler(const app::LauncherEnv& env, QObject* parent = nullptr);
    ~TOpenWebsiteGameActionHandler() override;
    TOpenWebsiteGameActionHandler(const TOpenWebsiteGameActionHandler&) = delete;
    TOpenWebsiteGameActionHandler& operator=(const TOpenWebsiteGameActionHandler&) = delete;

public Q_SLOTS:
    /// Opens the page of an OPEN_URL_* action; connected to the router's
    /// publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);

public:

    /// OPEN_URL_GET_PREMIUM: getPremiumUrl followed by the action's number (`parameter`).
    void handleOpenGetPremiumUrlGameAction(const TGameAction& action);
    /// OPEN_URL_SURVEY: limesurveyUrl, the survey id (`parameter`), then
    /// "/lang-en?token=" and the invitation token (`text`).
    void handleOpenSurveyUrlGameAction(const TGameAction& action);

private:
    /// The [URLS] page for `action`, or `defaultUrl` when none is configured.
    QUrl configuredUrl(EGameAction action, const QUrl& defaultUrl) const;
    /// Opens `url` from the event loop, after the action finished.
    static void openUrlLater(const QUrl& url);

    app::LauncherEnv m_env;
};

}
}
