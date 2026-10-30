#include "domainhandlers.h"

#include "app/clientoptions.h"
#include "app/portalurls.h"
#include "gameaction.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/map.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonObject>
#include <QMetaObject>
#include <QQuickWindow>
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

constexpr std::array<A, 2> GenericActions = {
    A::TOGGLE_SOUND,
    A::TOGGLE_CREATURE_HUDS_VISIBLE,
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

void toggleBoolOption(const QLatin1String name, const bool fallback)
{
    const bool current = app::ClientOptions::load().options().value(name).toBool(fallback);
    app::ClientOptions::store(QString(name), !current);
}

void toggleSound()
{
    const QJsonObject options = app::ClientOptions::load().options();
    const int master = options.value(QLatin1String("soundMasterVolume")).toInt(100);
    if (master > 0) {
        app::ClientOptions::store(QStringLiteral("soundMasterVolumeOld"), master);
        app::ClientOptions::store(QStringLiteral("soundMasterVolume"), 0);
    } else {
        const int restored = options.value(QLatin1String("soundMasterVolumeOld")).toInt(100);
        app::ClientOptions::store(QStringLiteral("soundMasterVolume"), restored > 0 ? restored : 100);
    }
}

CreaturePtr creatureOf(const TGameAction& action)
{
    return action.creatureId ? g_map.getCreatureById(action.creatureId) : nullptr;
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
        if (const CreaturePtr creature = creatureOf(action))
            QGuiApplication::clipboard()->setText(QString::fromStdString(creature->getName()));
        else if (!action.text.isEmpty())
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

TGenericGameActionHandler::TGenericGameActionHandler(QObject* parent)
    : cip::tibia::input::TGenericGameActionHandler(parent)
{
    setActionHandler(A::TOGGLE_SOUND, [](const TGameAction&) {
        toggleSound();
        return true;
    });
    setActionHandler(A::TOGGLE_CREATURE_HUDS_VISIBLE, [](const TGameAction&) {
        toggleBoolOption(QLatin1String("creatureHudEnabled"), true);
        return true;
    });
}

TGenericGameActionHandler::~TGenericGameActionHandler()
{
    for (const A type : GenericActions)
        setActionHandler(type, nullptr);
}

void TGenericGameActionHandler::handleToggleFullscreenAction()
{
    publish(makeAction(A::TOGGLE_FULLSCREEN));
}

void TGenericGameActionHandler::handleResetHuntingSessionAction()
{
    publish(makeAction(A::RESET_HUNTING_SESSION));
}

void TGenericGameActionHandler::handleToggleCreatureHudsVisibleGameAction()
{
    publish(makeAction(A::TOGGLE_CREATURE_HUDS_VISIBLE));
}

void TGenericGameActionHandler::handleToggleSoundGameAction()
{
    publish(makeAction(A::TOGGLE_SOUND));
}

void TGenericGameActionHandler::handleToggleFpsLagIndecatorGameAction()
{
    publish(makeAction(A::TOGGLE_FPS_LATENCY_INDICATOR));
}

TGatherAllGameActionHandler::TGatherAllGameActionHandler(QObject* parent)
    : cip::tibia::input::TGatherAllGameActionHandler(parent)
{
    setActionHandler(A::GATHER_ALL, [](const TGameAction& action) {
        if (action.object && action.object->isItem()) {
            const ItemPtr item = action.object->static_self_cast<Item>();
            const uint8_t stackpos = action.stackPosition >= 0
                ? static_cast<uint8_t>(action.stackPosition)
                : static_cast<uint8_t>(item->getStackPos());
            g_game.gatherAll(action.position, item->getId(), stackpos);
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

constexpr std::array<A, 13> OpenWebsiteActions = {
    A::OPEN_URL_GET_PREMIUM, A::OPEN_URL_GET_COINS, A::OPEN_URL_TIBIA_PAGE, A::OPEN_URL_CREATE_ACCOUNT,
    A::OPEN_URL_ACCESS_ACCOUNT, A::OPEN_URL_CIPSOFT, A::OPEN_URL_MANUAL, A::OPEN_URL_FAQ, A::OPEN_URL_SURVEY,
    A::OPEN_URL_LOST_ACCOUNT, A::OPEN_URL_PREMIUM_FEATURES, A::OPEN_URL_TWITCH_TIBIA, A::OPEN_URL_YOUTUBE_TIBIA,
};

}

TOpenWebsiteGameActionHandler::TOpenWebsiteGameActionHandler(const QUrl& loginBase, QObject* parent)
    : cip::tibia::input::TOpenWebsiteGameActionHandler(parent)
    , m_loginBase(loginBase)
{
    const auto openNamed = [this](const TGameAction& action) {
        app::PortalUrls::open(app::PortalUrls::urlFor(m_loginBase, action.type));
        return true;
    };
    for (const A type : OpenWebsiteActions)
        setActionHandler(type, openNamed);
    setActionHandler(A::OPEN_URL, [this](const TGameAction& action) {
        QUrl url = action.text.isEmpty() ? m_loginBase : QUrl(action.text);
        if (!url.isValid() || url.scheme().isEmpty())
            url = app::PortalUrls::urlFor(m_loginBase, A::OPEN_URL_TIBIA_PAGE);
        app::PortalUrls::open(url);
        return true;
    });
}

TOpenWebsiteGameActionHandler::~TOpenWebsiteGameActionHandler()
{
    for (const A type : OpenWebsiteActions)
        setActionHandler(type, nullptr);
    setActionHandler(A::OPEN_URL, nullptr);
}

}
