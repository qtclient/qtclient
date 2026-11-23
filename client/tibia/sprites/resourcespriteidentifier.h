#pragma once

#include "sprites/spriteidentifier.h"

namespace tibia {
namespace sprites {

/// A sprite made from a client resource rather than the appearances (CIP
/// sprites::IResourceSpriteIdentifier): its kind is always a resource, and
/// `resourceKind` says which resource.
class IResourceSpriteIdentifier : public ISpriteIdentifier
{
public:
    /// What `resourceKind` returns.
    enum EResourceKind
    {
        ImageResource = 1,
        StaticMinimapMarkerSymbolResource = 2,
    };

    int identifierKind() const override { return ResourceIdentifier; }
    virtual int resourceKind() const = 0;
};

/// Two identifiers name different sprites: not the same dynamic type and kind,
/// or not equal (the negation of sameSpriteIdentifier).
inline bool differentSpriteIdentifier(const ISpriteIdentifier& a, const ISpriteIdentifier& b)
{
    return !sameSpriteIdentifier(a, b);
}

}
}
