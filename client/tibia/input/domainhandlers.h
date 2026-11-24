#pragma once

#include "app/launcherenv.h"
#include "gameactionhandlers.h"
#include "shared/tibiaclipboardhelper.h"

#include <cip_gen.h>
#include <protocol.pb.h>

#include <QUrl>

#include <functional>
#include <memory>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace shared {
class TTibiaUrlTools;
}
namespace protocol {
class TGameActionContextHelper;
}
namespace input {

/// CIP input::TClipboardGameActionHandler: the copy actions. Text and chat
/// entries ask first (REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD) when
/// they hold a link and the link copy warning is on; creature names and the
/// client's own texts are copied at once.
class TClipboardGameActionHandler final : public cip::tibia::input::TClipboardGameActionHandler
{
    Q_OBJECT
public:
    /// The game's clipboard writer, the client options (the link copy warning)
    /// and the URL tools (whether a text holds a link).
    TClipboardGameActionHandler(std::shared_ptr<shared::TTibiaClipboardHelper> TibiaClipboardHelper,
                                std::shared_ptr<config::TClientOptions> ClientOptions,
                                std::shared_ptr<shared::TTibiaUrlTools> TibiaUrlTools, QObject* parent = nullptr);
    ~TClipboardGameActionHandler() override;
    TClipboardGameActionHandler(const TClipboardGameActionHandler&) = delete;
    TClipboardGameActionHandler& operator=(const TClipboardGameActionHandler&) = delete;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public Q_SLOTS:
    /// CIP handleGameAction: COPY_CHAT_ENTRY / COPY_TEXT go through
    /// handleCopyTextToClipboardGameAction, COPY_CREATURE_NAME / COPY_TRUSTED_TEXT
    /// to the clipboard; connected to the router's publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
    void handleCopyTextToClipboardGameAction(std::shared_ptr<const tibia::input::TGameActionCopyTextToClipboard> pGameAction);
    void handleCopyTrustedTextToClipboardGameAction(std::shared_ptr<const tibia::input::TGameActionCopyTextToClipboard> pGameAction);

private:
    std::shared_ptr<shared::TTibiaClipboardHelper> m_pTibiaClipboardHelper;
    std::shared_ptr<config::TClientOptions> m_pClientOptions;
    std::shared_ptr<shared::TTibiaUrlTools> m_pTibiaUrlTools;
};

/// CIP input::TGatherAllProtocolMessageHandler: sends the gather all request
/// (ClientGatherAll) of an object.
class TGatherAllProtocolMessageHandler final : public cip::tibia::input::TGatherAllProtocolMessageHandler
{
    Q_OBJECT
public:
    /// The converter that fills the message's object position and identifier.
    explicit TGatherAllProtocolMessageHandler(std::shared_ptr<protocol::TGameActionContextHelper> GameActionContextHelper,
                                              QObject* parent = nullptr);
    ~TGatherAllProtocolMessageHandler() override;

    /// v12: the object's location (position and stack position) and type.
    void requestGatherAll(const TObjectGameActionContext& object);

Q_SIGNALS:
    void sendGatherAll(const ::tibia::protobuf::protocol::GameclientMessageGatherAll& Message);

private:
    std::shared_ptr<protocol::TGameActionContextHelper> m_pGameActionContextHelper;
};

/// CIP input::TGatherAllGameActionHandler: GATHER_ALL goes to the gather all
/// protocol message handler.
class TGatherAllGameActionHandler final : public cip::tibia::input::TGatherAllGameActionHandler
{
    Q_OBJECT
public:
    explicit TGatherAllGameActionHandler(std::shared_ptr<TGatherAllProtocolMessageHandler> protocolMessageHandler,
                                         QObject* parent = nullptr);
    ~TGatherAllGameActionHandler() override;
    TGatherAllGameActionHandler(const TGatherAllGameActionHandler&) = delete;
    TGatherAllGameActionHandler& operator=(const TGatherAllGameActionHandler&) = delete;

public Q_SLOTS:
    /// CIP handleGameAction: acts on GATHER_ALL; connected to the router's
    /// publishGameActionInternal.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
    void handleGatherAllAction(std::shared_ptr<const tibia::input::gameactions::TGameActionGatherAll> pAction) const;

private:
    std::shared_ptr<TGatherAllProtocolMessageHandler> m_protocolMessageHandler;
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
    void handleOpenGetPremiumUrlGameAction(const TGameActionOpenGetPremiumUrl& action);
    /// OPEN_URL_SURVEY: limesurveyUrl, the survey id (`parameter`), then
    /// "/lang-en?token=" and the invitation token (`text`).
    void handleOpenSurveyUrlGameAction(const TGameActionOpenSurveyUrl& action);

private:
    /// The [URLS] page for `action`, or `defaultUrl` when none is configured.
    QUrl configuredUrl(EGameAction action, const QUrl& defaultUrl) const;
    /// Opens `url` from the event loop, after the action finished.
    static void openUrlLater(const QUrl& url);

    app::LauncherEnv m_env;
};

}
}
