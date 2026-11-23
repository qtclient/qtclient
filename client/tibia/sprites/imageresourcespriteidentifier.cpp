#include "sprites/imageresourcespriteidentifier.h"

#include <QHash>

namespace tibia::sprites {

std::shared_ptr<ISpriteIdentifier> TImageResourceSpriteIdentifier::clone() const
{
    return std::make_shared<TImageResourceSpriteIdentifier>(m_path, m_rect);
}

bool TImageResourceSpriteIdentifier::equals(const ISpriteIdentifier& other) const
{
    const auto& image = dynamic_cast<const TImageResourceSpriteIdentifier&>(other);
    return m_path == image.m_path && m_rect == image.m_rect;
}

size_t TImageResourceSpriteIdentifier::hash() const
{
    size_t seed = hashCombine(0, qHash(QStringView(m_path), 0));
    seed = hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_rect.left())));
    seed = hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_rect.top())));
    seed = hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_rect.width())));
    return hashCombine(seed, static_cast<size_t>(static_cast<int64_t>(m_rect.height())));
}

}
