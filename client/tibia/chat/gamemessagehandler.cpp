#include "gamemessagehandler.h"

#include "chat/chatchannel.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/textstorage.h"
#include "config/clientoptiondefaults.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::chat {

namespace {

using client::gamecore::argString;
using Args = std::vector<std::any>;

/// CIP's chat message modes: the protocol's (identity table).
enum Mode : int
{
    Failure = 21,
    Status = 30,
};

/// Hook arguments carry CIP's chat message mode (the protocol's).
int protocolMode(const Args& args, const size_t index)
{
    return index < args.size() ? static_cast<int>(tibia::game::argInt(args, index)) : -1;
}

/// Missing key → CIP default ; gameWindow* filters default to 1.
bool option(const QJsonObject& options, const char* name)
{
    const QLatin1String key(name);
    return options.value(key).toBool(config::defaultClientOptions().value(key).toBool());
}

}

TGameMessageHandler::TGameMessageHandler(TTextStorage& texts)
    : m_texts(texts)
{
    // IdleOT host extension: packs post their own status lines (CIP posts SET_STATUS_LINE).
    m_hooks.emplace_back("onTextMessage", [this](const Args& args) {
        onTextMessage(protocolMode(args, 0), argString(args, 1));
        return std::any();
    });
}

TGameMessageHandler::~TGameMessageHandler()
{
}

void TGameMessageHandler::setOptions(const QJsonObject& options)
{
    m_showMessages = option(options, "gameWindowShowMessages");
}

void TGameMessageHandler::onTextMessage(const int mode, const QString& text)
{
    if ((mode == Status || mode == Failure) && m_showMessages)
        m_texts.setStatusMessage(text, overlayColor(mode));
}

}
