#include "sprites/spritesheetsmetadatamanager.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace tibia::sprites {

QSize spriteSizeOfLayout(const uint32_t layout)
{
    static constexpr std::array<std::pair<int, int>, 4> sizes = { { { 32, 32 }, { 32, 64 }, { 64, 32 }, { 64, 64 } } };
    if (layout < sizes.size())
        return QSize(sizes[layout].first, sizes[layout].second);
    return QSize(0, 0);
}

TSpritesheetMetadata TSpritesheetMetadata::invalid()
{
    return TSpritesheetMetadata(QStringLiteral("invalid_spritesheet_id"), 0xffffffffu, 0xffffffffu, 0);
}

TSpritesheetsMetadataManager::TSpritesheetsMetadataManager()
    : m_lastLookup(TSpritesheetMetadata::invalid())
{
}

void TSpritesheetsMetadataManager::addSpritesheet(const TSpritesheetID& spritesheetId, const uint32_t firstSpriteId,
                                                  const uint32_t lastSpriteId, const uint32_t layout)
{
    if (m_spritesheetIds.contains(spritesheetId))
        throw std::invalid_argument("Spritesheet already stored");
    m_spritesheetIds.insert(spritesheetId);
    // Before the first spritesheet that starts after this one's last sprite.
    const auto at = std::upper_bound(m_spritesheets.begin(), m_spritesheets.end(), lastSpriteId,
                                     [](const uint32_t last, const std::shared_ptr<TSpritesheetMetadata>& stored) {
                                         return last < stored->getFirstSpriteId();
                                     });
    m_spritesheets.insert(at, std::make_shared<TSpritesheetMetadata>(spritesheetId, firstSpriteId, lastSpriteId, layout));
}

std::vector<TSpritesheetID> TSpritesheetsMetadataManager::getSpritesheetIds() const
{
    std::vector<TSpritesheetID> ids;
    for (const auto& spritesheet : m_spritesheets)
        ids.push_back(spritesheet->spritesheetId);
    return ids;
}

TSpritesheetMetadata TSpritesheetsMetadataManager::getMetadataForSprite(const uint32_t spriteId)
{
    if (!m_lastLookup.contains(spriteId)) {
        const auto it = std::lower_bound(m_spritesheets.begin(), m_spritesheets.end(), spriteId,
                                         [](const std::shared_ptr<TSpritesheetMetadata>& stored, const uint32_t id) {
                                             return stored->getLastSpriteId() < id;
                                         });
        m_lastLookup = it == m_spritesheets.end() ? TSpritesheetMetadata::invalid() : **it;
    }
    return m_lastLookup;
}

void TSpritesheetsMetadataManager::clear()
{
    m_spritesheetIds.clear();
    m_spritesheets.clear();
    m_lastLookup = TSpritesheetMetadata::invalid();
}

TSpritesheetID TSpritesheetsMetadataManager::getSpritesheetIdForSprite(const uint32_t spriteId)
{
    return getMetadataForSprite(spriteId).spritesheetId;
}

}
