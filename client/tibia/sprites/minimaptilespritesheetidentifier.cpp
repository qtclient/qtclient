#include "sprites/minimaptilespritesheetidentifier.h"

#include <bit>

namespace tibia::sprites {

uint32_t TMinimapTileSpritesheetIdentifier::tileIdHash(const Position& tileId)
{
    return static_cast<uint32_t>(tileId.z) * 0xf4265u ^ static_cast<uint32_t>(tileId.y) * 0x409u
        ^ static_cast<uint32_t>(tileId.x);
}

std::shared_ptr<ISpriteIdentifier> TMinimapTileSpritesheetIdentifier::clone() const
{
    return std::make_shared<TMinimapTileSpritesheetIdentifier>(m_scale, m_tileId, m_image);
}

bool TMinimapTileSpritesheetIdentifier::equals(const ISpriteIdentifier& other) const
{
    const auto& tile = dynamic_cast<const TMinimapTileSpritesheetIdentifier&>(other);
    return m_tileId == tile.m_tileId && m_scale == tile.m_scale;
}

size_t TMinimapTileSpritesheetIdentifier::hash() const
{
    size_t seed = hashCombine(0, tileIdHash(m_tileId));
    seed = hashCombine(seed, std::bit_cast<uint32_t>(m_scale + 0.f));
    return hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_image)));
}

}
