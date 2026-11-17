#include "sprites/spriteidentifier.h"

#include <QHash>

#include <typeinfo>

namespace tibia::sprites {

size_t hashCombine(const size_t seed, const size_t value)
{
    uint64_t x = seed + 0x9e3779b9 + value;
    x = (x >> 32 ^ x) * 0xe9846af9b1a615dULL;
    x = (x >> 32 ^ x) * 0xe9846af9b1a615dULL;
    return x >> 28 ^ x;
}

bool sameSpriteIdentifier(const ISpriteIdentifier& a, const ISpriteIdentifier& b)
{
    return typeid(a) == typeid(b) && a.identifierKind() == b.identifierKind() && a.equals(b);
}

bool TAssetSpritesheetIdentifier::equals(const ISpriteIdentifier& other) const
{
    return m_spritesheetId == static_cast<const TAssetSpritesheetIdentifier&>(other).m_spritesheetId;
}

size_t TAssetSpritesheetIdentifier::hash() const
{
    return hashCombine(0, qHash(m_spritesheetId));
}

bool TEffectWrapperSpritesheetIdentifier::equals(const ISpriteIdentifier& other) const
{
    const auto& wrapper = static_cast<const TEffectWrapperSpritesheetIdentifier&>(other);
    if (!m_wrapped || !wrapper.m_wrapped)
        return !m_wrapped && !wrapper.m_wrapped && m_color == wrapper.m_color && m_effect == wrapper.m_effect;
    return sameSpriteIdentifier(*wrapper.m_wrapped, *m_wrapped) && wrapper.m_color == m_color
        && wrapper.m_effect == m_effect;
}

size_t TEffectWrapperSpritesheetIdentifier::hash() const
{
    size_t seed = hashCombine(0, static_cast<size_t>(static_cast<int64_t>(m_wrapped ? m_wrapped->identifierKind() : 0)));
    seed = hashCombine(seed, m_wrapped ? m_wrapped->hash() : 0);
    seed = hashCombine(seed, m_color.rgba());
    return hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_effect)));
}

}
