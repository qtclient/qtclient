#pragma once


#include "game/tgamenotify.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>
#include <QJsonObject>
#include <QString>

#include <vector>

namespace tibia {
namespace chat {

class TTextStorage;
class TWhiteBlacklist;

/// The pack's status lines (an IdleOT host extension). The status line and
/// centre message game actions are chat::TChatGameActionHandler's; the
/// server's talk and text messages go through chat::TChatProtocolMessageHandler.
class TGameMessageHandler
{
public:
    explicit TGameMessageHandler(TTextStorage& texts);
    ~TGameMessageHandler();
    TGameMessageHandler(const TGameMessageHandler&) = delete;
    TGameMessageHandler& operator=(const TGameMessageHandler&) = delete;

    /// The gameWindowShowMessages option.
    void setOptions(const QJsonObject& options);

private:
    void onTextMessage(int mode, const QString& text);

    TTextStorage& m_texts;
    std::vector<tibia::game::SoftHook> m_hooks;

    bool m_showMessages = true;
};

}
}
