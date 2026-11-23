#include "sprites/staticminimapmarkersymbolresourcespriteidentifier.h"

namespace tibia::sprites {

std::shared_ptr<ISpriteIdentifier> TStaticMinimapMarkerSymbolResourceSpriteIdentifier::clone() const
{
    return std::make_shared<TStaticMinimapMarkerSymbolResourceSpriteIdentifier>(m_symbol);
}

bool TStaticMinimapMarkerSymbolResourceSpriteIdentifier::equals(const ISpriteIdentifier& other) const
{
    return m_symbol == dynamic_cast<const TStaticMinimapMarkerSymbolResourceSpriteIdentifier&>(other).m_symbol;
}

size_t TStaticMinimapMarkerSymbolResourceSpriteIdentifier::hash() const
{
    return hashCombine(0, m_symbol);
}

}
