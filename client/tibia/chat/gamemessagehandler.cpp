#include "gamemessagehandler.h"

#include "chat/chatchannel.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/textstorage.h"
#include "config/clientoptiondefaults.h"
#include "input/gameactionhandlers.h"

#include "protocol/protocolcodes.h"
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

/// Hook arguments carry CIP's mode; CIP routes by the protocol's.
int protocolMode(const Args& args, const size_t index)
{
    if (index < args.size())
        if (const auto* mode = std::any_cast<tibia::client::MessageMode>(&args[index]))
            return Proto::translateMessageModeToServer(*mode);
    return -1;
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
    using client::gamecore::Hook;
    // IdleOT host extension: packs post their own status lines (CIP posts SET_STATUS_LINE).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onTextMessage", [this](GameObject*, const Args& args) {
        onTextMessage(protocolMode(args, 0), argString(args, 1));
        return std::any();
    });
    // CIP (case 0x66 of): the status line, gated by gameWindowShowMessages.
    input::setActionHandler(input::EGameAction::SET_STATUS_LINE, [this](const input::TGameActionSetStatusLine& action) {
        if (m_showMessages)
            m_texts.setStatusMessage(action.text(), action.color().isValid() ? action.color() : systemWhite());
        return true;
    });
    // CIP (case 0xa0): a centre message (text storage slot 0xd0), same gate.
    input::setActionHandler(input::EGameAction::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, [this](const input::TGameActionAddTextMessage& action) {
        if (m_showMessages)
            m_texts.addCenterMessage(action.text(), action.color().isValid() ? action.color() : systemWhite());
        return true;
    });
    input::setActionHandler(input::EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW, [this](const input::IGameAction&) {
        m_texts.clearOldestMessage();
        return true;
    });
}

TGameMessageHandler::~TGameMessageHandler()
{
    input::setActionHandler(input::EGameAction::SET_STATUS_LINE, nullptr);
    input::setActionHandler(input::EGameAction::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, nullptr);
    input::setActionHandler(input::EGameAction::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW, nullptr);
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
