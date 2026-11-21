#include "game/currentgamesession.h"

#include "analysers/analyserstorages.h"
#include "container/containerciptypes.h"
#include "creatures/creaturestorage.h"
#include "game/cooldownstorage.h"
#include "game/gamesessionbase.h"
#include "game/igamesession.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace tibia::game {

namespace {

struct TCurrentGameSession
{
    /// The session itself is the game client's; this only must not outlive it.
    std::weak_ptr<IGameSession> owner;
    TGameSessionBase* session = nullptr;
    int nextListenerId = 1;
    std::vector<std::pair<int, std::function<void()>>> listeners;
};

TCurrentGameSession& current()
{
    static TCurrentGameSession state;
    return state;
}

}

TGameSessionBase* currentGameSession()
{
    TCurrentGameSession& state = current();
    return state.owner.expired() ? nullptr : state.session;
}

void setCurrentGameSession(const std::shared_ptr<IGameSession>& session)
{
    TCurrentGameSession& state = current();
    auto* next = dynamic_cast<TGameSessionBase*>(session.get());
    if (next == currentGameSession())
        return;
    state.owner = next ? session : std::shared_ptr<IGameSession>();
    state.session = next;
    // A copy: a listener may subscribe or unsubscribe.
    const auto listeners = state.listeners;
    for (const auto& [id, listener] : listeners) {
        if (listener)
            listener();
    }
}

int subscribeCurrentGameSessionChanged(std::function<void()> listener)
{
    TCurrentGameSession& state = current();
    const int id = state.nextListenerId++;
    state.listeners.emplace_back(id, std::move(listener));
    return id;
}

void unsubscribeCurrentGameSessionChanged(const int id)
{
    std::erase_if(current().listeners, [id](const auto& entry) { return entry.first == id; });
}

TPlayerData* currentPlayerData()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->playerData().get() : nullptr;
}

TPlayerSkillStats* currentPlayerSkillStats()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->playerSkillStats().get() : nullptr;
}

creatures::TCreatureStorage* currentCreatureStorage()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->creatureStorage().get() : nullptr;
}

container::TContainerStorage* currentContainerStorage()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->containerStorage().get() : nullptr;
}

container::TInventoryContainer* currentInventoryContainer()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->inventoryContainer().get() : nullptr;
}

TCooldownStorage* currentCooldownStorage()
{
    TGameSessionBase* session = currentGameSession();
    return session ? session->cooldownStorage().get() : nullptr;
}

quickloot::TQuickLootBlackWhitelistStorage* currentQuickLootBlackWhitelistStorage()
{
    // Client-wide until the quick-loot lists are a session member (CIP createStorages).
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    return storages ? storages->lootBlackWhitelist() : nullptr;
}

bool currentCharacterConfigurationLoaded()
{
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    return storages && storages->hasCharacter();
}

}
