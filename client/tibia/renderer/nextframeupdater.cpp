#include "nextframeupdater.h"

#include "chat/textstorage.h"
#include "renderer/worldmaprenderprovider.h"
#include <tibia/creatures/movementtweens.h>
#include <tibia/game/gamesession.h>
#include "worldmap/numericaleffectstorage.h"

namespace tibia::renderer {

TNextFrameUpdater::TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts,
                                     worldmap::TNumericalEffectStorage& numericalEffects)
    : m_provider(provider)
    , m_texts(texts)
    , m_numericalEffects(numericalEffects)
{}

// b1/b1d06504a316841f
void TNextFrameUpdater::v2(const qint64 timestampMs)
{
    tibia::creatures::advanceAllMovementTweens(timestampMs);
    m_texts.update(timestampMs, tibia::game::localPlayerPosition());
    m_numericalEffects.update(timestampMs);
    m_provider.advance();
    tibia::creatures::resumeMovementClock();
}

}
