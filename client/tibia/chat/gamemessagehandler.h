#pragma once

#include "input/gameactiondispatch.h"
#include "shared/cip_typed.h"

#include "client/gamecore.h"

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

/// The game window's client-side lines: SET_STATUS_LINE, the beneath-middle
/// message and pack status lines. The server's talk and text messages go
/// through chat::TChatProtocolMessageHandler.
class TGameMessageHandler : public ::tibia::CipTyped
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
    std::vector<client::gamecore::Hook> m_hooks;

    bool m_showMessages = true;

public:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
