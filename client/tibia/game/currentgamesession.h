#pragma once

// The game client's current game session, for code no session is handed to:
// the pack ABI peels (mods/host_api_v1.cpp), the pack-facing map, creature and
// thing peels (game/creaturesession.h, mapsession.h, thingsession.h) and the
// pack's own controllers. CipSoft reaches a session only through TGameClient and
// the connectors it calls; qtclient's packs have no connector, so they resolve
// the session's objects through these accessors at call time and rebind on
// subscribeCurrentGameSessionChanged. Never keep what they return: each login,
// replay or tutorial builds a new session with new objects.

#include <functional>
#include <memory>

namespace tibia {
namespace analysers {
class TAnalyserStorages;
}
namespace container {
class TContainerStorage;
class TInventoryContainer;
}
namespace creatures {
class TCreatureStorage;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
namespace teamfinder {
class TTeamFinderStorage;
}
namespace worldmap {
class TAmbientLightStorage;
class TWorldMapStorage;
}
namespace effects {
class TEffectStorage;
}
namespace game {

class IGameSession;
class TGameSessionBase;
class TCooldownStorage;
class TPlayerData;
class TPlayerSkillStats;

/// The current game session (one per login, replay or tutorial); null between them.
TGameSessionBase* currentGameSession();
/// TGameClient::setCurrentGameSession only. Calls the change listeners when it changes.
void setCurrentGameSession(const std::shared_ptr<IGameSession>& session);

/// Calls `listener` each time the current game session changes (also to none).
int subscribeCurrentGameSessionChanged(std::function<void()> listener);
void unsubscribeCurrentGameSessionChanged(int id);

/// The current session's members; null between sessions.
TPlayerData* currentPlayerData();
TPlayerSkillStats* currentPlayerSkillStats();
creatures::TCreatureStorage* currentCreatureStorage();
container::TContainerStorage* currentContainerStorage();
container::TInventoryContainer* currentInventoryContainer();
teamfinder::TTeamFinderStorage* currentTeamFinderStorage();
worldmap::TWorldMapStorage* currentWorldMapStorage();
effects::TEffectStorage* currentEffectStorage();
worldmap::TAmbientLightStorage* currentAmbientLightStorage();

/// CipSoft session members qtclient does not make per session yet: each body
/// reads the class's client-wide object until its domain slice moves it into
/// the session (then it reads the session's getter, and the callers stay).
TCooldownStorage* currentCooldownStorage();
quickloot::TQuickLootBlackWhitelistStorage* currentQuickLootBlackWhitelistStorage();
/// The session's character configuration was read (the analyser and quick-loot
/// storages hold the character's lists); false before that and after the game ended.
bool currentCharacterConfigurationLoaded();

}
}
