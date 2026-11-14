#pragma once

// Classic wire → protobuf for the player's own data: basic and current data,
// skills, state, tactics, the attack target and the spell and use delays.

#include "protocol/protocolclassicwire.h"

namespace tibia::protobuf::protocol {
class GameserverMessage;
}

namespace tibia::protocol {

/// Fill a player message (PlayerDataBasic, PlayerDataCurrent, PlayerSkills,
/// PlayerState, UpdateTarget, SpellDelay, SpellGroupDelay, MultiUseDelay, SetTactics); 0 when the body is short.
size_t fillPlayerMessage(protobuf::protocol::GameserverMessage& msg, int opcode, ClassicReader& r);
bool isPlayerMessage(int opcode);

}
