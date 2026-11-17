#include "nextframeupdater.h"

#include "chat/textstorage.h"
#include "game/cooldownstorage.h"
#include "worldmap/ambientlightstorage.h"
#include "renderer/worldmaprenderprovider.h"
#include <tibia/creatures/movementtweens.h>
#include <tibia/game/gamesession.h>
#include "effects/effectstorage.h"

namespace tibia::renderer {

TNextFrameUpdater::TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts)
    : m_provider(provider)
    , m_texts(texts)
{}

void TNextFrameUpdater::v2(const qint64 timestampMs)
{
    tibia::creatures::advanceAllMovementTweens(timestampMs);
    if (worldmap::TAmbientLightStorage* ambient = worldmap::TAmbientLightStorage::instance())
        ambient->update(timestampMs);
    m_texts.update(timestampMs);
    // Cooldowns that ran out drop and announce it; their end times are on
    // the cooldown storage's own clock.
    if (game::TCooldownStorage* cooldowns = game::TCooldownStorage::instance())
        cooldowns->expireCooldowns(game::TCooldownStorage::now());
    effects::TEffectStorage::instance().update(timestampMs);
    m_provider.advance();
    tibia::creatures::resumeMovementClock();
    tibia::game::localPlayerMovementTick();
}

}
