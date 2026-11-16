#pragma once

#include "game/tgamenotify.h"

#include <cip_gen.h>

#include <memory>
#include <vector>

namespace tibia {
namespace chat {
class TTextStorage;
}
namespace container {
class TContainerStorage;
class TInventoryContainer;
class TDepotSearchDetailListStorage;
class TObjectCountStorage;
}
namespace creatures {
class TCreatureMovementSpeedCalculator;
class TCreatureStorage;
}
namespace game {
class TGameActionRouter;
class TOutfitDialogStorage;
class TPlayerData;
class TPlayerSkillStats;
}
namespace protocol {
class TProtocolMessageQueue;
}
namespace questlog {
class TQuestLogStorage;
}
namespace effects {
class TEffectGameActionHandler;
class TEffectProtocolMessageHandler;
class TEffectStorage;
}
namespace sound {
class TSoundProtocolMessageHandler;
class TSoundStorage;
}
namespace worldmap {
class TAmbientLightStorage;
class TNumericalEffectStorage;
class TWorldMapStorage;
class TWorldMapViewport;
class TWorldmapProtocolMessageHandler;
}
}

namespace tibia::game {

/// CIP game::TGameSessionBase: a game session (live or replayed) owns its
/// storages, protocol message handlers and providers, and hands them out by
/// type. Each session builds its own; the one being played is the active
/// session, whose storages are their classes' instance(). Everything holding
/// a storage rebinds on TGameNotify::gameSessionChanged.
class TGameSessionBase : public cip::tibia::game::TGameSessionBase
{
    Q_OBJECT
public:
    /// Builds the session's storages (the engine must be up).
    explicit TGameSessionBase(QObject* parent = nullptr);
    ~TGameSessionBase() override;

    /// The session being played, or null.
    static TGameSessionBase* active();
    /// Makes this the session being played: its storages become their classes'
    /// instance(), and gameSessionChanged announces it.
    void activate();
    bool isActive() const { return active() == this; }

    const std::shared_ptr<worldmap::TWorldMapViewport>& worldMapViewport() const { return m_worldMapViewport; }
    const std::shared_ptr<creatures::TCreatureMovementSpeedCalculator>& creatureMovementSpeedCalculator() const
    {
        return m_creatureMovementSpeedCalculator;
    }
    const std::shared_ptr<TPlayerData>& playerData() const { return m_playerData; }
    const std::shared_ptr<TPlayerSkillStats>& playerSkillStats() const { return m_playerSkillStats; }
    const std::shared_ptr<questlog::TQuestLogStorage>& questLogStorage() const { return m_questLogStorage; }
    const std::shared_ptr<TOutfitDialogStorage>& outfitDialogStorage() const { return m_outfitDialogStorage; }
    const std::shared_ptr<TGameActionRouter>& gameActionRouter() const { return m_gameActionRouter; }
    const std::shared_ptr<worldmap::TWorldMapStorage>& worldMapStorage() const { return m_worldMapStorage; }
    const std::shared_ptr<worldmap::TAmbientLightStorage>& ambientLightStorage() const { return m_ambientLightStorage; }
    const std::shared_ptr<worldmap::TWorldmapProtocolMessageHandler>& worldmapProtocolMessageHandler() const
    {
        return m_worldmapProtocolMessageHandler;
    }
    const std::shared_ptr<creatures::TCreatureStorage>& creatureStorage() const { return m_creatureStorage; }
    const std::shared_ptr<container::TContainerStorage>& containerStorage() const { return m_containerStorage; }
    const std::shared_ptr<container::TInventoryContainer>& inventoryContainer() const { return m_inventoryContainer; }
    /// The player's inventory counts, by type and tier.
    const std::shared_ptr<container::TObjectCountStorage>& objectCountStorage() const { return m_objectCountStorage; }
    const std::shared_ptr<container::TDepotSearchDetailListStorage>& depotSearchDetailListStorage() const
    {
        return m_depotSearchDetailListStorage;
    }
    const std::shared_ptr<sound::TSoundStorage>& soundStorage() const { return m_soundStorage; }
    const std::shared_ptr<sound::TSoundProtocolMessageHandler>& soundProtocolMessageHandler() const
    {
        return m_soundProtocolMessageHandler;
    }
    const std::shared_ptr<chat::TTextStorage>& textStorage() const { return m_textStorage; }
    const std::shared_ptr<worldmap::TNumericalEffectStorage>& numericalEffectStorage() const { return m_numericalEffectStorage; }
    const std::shared_ptr<effects::TEffectStorage>& effectStorage() const { return m_effectStorage; }
    const std::shared_ptr<effects::TEffectGameActionHandler>& effectGameActionHandler() const { return m_effectGameActionHandler; }
    const std::shared_ptr<effects::TEffectProtocolMessageHandler>& effectProtocolMessageHandler() const
    {
        return m_effectProtocolMessageHandler;
    }

private:
    /// CIP: the session's storages (world map, creatures, containers, ...).
    void createStorages();
    /// CIP: the protocol message handlers that fill them.
    void createProtocolMessageHandlers();
    void install(bool installed);
    /// CIP: connects the session's message queue to its handlers' typed slots.
    void connectProtocolMessageHandlers(protocol::TProtocolMessageQueue* queue);
    void releaseStorages();

    bool m_worldInitialized = false;
    std::vector<QMetaObject::Connection> m_queueConnections;
    std::shared_ptr<worldmap::TWorldMapViewport> m_worldMapViewport;
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_creatureMovementSpeedCalculator;
    std::shared_ptr<TPlayerData> m_playerData;
    std::shared_ptr<TPlayerSkillStats> m_playerSkillStats;
    std::shared_ptr<questlog::TQuestLogStorage> m_questLogStorage;
    std::shared_ptr<TOutfitDialogStorage> m_outfitDialogStorage;
    std::shared_ptr<TGameActionRouter> m_gameActionRouter;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<worldmap::TAmbientLightStorage> m_ambientLightStorage;
    std::shared_ptr<worldmap::TWorldmapProtocolMessageHandler> m_worldmapProtocolMessageHandler;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<container::TContainerStorage> m_containerStorage;
    std::shared_ptr<container::TInventoryContainer> m_inventoryContainer;
    std::shared_ptr<container::TObjectCountStorage> m_objectCountStorage;
    std::shared_ptr<container::TDepotSearchDetailListStorage> m_depotSearchDetailListStorage;
    std::shared_ptr<sound::TSoundStorage> m_soundStorage;
    std::shared_ptr<sound::TSoundProtocolMessageHandler> m_soundProtocolMessageHandler;
    std::shared_ptr<chat::TTextStorage> m_textStorage;
    std::shared_ptr<worldmap::TNumericalEffectStorage> m_numericalEffectStorage;
    std::shared_ptr<effects::TEffectStorage> m_effectStorage;
    std::shared_ptr<effects::TEffectGameActionHandler> m_effectGameActionHandler;
    std::shared_ptr<effects::TEffectProtocolMessageHandler> m_effectProtocolMessageHandler;
};

/// Calls `bind` now and each time the active game session changes, for code
/// that connects to a session storage's signals.
template<typename Bind>
void onEachGameSession(QObject* context, Bind bind)
{
    bind();
    QObject::connect(TGameNotify::instance(), &TGameNotify::gameSessionChanged, context, bind);
}

}
