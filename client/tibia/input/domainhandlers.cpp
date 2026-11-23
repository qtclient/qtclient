#include "domainhandlers.h"

#include <algorithm>

#include "app/portalurls.h"
#include "config/clientoptions.h"
#include "shared/tibiaurltools.h"
#include "gameaction.h"

#include <creatures/creaturestorage.h>
#include <tibia/game/gamesession.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>

#include <array>

namespace tibia::input {

namespace {

using A = EGameAction;

}

TClipboardGameActionHandler::TClipboardGameActionHandler(QObject* parent)
    : cip::tibia::input::TClipboardGameActionHandler(parent)
    // Host: CipSoft receives the game client's helper instances.
    , m_clipboardHelper(std::make_shared<shared::TTibiaClipboardHelper>(std::make_shared<shared::TClipboardHelper>(),
                                                                         std::make_shared<shared::TTibiaUrlTools>()))
{
}

TClipboardGameActionHandler::~TClipboardGameActionHandler() = default;

void TClipboardGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    switch (pGameAction->gameActionType()) {
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
    case A::COPY_TEXT_TO_CLIPBOARD:
        if (auto copy = std::dynamic_pointer_cast<const TGameActionCopyTextToClipboard>(pGameAction))
            handleCopyTextToClipboardGameAction(std::move(copy));
        return;
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
    case A::COPY_TRUSTED_TEXT_TO_CLIPBOARD:
        if (const auto* copy = gameActionAs<TGameActionCopyTextToClipboard>(pGameAction))
            QGuiApplication::clipboard()->setText(copy->text());
        return;
    default:
        return;
    }
}

void TClipboardGameActionHandler::handleCopyTextToClipboardGameAction(
    const std::shared_ptr<const TGameActionCopyTextToClipboard> pGameAction)
{
    const config::TClientOptions* options = config::TClientOptions::instance();
    if (options && options->boolOption(config::EClientOption::linkCopyWarningEnabled)
        && shared::TTibiaUrlTools::containsUrl(pGameAction->text())) {
        Q_EMIT publishGameAction(std::make_shared<TGameActionRequestConformationForCopyingTextToClipboard>(pGameAction->text()));
        return;
    }
    m_clipboardHelper->setTextInClipboard(pGameAction->text());
}

void TClipboardGameActionHandler::handleCopyTrustedTextToClipboardGameAction(
    const std::shared_ptr<const TGameActionCopyTextToClipboard> pGameAction)
{
    m_clipboardHelper->setTextInClipboard(pGameAction->text());
}

TGatherAllGameActionHandler::TGatherAllGameActionHandler(std::shared_ptr<TGatherAllProtocolMessageHandler> protocolMessageHandler,
                                                         QObject* parent)
    : cip::tibia::input::TGatherAllGameActionHandler(parent)
    , m_protocolMessageHandler(std::move(protocolMessageHandler))
{
}

TGatherAllGameActionHandler::~TGatherAllGameActionHandler() = default;

void TGatherAllGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != A::GATHER_ALL)
        return;
    handleGatherAllAction(std::dynamic_pointer_cast<const gameactions::TGameActionGatherAll>(pGameAction));
}

void TGatherAllGameActionHandler::handleGatherAllAction(const std::shared_ptr<const gameactions::TGameActionGatherAll> pAction) const
{
    m_protocolMessageHandler->requestGatherAll(pAction->context());
}

TGatherAllProtocolMessageHandler::TGatherAllProtocolMessageHandler(QObject* parent)
    : cip::tibia::input::TGatherAllProtocolMessageHandler(parent)
{}

void TGatherAllProtocolMessageHandler::requestGatherAll(const TObjectGameActionContext& object)
{
    ::tibia::protobuf::protocol::GameclientMessageGatherAll message;
    // Without a location the object is anywhere the player has it.
    const Position position = object.location() ? object.position() : Position(UINT16_MAX, 0, 0);
    const int stackPosition = object.location() ? object.stackPosition() : 0;
    message.mutable_position()->set_x(position.x);
    message.mutable_position()->set_y(position.y);
    message.mutable_position()->set_z(position.z);
    message.set_object_id(object.objectClientId());
    message.set_stack_position(static_cast<uint32_t>(std::max(stackPosition, 0)));
    Q_EMIT sendGatherAll(message);
}

namespace {

constexpr std::array<A, 14> OpenWebsiteActions = {
    A::OPEN_URL_GET_PREMIUM, A::OPEN_URL_GET_COINS, A::OPEN_URL_TIBIA_PAGE, A::OPEN_URL_CREATE_ACCOUNT,
    A::OPEN_URL_ACCESS_ACCOUNT, A::OPEN_URL_CIPSOFT, A::OPEN_URL_MANUAL, A::OPEN_URL_FAQ, A::OPEN_URL_SURVEY,
    A::OPEN_URL_LOST_ACCOUNT, A::OPEN_URL_PREMIUM_FEATURES, A::OPEN_URL_TWITCH_TIBIA, A::OPEN_URL_YOUTUBE_TIBIA,
    A::OPEN_URL,
};

/// Fallback of every [URLS] page but cipSoftUrl.
const QUrl& tibiaComUrl()
{
    static const QUrl url(QStringLiteral("http://www.tibia.com/"));
    return url;
}

}

TOpenWebsiteGameActionHandler::TOpenWebsiteGameActionHandler(const app::LauncherEnv& env, QObject* parent)
    : cip::tibia::input::TOpenWebsiteGameActionHandler(parent)
    , m_env(env)
{
}

TOpenWebsiteGameActionHandler::~TOpenWebsiteGameActionHandler() = default;

void TOpenWebsiteGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction
        || std::find(OpenWebsiteActions.begin(), OpenWebsiteActions.end(), pGameAction->gameActionType()) == OpenWebsiteActions.end())
        return;
    const EGameAction type = pGameAction->gameActionType();
    switch (type) {
    case A::OPEN_URL_GET_PREMIUM:
        if (const auto* premium = gameActionAs<TGameActionOpenGetPremiumUrl>(pGameAction))
            handleOpenGetPremiumUrlGameAction(*premium);
        break;
    case A::OPEN_URL_GET_COINS:
    case A::OPEN_URL_TIBIA_PAGE:
    case A::OPEN_URL_CREATE_ACCOUNT:
    case A::OPEN_URL_ACCESS_ACCOUNT:
    case A::OPEN_URL_MANUAL:
    case A::OPEN_URL_FAQ:
    case A::OPEN_URL_LOST_ACCOUNT:
    case A::OPEN_URL_PREMIUM_FEATURES:
    case A::OPEN_URL_TWITCH_TIBIA:
    case A::OPEN_URL_YOUTUBE_TIBIA:
        openUrlLater(configuredUrl(type, tibiaComUrl()));
        break;
    case A::OPEN_URL_CIPSOFT:
        // cipSoftUrl is the one page without a tibia.com fallback.
        openUrlLater(configuredUrl(type, QUrl()));
        break;
    case A::OPEN_URL_SURVEY:
        if (const auto* survey = gameActionAs<TGameActionOpenSurveyUrl>(pGameAction))
            handleOpenSurveyUrlGameAction(*survey);
        break;
    case A::OPEN_URL:
        if (const auto* url = gameActionAs<TGameActionOpenUrl>(pGameAction))
            openUrlLater(QUrl(url->url(), QUrl::TolerantMode));
        break;
    default:
        break;
    }
}

void TOpenWebsiteGameActionHandler::handleOpenGetPremiumUrlGameAction(const TGameActionOpenGetPremiumUrl& action)
{
    QString url = configuredUrl(A::OPEN_URL_GET_PREMIUM, tibiaComUrl()).toString();
    url.append(QString::number(action.source()));
    openUrlLater(QUrl(url, QUrl::TolerantMode));
}

void TOpenWebsiteGameActionHandler::handleOpenSurveyUrlGameAction(const TGameActionOpenSurveyUrl& action)
{
    QString url = configuredUrl(A::OPEN_URL_SURVEY, tibiaComUrl()).toString();
    url.append(QString::number(action.surveyId()));
    url.append(QStringLiteral("/lang-en?token="));
    url.append(action.token());
    openUrlLater(QUrl(url, QUrl::TolerantMode));
}

QUrl TOpenWebsiteGameActionHandler::configuredUrl(const EGameAction action, const QUrl& defaultUrl) const
{
    // Host concern: app::PortalUrls is qtclient's [URLS] group.
    const QUrl url = app::PortalUrls::urlFor(m_env, action);
    return url.isEmpty() ? defaultUrl : url;
}

void TOpenWebsiteGameActionHandler::openUrlLater(const QUrl& url)
{
    QTimer::singleShot(0, [url] { QDesktopServices::openUrl(url); });
}

}
