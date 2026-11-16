#include "domainhandlers.h"

#include "app/portalurls.h"
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

constexpr std::array<A, 7> ClipboardActions = {
    A::COPY_TEXT_TO_CLIPBOARD,
    A::COPY_TRUSTED_TEXT_TO_CLIPBOARD,
    A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD,
    A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD,
    A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD,
    A::COPY_CREATURE_NAME_TO_CLIPBOARD,
    A::COPY_CHAT_ENTRY_TO_CLIPBOARD,
};

QObject* focusedTextField()
{
    return QGuiApplication::focusObject();
}

QString focusedSelectedText()
{
    if (QObject* field = focusedTextField())
        return field->property("selectedText").toString();
    return {};
}

void copyFocusedOrActionText(const TGameAction& action)
{
    QString text = action.text;
    if (text.isEmpty())
        text = focusedSelectedText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

bool handleClipboard(const TGameAction& action)
{
    switch (action.type) {
    case A::COPY_TEXT_TO_CLIPBOARD:
    case A::COPY_TRUSTED_TEXT_TO_CLIPBOARD:
        QGuiApplication::clipboard()->setText(action.text);
        return true;
    case A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD:
    case A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD:
    case A::COPY_CHAT_ENTRY_TO_CLIPBOARD:
        copyFocusedOrActionText(action);
        return true;
    case A::COPY_CREATURE_WITH_ID_NAME_TO_CLIPBOARD:
    case A::COPY_CREATURE_NAME_TO_CLIPBOARD:
        if (action.creatureId) {
            if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
                if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(action.creatureId)) {
                    QGuiApplication::clipboard()->setText(row->name());
                    return true;
                }
            }
        }
        if (!action.text.isEmpty())
            QGuiApplication::clipboard()->setText(action.text);
        return true;
    default:
        return false;
    }
}

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
    for (const A type : ClipboardActions)
        setActionHandler(type, handleClipboard);
}

TClipboardGameActionHandler::~TClipboardGameActionHandler()
{
    for (const A type : ClipboardActions)
        setActionHandler(type, nullptr);
}

TGatherAllGameActionHandler::TGatherAllGameActionHandler(QObject* parent)
    : cip::tibia::input::TGatherAllGameActionHandler(parent)
{
    setActionHandler(A::GATHER_ALL, [](const TGameAction& action) {
        const uint16_t typeId = action.objectClientId;
        if (typeId != 0) {
            const uint8_t stackpos = action.stackPosition >= 0 ? static_cast<uint8_t>(action.stackPosition) : 0;
            tibia::game::gatherAll(action.position, typeId, stackpos);
        }
        return true;
    });
}

TGatherAllGameActionHandler::~TGatherAllGameActionHandler()
{
    setActionHandler(A::GATHER_ALL, nullptr);
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
    for (const A type : OpenWebsiteActions) {
        setActionHandler(type, [this](const TGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
}

TOpenWebsiteGameActionHandler::~TOpenWebsiteGameActionHandler()
{
    for (const A type : OpenWebsiteActions)
        setActionHandler(type, nullptr);
}

void TOpenWebsiteGameActionHandler::handleGameAction(const TGameAction& action)
{
    switch (action.type) {
    case A::OPEN_URL_GET_PREMIUM:
        handleOpenGetPremiumUrlGameAction(action);
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
        openUrlLater(configuredUrl(action.type, tibiaComUrl()));
        break;
    case A::OPEN_URL_CIPSOFT:
        // cipSoftUrl is the one page without a tibia.com fallback.
        openUrlLater(configuredUrl(action.type, QUrl()));
        break;
    case A::OPEN_URL_SURVEY:
        handleOpenSurveyUrlGameAction(action);
        break;
    case A::OPEN_URL:
        openUrlLater(QUrl(action.text, QUrl::TolerantMode));
        break;
    default:
        break;
    }
}

void TOpenWebsiteGameActionHandler::handleOpenGetPremiumUrlGameAction(const TGameAction& action)
{
    QString url = configuredUrl(A::OPEN_URL_GET_PREMIUM, tibiaComUrl()).toString();
    url.append(QString::number(action.parameter));
    openUrlLater(QUrl(url, QUrl::TolerantMode));
}

void TOpenWebsiteGameActionHandler::handleOpenSurveyUrlGameAction(const TGameAction& action)
{
    QString url = configuredUrl(A::OPEN_URL_SURVEY, tibiaComUrl()).toString();
    url.append(QString::number(action.parameter));
    url.append(QStringLiteral("/lang-en?token="));
    url.append(action.text);
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
