#include "nextframeupdater.h"

#include "analysers/analyserstorages.h"
#include "chat/textstorage.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "effects/effectstorage.h"
#include "game/cooldownstorage.h"
#include "renderer/worldmaprenderprovider.h"
#include "sound/soundstorage.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/worldmapstorage.h"
#include <tibia/creatures/movementtweens.h>
#include <tibia/game/gamesession.h>

#include <utility>

namespace tibia::renderer {

TNextFrameUpdater::TNextFrameUpdater(std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                                     std::shared_ptr<effects::TEffectStorage> effectStorage,
                                     std::shared_ptr<chat::TTextStorage> textStorage,
                                     std::shared_ptr<game::TCooldownStorage> cooldownStorage,
                                     std::shared_ptr<worldmap::TAmbientLightStorage> ambientLightStorage,
                                     std::shared_ptr<cyclopedia::TItemTrackingStorage> itemTrackingStorage,
                                     std::shared_ptr<analysers::TGainWasteStorage> gainWasteStorage,
                                     std::shared_ptr<analysers::TImpactStorage> impactStorage,
                                     std::shared_ptr<analysers::TDamageInputStorage> damageInputStorage,
                                     std::shared_ptr<analysers::TProgressStorage> progressStorage,
                                     std::shared_ptr<analysers::THuntingSessionStorage> huntingSessionStorage,
                                     std::shared_ptr<sound::TSoundStorage> soundStorage)
    : m_worldMapStorage(std::move(worldMapStorage))
    , m_effectStorage(std::move(effectStorage))
    , m_textStorage(std::move(textStorage))
    , m_cooldownStorage(std::move(cooldownStorage))
    , m_ambientLightStorage(std::move(ambientLightStorage))
    , m_itemTrackingStorage(std::move(itemTrackingStorage))
    , m_gainWasteStorage(std::move(gainWasteStorage))
    , m_impactStorage(std::move(impactStorage))
    , m_damageInputStorage(std::move(damageInputStorage))
    , m_progressStorage(std::move(progressStorage))
    , m_huntingSessionStorage(std::move(huntingSessionStorage))
    , m_soundStorage(std::move(soundStorage))
{
}

void TNextFrameUpdater::setWorldMapRenderProvider(std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider)
{
    m_worldMapRenderProvider = std::move(worldMapRenderProvider);
}

void TNextFrameUpdater::update(const qint64& nextFrameTimestamp)
{
    // Host: the creatures' movement tweens advance before the storages.
    tibia::creatures::advanceAllMovementTweens(nextFrameTimestamp);

    m_worldMapStorage->updateAnimations(nextFrameTimestamp);
    m_effectStorage->update(nextFrameTimestamp);
    m_textStorage->update(nextFrameTimestamp);
    // Host: the cooldowns' end times and the analysers' timestamps are on
    // their storages' own clocks, so they advance to those clocks' now.
    m_cooldownStorage->expireCooldowns(game::TCooldownStorage::now());
    m_ambientLightStorage->update(nextFrameTimestamp);
    const qint64 analysersNow = analysers::TAnalyserStorages::nowMs();
    m_itemTrackingStorage->update(analysersNow);
    m_gainWasteStorage->tick(analysersNow);
    m_impactStorage->tick(analysersNow);
    m_damageInputStorage->update(analysersNow);
    m_progressStorage->tick(analysersNow);
    m_huntingSessionStorage->tick(analysersNow);
    m_soundStorage->update(nextFrameTimestamp);

    // Host: the world map's frame is built here, then the movement clock
    // runs on and the local player's walk goes on.
    if (m_worldMapRenderProvider)
        m_worldMapRenderProvider->advance();
    tibia::creatures::resumeMovementClock();
    tibia::game::localPlayerMovementTick();
}

}
