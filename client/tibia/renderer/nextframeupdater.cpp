#include "nextframeupdater.h"

#include "chat/textstorage.h"
#include "renderer/worldmaprenderprovider.h"
#include "tibia/world/world.h"
#include "worldmap/numericaleffectstorage.h"

namespace tibia::renderer {

TNextFrameUpdater::TNextFrameUpdater(qmlcomponents::TWorldMapRenderProvider& provider, chat::TTextStorage& texts,
                                     worldmap::TNumericalEffectStorage& numericalEffects)
    : m_provider(provider)
    , m_texts(texts)
    , m_numericalEffects(numericalEffects)
{}

void TNextFrameUpdater::advance(const qint64 timestampMs)
{
    tibia::world::motion().advance(timestampMs);
    const LocalPlayerPtr player = tibia::world::player();
    m_texts.update(timestampMs, player ? player->getPosition() : Position());
    m_numericalEffects.update(timestampMs);
    m_provider.advance();
    tibia::world::motion().resume();
}

}
