#pragma once

#include "sprites/resourcespriteidentifier.h"

#include <cstdint>

namespace tibia {
namespace sprites {

/// The sprite of a minimap marker symbol (CIP
/// sprites::TStaticMinimapMarkerSymbolResourceSpriteIdentifier).
class TStaticMinimapMarkerSymbolResourceSpriteIdentifier final : public IResourceSpriteIdentifier
{
public:
    explicit TStaticMinimapMarkerSymbolResourceSpriteIdentifier(uint8_t symbol)
        : m_symbol(symbol)
    {
    }

    std::shared_ptr<ISpriteIdentifier> clone() const override;
    /// Same symbol.
    bool equals(const ISpriteIdentifier& other) const override;
    size_t hash() const override;
    int resourceKind() const override { return StaticMinimapMarkerSymbolResource; }

    uint8_t symbol() const { return m_symbol; }

private:
    uint8_t m_symbol = 0;
};

}
}
