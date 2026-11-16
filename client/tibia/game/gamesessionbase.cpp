#include "game/gamesessionbase.h"

#include "game/tgamenotify.h"
#include "protocol/protocolciptypes.h"

#include "chat/textstorage.h"
#include "container/containerciptypes.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
#include "game/gameciptypes.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include "questlog/questlogciptypes.h"
#include "effects/effectprotocolmessagehandler.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "sound/soundprotocolmessagehandler.h"
#include "sound/soundstorage.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/numericaleffectstorage.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

namespace tibia::game {

namespace {

TGameSessionBase* g_active = nullptr;

}

TGameSessionBase::TGameSessionBase(QObject* parent)
    : cip::tibia::game::TGameSessionBase(parent)
    , m_worldMapViewport(std::make_shared<worldmap::TWorldMapViewport>())
    , m_creatureMovementSpeedCalculator(std::make_shared<creatures::TCreatureMovementSpeedCalculator>())
    // CIP's world camera (renderer::TWorldMapCamera) is the render provider's here.
    , m_playerData(std::make_shared<TPlayerData>())
    , m_playerSkillStats(std::make_shared<TPlayerSkillStats>())
    , m_questLogStorage(std::make_shared<questlog::TQuestLogStorage>())
    , m_outfitDialogStorage(std::make_shared<TOutfitDialogStorage>())
    , m_gameActionRouter(std::make_shared<TGameActionRouter>())
{
    createStorages();
    createProtocolMessageHandlers();
}

void TGameSessionBase::createStorages()
{
    m_worldMapStorage = std::make_shared<worldmap::TWorldMapStorage>(m_creatureMovementSpeedCalculator.get());
    m_ambientLightStorage = std::make_shared<worldmap::TAmbientLightStorage>();
    m_creatureStorage = std::make_shared<creatures::TCreatureStorage>();
    m_effectStorage = std::make_shared<effects::TEffectStorage>();
    m_containerStorage = std::make_shared<container::TContainerStorage>();
    m_inventoryContainer = std::make_shared<container::TInventoryContainer>();
    m_objectCountStorage = std::make_shared<container::TObjectCountStorage>();
    m_depotSearchDetailListStorage = std::make_shared<container::TDepotSearchDetailListStorage>();
    m_soundStorage = std::make_shared<sound::TSoundStorage>();
    m_textStorage = std::make_shared<chat::TTextStorage>();
    m_numericalEffectStorage = std::make_shared<worldmap::TNumericalEffectStorage>();
    // Numerical effects count against the effect limit.
    m_effectStorage->setNumericalCount([numerical = m_numericalEffectStorage.get()] { return numerical->effects().size(); });
    // The own creature's arrival clears the map texts (CIP TTextStorage).
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
            [texts = m_textStorage.get()](std::weak_ptr<creatures::TCreature> player) {
                texts->onPlayerCreatureAddedToGameSession(std::move(player));
            });
}

void TGameSessionBase::createProtocolMessageHandlers()
{
    m_worldmapProtocolMessageHandler = std::make_shared<worldmap::TWorldmapProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler->setViewport(m_worldMapViewport.get());
    m_effectProtocolMessageHandler = std::make_shared<effects::TEffectProtocolMessageHandler>();
    m_soundProtocolMessageHandler = std::make_shared<sound::TSoundProtocolMessageHandler>();
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>();
}

TGameSessionBase::~TGameSessionBase()
{
    if (isActive()) {
        install(false);
        g_active = nullptr;
        TGameNotify::instance()->emitGameSessionChanged();
    }
    releaseStorages();
}

TGameSessionBase* TGameSessionBase::active()
{
    return g_active;
}

void TGameSessionBase::activate()
{
    if (isActive())
        return;
    if (g_active)
        g_active->install(false);
    g_active = this;
    install(true);
    // The map's session state needs its storage installed (aware range, pruning).
    if (!m_worldInitialized) {
        m_worldMapStorage->sessionState().init();
        m_worldInitialized = true;
    }
    TGameNotify::instance()->emitGameSessionChanged();
}

void TGameSessionBase::install(const bool installed)
{
    worldmap::TWorldMapStorage::setInstance(installed ? m_worldMapStorage.get() : nullptr);
    creatures::TCreatureStorage::setInstance(installed ? m_creatureStorage.get() : nullptr);
    container::TContainerStorage::setInstance(installed ? m_containerStorage.get() : nullptr);
    container::TInventoryContainer::setInstance(installed ? m_inventoryContainer.get() : nullptr);
    worldmap::TWorldmapProtocolMessageHandler::setLiveInstance(installed ? m_worldmapProtocolMessageHandler.get() : nullptr);
    effects::TEffectStorage::setInstance(installed ? m_effectStorage.get() : nullptr);
    TPlayerData::setLiveInstance(installed ? m_playerData.get() : nullptr);
    TPlayerSkillStats::setLiveInstance(installed ? m_playerSkillStats.get() : nullptr);

    // Only the active session's handlers read the protocol queue.
    for (const QMetaObject::Connection& connection : m_queueConnections)
        disconnect(connection);
    m_queueConnections.clear();
    protocol::TProtocolMessageQueue* queue = protocol::protocolMessageQueue();
    if (!installed || !queue)
        return;
    m_queueConnections.push_back(connect(queue, &protocol::TProtocolMessageQueue::serverMessageEnqueued,
                                         m_worldmapProtocolMessageHandler.get(),
                                         &worldmap::TWorldmapProtocolMessageHandler::onServerMessageEnqueued));
    m_queueConnections.push_back(connect(queue, &protocol::TProtocolMessageQueue::serverMessageEnqueued,
                                         m_effectProtocolMessageHandler.get(),
                                         &effects::TEffectProtocolMessageHandler::onServerMessageEnqueued));
}

void TGameSessionBase::releaseStorages()
{
    // Effects and texts refer to the map; the map handler to its storage.
    m_effectProtocolMessageHandler.reset();
    m_effectGameActionHandler.reset();
    m_effectStorage.reset();
    m_numericalEffectStorage.reset();
    m_textStorage.reset();
    m_ambientLightStorage.reset();
    m_worldmapProtocolMessageHandler.reset();
    m_worldMapStorage.reset();
    m_creatureStorage.reset();
    m_inventoryContainer.reset();
    m_containerStorage.reset();
    m_soundProtocolMessageHandler.reset();
    m_soundStorage.reset();
    m_gameActionRouter.reset();
    m_outfitDialogStorage.reset();
    m_questLogStorage.reset();
    m_playerSkillStats.reset();
    m_playerData.reset();
    m_creatureMovementSpeedCalculator.reset();
}

}
