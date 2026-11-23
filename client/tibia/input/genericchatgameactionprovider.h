#pragma once

#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <QString>

namespace tibia {
namespace chat {
class TChatChannelStorage;
class TWhiteBlacklist;
}
namespace creatures {
class TVipStorage;
}
namespace input {

/// The chat actions on a player's name (CIP: input::TGenericChatGameActionProvider):
/// message, invite to the own private channel, report, ignore or unignore, add as VIP.
class TGenericChatGameActionProvider : public ::tibia::CipTyped
{
public:
    TGenericChatGameActionProvider(const chat::TChatChannelStorage* chatChannelStorage, const creatures::TVipStorage* vipStorage);

    /// v2: the ignore list the ignore / unignore choice follows; without one
    /// neither is offered.
    void setWhiteBlacklist(const chat::TWhiteBlacklist* whiteBlacklist);
    /// v3: appends the actions on `name` to `actions`.
    void appendGameActions(TGameActionList& actions, const QString& name) const;

private:
    const chat::TChatChannelStorage* m_chatChannelStorage;
    const creatures::TVipStorage* m_vipStorage;
    const chat::TWhiteBlacklist* m_whiteBlacklist = nullptr;
};

}
}
