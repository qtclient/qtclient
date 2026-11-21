#pragma once

#include "game/igamesession.h"
#include "game/tgamenotify.h"

#include <memory>
#include <vector>

namespace tibia {
namespace chat {
class TChatChannelStorage;
class TChatGameActionHandler;
class TChatProtocolMessageHandler;
class TChatStorage;
class TTextStorage;
class TWhiteBlacklist;
}
namespace container {
class TContainerStorage;
class TInventoryContainer;
class TDepotSearchDetailListStorage;
class TObjectAppearanceInstanceInfoStorage;
class TObjectCountStorage;
class TStashStorage;
}
namespace creatures {
class TCreatureMovementSpeedCalculator;
class TCreatureStorage;
class TUnjustifiedPointsStorage;
class TVipGameActionHandler;
class TVipProtocolMessageHandler;
class TVipStorage;
}
namespace exivaoptions {
class TExivaOptionsProtocolMessageHandler;
class TExivaOptionsStorage;
}
namespace friends {
class TFriendsAccountSearchStorage;
class TFriendsFriendListStorage;
class TFriendsInviteStorage;
class TFriendsProtocolMessageHandler;
class TFriendsStorage;
}
namespace teamfinder {
class TTeamFinderProtocolMessageHandler;
class TTeamFinderStorage;
}
namespace trade {
class TCharacterTradeProtocolMessageHandler;
class TCharacterTradeStorage;
class TNPCTradeProtocolMessageHandler;
class TNPCTradeStorage;
class TPlayerTradeGameActionHandler;
class TPlayerTradeProtocolMessageHandler;
class TPlayerTradeStorage;
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
class TEffectGameActionHandler;
}
namespace sound {
class TSoundProtocolMessageHandler;
class TSoundStorage;
class TGameSessionSoundProvider;
class TObjectSoundController;
}
namespace worldmap {
class TAmbientLightStorage;
class TWorldMapStorage;
class TWorldMapViewport;
class TWorldmapProtocolMessageHandler;
}
}

namespace tibia::game {

/// CIP game::TGameSessionBase: a game session (live or replayed) owns its
/// storages, protocol message handlers and providers, and hands them out by
/// type. Each login builds a new one; the game client's current session is
/// the active one, whose storages are their classes' instance(). Everything
/// holding a storage rebinds on TGameNotify::gameSessionChanged.
class TGameSessionBase : public IGameSession
{
    Q_OBJECT
public:
    explicit TGameSessionBase(QObject* parent = nullptr);
    ~TGameSessionBase() override;

    /// Keeps the meta info and builds the session's protocol message queue
    /// from its configuration.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;

    /// The game client's current session, or null.
    static TGameSessionBase* active();
    bool isActive() const { return active() == this; }
    /// The game client makes this its current session (or stops it being
    /// one): its storages become their classes' instance(), and
    /// gameSessionChanged announces it.
    void install(bool installed);
    /// Re-links the protocol queue to the handlers that exist now. The game
    /// window creates most handlers after a replay given on the command line
    /// started.
    void relinkProtocolMessageHandlers();

    const std::shared_ptr<protocol::TProtocolMessageQueue>& protocolMessageQueue() const { return m_protocolMessageQueue; }
    const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo() const { return m_metaInfo; }

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
    /// The supply stash's content.
    const std::shared_ptr<container::TStashStorage>& stashStorage() const { return m_stashStorage; }
    const std::shared_ptr<container::TDepotSearchDetailListStorage>& depotSearchDetailListStorage() const
    {
        return m_depotSearchDetailListStorage;
    }
    /// The server's names (and the player's counts) of object instances.
    const std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage>& objectAppearanceInstanceInfoStorage() const
    {
        return m_objectAppearanceInstanceInfoStorage;
    }
    const std::shared_ptr<sound::TSoundStorage>& soundStorage() const { return m_soundStorage; }
    const std::shared_ptr<sound::TGameSessionSoundProvider>& gameSessionSoundProvider() const
    {
        return m_gameSessionSoundProvider;
    }
    const std::shared_ptr<sound::TObjectSoundController>& objectSoundController() const { return m_objectSoundController; }
    const std::shared_ptr<sound::TSoundProtocolMessageHandler>& soundProtocolMessageHandler() const
    {
        return m_soundProtocolMessageHandler;
    }
    const std::shared_ptr<chat::TTextStorage>& textStorage() const { return m_textStorage; }
    const std::shared_ptr<effects::TEffectStorage>& effectStorage() const { return m_effectStorage; }
    const std::shared_ptr<effects::TEffectGameActionHandler>& effectGameActionHandler() const { return m_effectGameActionHandler; }
    const std::shared_ptr<effects::TEffectProtocolMessageHandler>& effectProtocolMessageHandler() const
    {
        return m_effectProtocolMessageHandler;
    }
    /// The chat's channels and the NPC conversation.
    const std::shared_ptr<chat::TChatChannelStorage>& chatChannelStorage() const { return m_chatChannelStorage; }
    const std::shared_ptr<chat::TChatStorage>& chatStorage() const { return m_chatStorage; }
    const std::shared_ptr<creatures::TVipStorage>& vipStorage() const { return m_vipStorage; }
    const std::shared_ptr<creatures::TUnjustifiedPointsStorage>& unjustifiedPointsStorage() const { return m_unjustifiedPointsStorage; }
    const std::shared_ptr<trade::TNPCTradeStorage>& npcTradeStorage() const { return m_npcTradeStorage; }
    const std::shared_ptr<trade::TPlayerTradeStorage>& playerTradeStorage() const { return m_playerTradeStorage; }
    const std::shared_ptr<friends::TFriendsFriendListStorage>& friendsFriendListStorage() const { return m_friendsFriendListStorage; }
    const std::shared_ptr<friends::TFriendsAccountSearchStorage>& friendsAccountSearchStorage() const
    {
        return m_friendsAccountSearchStorage;
    }
    const std::shared_ptr<friends::TFriendsInviteStorage>& friendsInviteStorage() const { return m_friendsInviteStorage; }
    const std::shared_ptr<friends::TFriendsStorage>& friendsStorage() const { return m_friendsStorage; }
    const std::shared_ptr<exivaoptions::TExivaOptionsStorage>& exivaOptionsStorage() const { return m_exivaOptionsStorage; }
    const std::shared_ptr<trade::TCharacterTradeStorage>& characterTradeStorage() const { return m_characterTradeStorage; }
    const std::shared_ptr<teamfinder::TTeamFinderStorage>& teamFinderStorage() const { return m_teamFinderStorage; }
    const std::shared_ptr<chat::TChatProtocolMessageHandler>& chatProtocolMessageHandler() const { return m_chatProtocolMessageHandler; }
    const std::shared_ptr<trade::TNPCTradeProtocolMessageHandler>& npcTradeProtocolMessageHandler() const
    {
        return m_npcTradeProtocolMessageHandler;
    }
    const std::shared_ptr<trade::TPlayerTradeProtocolMessageHandler>& playerTradeProtocolMessageHandler() const
    {
        return m_playerTradeProtocolMessageHandler;
    }
    const std::shared_ptr<creatures::TVipProtocolMessageHandler>& vipProtocolMessageHandler() const { return m_vipProtocolMessageHandler; }
    const std::shared_ptr<exivaoptions::TExivaOptionsProtocolMessageHandler>& exivaOptionsProtocolMessageHandler() const
    {
        return m_exivaOptionsProtocolMessageHandler;
    }
    const std::shared_ptr<teamfinder::TTeamFinderProtocolMessageHandler>& teamFinderProtocolMessageHandler() const
    {
        return m_teamFinderProtocolMessageHandler;
    }
    const std::shared_ptr<friends::TFriendsProtocolMessageHandler>& friendsProtocolMessageHandler() const
    {
        return m_friendsProtocolMessageHandler;
    }
    const std::shared_ptr<trade::TCharacterTradeProtocolMessageHandler>& characterTradeProtocolMessageHandler() const
    {
        return m_characterTradeProtocolMessageHandler;
    }
    const std::shared_ptr<chat::TChatGameActionHandler>& chatGameActionHandler() const { return m_chatGameActionHandler; }
    const std::shared_ptr<creatures::TVipGameActionHandler>& vipGameActionHandler() const { return m_vipGameActionHandler; }
    const std::shared_ptr<trade::TPlayerTradeGameActionHandler>& playerTradeGameActionHandler() const
    {
        return m_playerTradeGameActionHandler;
    }

    /// The client's ignore and white lists reach the session's chat and team
    /// finder handlers (null when the session stops being the current one).
    void setWhiteBlacklist(chat::TWhiteBlacklist* whiteBlacklist);

protected:
    /// CIP: the session's storages (world map, creatures, containers, ...).
    void createStorages();
    /// CIP: the protocol message handlers that fill them.
    void createProtocolMessageHandlers();
    /// CIP: connects the session's message queue to its handlers' typed slots.
    void connectProtocolMessageHandlers();

private:
    void releaseStorages();

    bool m_worldInitialized = false;
    std::vector<QMetaObject::Connection> m_queueConnections;
    std::shared_ptr<const TGameSessionMetaInfo> m_metaInfo;
    std::shared_ptr<protocol::TProtocolMessageQueue> m_protocolMessageQueue;
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
    std::shared_ptr<container::TStashStorage> m_stashStorage;
    std::shared_ptr<container::TObjectCountStorage> m_objectCountStorage;
    std::shared_ptr<container::TDepotSearchDetailListStorage> m_depotSearchDetailListStorage;
    std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> m_objectAppearanceInstanceInfoStorage;
    std::shared_ptr<sound::TSoundStorage> m_soundStorage;
    std::shared_ptr<sound::TSoundProtocolMessageHandler> m_soundProtocolMessageHandler;
    std::shared_ptr<sound::TObjectSoundController> m_objectSoundController;
    std::shared_ptr<sound::TGameSessionSoundProvider> m_gameSessionSoundProvider;
    std::shared_ptr<chat::TTextStorage> m_textStorage;
    std::shared_ptr<effects::TEffectStorage> m_effectStorage;
    std::shared_ptr<effects::TEffectGameActionHandler> m_effectGameActionHandler;
    std::shared_ptr<effects::TEffectProtocolMessageHandler> m_effectProtocolMessageHandler;
    std::shared_ptr<chat::TChatChannelStorage> m_chatChannelStorage;
    std::shared_ptr<chat::TChatStorage> m_chatStorage;
    std::shared_ptr<creatures::TVipStorage> m_vipStorage;
    std::shared_ptr<creatures::TUnjustifiedPointsStorage> m_unjustifiedPointsStorage;
    std::shared_ptr<trade::TNPCTradeStorage> m_npcTradeStorage;
    std::shared_ptr<trade::TPlayerTradeStorage> m_playerTradeStorage;
    std::shared_ptr<friends::TFriendsFriendListStorage> m_friendsFriendListStorage;
    std::shared_ptr<friends::TFriendsAccountSearchStorage> m_friendsAccountSearchStorage;
    std::shared_ptr<friends::TFriendsInviteStorage> m_friendsInviteStorage;
    std::shared_ptr<friends::TFriendsStorage> m_friendsStorage;
    std::shared_ptr<exivaoptions::TExivaOptionsStorage> m_exivaOptionsStorage;
    std::shared_ptr<trade::TCharacterTradeStorage> m_characterTradeStorage;
    std::shared_ptr<teamfinder::TTeamFinderStorage> m_teamFinderStorage;
    std::shared_ptr<chat::TChatProtocolMessageHandler> m_chatProtocolMessageHandler;
    std::shared_ptr<trade::TNPCTradeProtocolMessageHandler> m_npcTradeProtocolMessageHandler;
    std::shared_ptr<trade::TPlayerTradeProtocolMessageHandler> m_playerTradeProtocolMessageHandler;
    std::shared_ptr<creatures::TVipProtocolMessageHandler> m_vipProtocolMessageHandler;
    std::shared_ptr<exivaoptions::TExivaOptionsProtocolMessageHandler> m_exivaOptionsProtocolMessageHandler;
    std::shared_ptr<teamfinder::TTeamFinderProtocolMessageHandler> m_teamFinderProtocolMessageHandler;
    std::shared_ptr<friends::TFriendsProtocolMessageHandler> m_friendsProtocolMessageHandler;
    std::shared_ptr<trade::TCharacterTradeProtocolMessageHandler> m_characterTradeProtocolMessageHandler;
    std::shared_ptr<chat::TChatGameActionHandler> m_chatGameActionHandler;
    std::shared_ptr<creatures::TVipGameActionHandler> m_vipGameActionHandler;
    std::shared_ptr<trade::TPlayerTradeGameActionHandler> m_playerTradeGameActionHandler;
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
