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

TActionButtonGameActionHandler::TActionButtonGameActionHandler(ActionHandler handler, QObject* parent)
    : cip::tibia::input::TActionButtonGameActionHandler(parent)
{
    setActionHandler(A::ACTION_BUTTON_TRIGGERED, std::move(handler));
}

TActionButtonGameActionHandler::~TActionButtonGameActionHandler()
{
    setActionHandler(A::ACTION_BUTTON_TRIGGERED, nullptr);
}

TClipboardGameActionHandler::TClipboardGameActionHandler(QObject* parent)
    : cip::tibia::input::TClipboardGameActionHandler(parent)
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
    QGuiApplication::clipboard()->setText(pGameAction->text());
}

void TClipboardGameActionHandler::handleCopyTrustedTextToClipboardGameAction(
    const std::shared_ptr<const TGameActionCopyTextToClipboard> pGameAction)
{
    QGuiApplication::clipboard()->setText(pGameAction->text());
}

TGatherAllGameActionHandler::TGatherAllGameActionHandler(QObject* parent)
    : cip::tibia::input::TGatherAllGameActionHandler(parent)
{
}

TGatherAllGameActionHandler::~TGatherAllGameActionHandler() = default;

void TGatherAllGameActionHandler::handleGameAction(const tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != A::GATHER_ALL)
        return;
    const std::optional<TObjectGameActionContext> object = objectContextOf(*pGameAction);
    if (object && object->objectClientId != 0) {
        const uint8_t stackpos = object->stackPosition >= 0 ? static_cast<uint8_t>(object->stackPosition) : 0;
        tibia::game::gatherAll(object->position, object->objectClientId, stackpos);
    }
}

TGatherAllProtocolMessageHandler::TGatherAllProtocolMessageHandler(QObject* parent)
    : cip::tibia::input::TGatherAllProtocolMessageHandler(parent)
{}

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
