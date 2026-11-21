#pragma once

#include "config/characterconfigurationmanager.h"
#include "game/igamesession.h"
#include "game/tgamenotify.h"

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
class TObjectAppearanceInstanceInfoStorage;
class TObjectCountStorage;
class TStashStorage;
}
namespace creatures {
class TCreatureMovementSpeedCalculator;
class TCreatureStorage;
}
namespace game {
class TBlessingsProtocolMessageHandler;
class TCooldownStorage;
class TEditTextProtocolMessageHandler;
class TGameActionRouter;
class TGameEventProtocolMessageHandler;
class TGameProtocolMessageHandler;
class TOutfitDialogStorage;
class TPlayerData;
class TPlayerGameActionHandler;
class TPlayerProtocolMessageHandler;
class TPlayerSkillStats;
class TPremiumProtocolMessageHandler;
class TServerModalDialogProtocolMessageHandler;
class TSpellBookProtocolMessageHandler;
}
namespace creatures {
class TCreaturesGameActionHandler;
class TUnjustifiedPointsProtocolMessageHandler;
}
namespace input {
class TCrossHairSpellGameActionHandler;
class TGenericGameActionHandler;
class TUseWithGameActionHandler;
}
namespace news {
class TNewsProtocolMessageHandler;
}
namespace reporting {
class TBugReportProtocolMessageHandler;
class TRuleViolationProtocolMessageHandler;
}
namespace tutorial {
class TTutorialProtocolMessageHandler;
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
    const std::shared_ptr<TCooldownStorage>& cooldownStorage() const { return m_cooldownStorage; }
    /// Loads and saves the character's configuration storages (sidebars,
    /// action bars, status bar, analysers, ...), which it holds.
    const std::shared_ptr<config::TCharacterConfigurationManager>& characterConfigurationManager() const
    {
        return m_characterConfigurationManager;
    }
    const std::shared_ptr<TPlayerProtocolMessageHandler>& playerProtocolMessageHandler() const { return m_playerProtocolMessageHandler; }
    const std::shared_ptr<TGameProtocolMessageHandler>& gameProtocolMessageHandler() const { return m_gameProtocolMessageHandler; }
    const std::shared_ptr<TSpellBookProtocolMessageHandler>& spellBookProtocolMessageHandler() const
    {
        return m_spellBookProtocolMessageHandler;
    }
    const std::shared_ptr<TServerModalDialogProtocolMessageHandler>& serverModalDialogProtocolMessageHandler() const
    {
        return m_serverModalDialogProtocolMessageHandler;
    }
    const std::shared_ptr<reporting::TRuleViolationProtocolMessageHandler>& ruleViolationProtocolMessageHandler() const
    {
        return m_ruleViolationProtocolMessageHandler;
    }
    const std::shared_ptr<reporting::TBugReportProtocolMessageHandler>& bugReportProtocolMessageHandler() const
    {
        return m_bugReportProtocolMessageHandler;
    }
    const std::shared_ptr<TEditTextProtocolMessageHandler>& editTextProtocolMessageHandler() const
    {
        return m_editTextProtocolMessageHandler;
    }
    const std::shared_ptr<news::TNewsProtocolMessageHandler>& newsProtocolMessageHandler() const { return m_newsProtocolMessageHandler; }
    const std::shared_ptr<TPremiumProtocolMessageHandler>& premiumProtocolMessageHandler() const
    {
        return m_premiumProtocolMessageHandler;
    }
    const std::shared_ptr<tutorial::TTutorialProtocolMessageHandler>& tutorialProtocolMessageHandler() const
    {
        return m_tutorialProtocolMessageHandler;
    }
    const std::shared_ptr<TBlessingsProtocolMessageHandler>& blessingsProtocolMessageHandler() const
    {
        return m_blessingsProtocolMessageHandler;
    }
    const std::shared_ptr<TGameEventProtocolMessageHandler>& gameEventProtocolMessageHandler() const
    {
        return m_gameEventProtocolMessageHandler;
    }
    const std::shared_ptr<input::TGenericGameActionHandler>& genericGameActionHandler() const { return m_genericGameActionHandler; }

    /// Host shim until the analysers, quick loot, Cyclopedia map and skill
    /// wheel storages are the session's: the game client hands in its own
    /// before initialize(), and the character configuration manager takes them.
    void setCharacterConfigurationStorages(const config::TCharacterConfigurationManager::TStorages& storages)
    {
        m_characterConfigurationStorages = storages;
    }

protected:
    /// CIP: the session's storages (world map, creatures, containers, ...).
    void createStorages();
    /// CIP: the protocol message handlers that fill them.
    void createProtocolMessageHandlers();
    /// CIP: connects the session's message queue to its handlers' typed slots.
    void connectProtocolMessageHandlers();
    /// CIP: the game action handlers, on the session's game action router.
    void createGameActionHandlers();

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
    std::shared_ptr<TCooldownStorage> m_cooldownStorage;
    std::shared_ptr<config::TCharacterConfigurationManager> m_characterConfigurationManager;
    std::shared_ptr<input::TGenericGameActionHandler> m_genericGameActionHandler;
    std::shared_ptr<creatures::TCreaturesGameActionHandler> m_creaturesGameActionHandler;
    std::shared_ptr<input::TUseWithGameActionHandler> m_useWithGameActionHandler;
    std::shared_ptr<input::TCrossHairSpellGameActionHandler> m_crossHairSpellGameActionHandler;
    std::shared_ptr<TPlayerGameActionHandler> m_playerGameActionHandler;
    std::shared_ptr<TGameProtocolMessageHandler> m_gameProtocolMessageHandler;
    std::shared_ptr<TPlayerProtocolMessageHandler> m_playerProtocolMessageHandler;
    std::shared_ptr<TServerModalDialogProtocolMessageHandler> m_serverModalDialogProtocolMessageHandler;
    std::shared_ptr<reporting::TRuleViolationProtocolMessageHandler> m_ruleViolationProtocolMessageHandler;
    std::shared_ptr<reporting::TBugReportProtocolMessageHandler> m_bugReportProtocolMessageHandler;
    std::shared_ptr<TEditTextProtocolMessageHandler> m_editTextProtocolMessageHandler;
    std::shared_ptr<creatures::TUnjustifiedPointsProtocolMessageHandler> m_unjustifiedPointsProtocolMessageHandler;
    std::shared_ptr<news::TNewsProtocolMessageHandler> m_newsProtocolMessageHandler;
    std::shared_ptr<TPremiumProtocolMessageHandler> m_premiumProtocolMessageHandler;
    std::shared_ptr<tutorial::TTutorialProtocolMessageHandler> m_tutorialProtocolMessageHandler;
    std::shared_ptr<TBlessingsProtocolMessageHandler> m_blessingsProtocolMessageHandler;
    std::shared_ptr<TGameEventProtocolMessageHandler> m_gameEventProtocolMessageHandler;
    std::shared_ptr<TSpellBookProtocolMessageHandler> m_spellBookProtocolMessageHandler;
    config::TCharacterConfigurationManager::TStorages m_characterConfigurationStorages;
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
