#pragma once

#include <QSize>
#include <QString>

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace sprites {

/// A spritesheet is identified by its asset file name.
using TSpritesheetID = QString;

/// Size of one sprite of a spritesheet layout (catalog "spritetype" 0..3);
/// an empty size for any other layout.
QSize spriteSizeOfLayout(uint32_t layout);

/// What the catalog says about one spritesheet: its file, the sprite ids it
/// holds and their layout.
struct TSpritesheetMetadata
{
    TSpritesheetID spritesheetId;
    uint32_t firstSpriteId = 0;
    uint32_t lastSpriteId = 0;
    uint32_t layout = 0;

    TSpritesheetMetadata() = default;
    TSpritesheetMetadata(TSpritesheetID id, uint32_t first, uint32_t last, uint32_t spriteLayout)
        : spritesheetId(std::move(id)), firstSpriteId(first), lastSpriteId(last), layout(spriteLayout)
    {
    }

    /// The metadata answered for a sprite no spritesheet holds.
    static TSpritesheetMetadata invalid();

    bool contains(uint32_t spriteId) const { return firstSpriteId <= spriteId && spriteId <= lastSpriteId; }
    uint32_t getFirstSpriteId() const { return firstSpriteId; }
    uint32_t getLastSpriteId() const { return lastSpriteId; }
    uint32_t getLayout() const { return layout; }
    bool isValid() const { return spritesheetId != invalid().spritesheetId; }
};

/// The spritesheets of the asset catalog, sorted by sprite id
/// (CIP sprites::TSpritesheetsMetadataManager).
class TSpritesheetsMetadataManager
{
public:
    TSpritesheetsMetadataManager();
    virtual ~TSpritesheetsMetadataManager() = default;

    /// Adds a spritesheet; one whose file is already stored throws std::invalid_argument.
    virtual void addSpritesheet(const TSpritesheetID& spritesheetId, uint32_t firstSpriteId, uint32_t lastSpriteId,
                                uint32_t layout);
    /// Every spritesheet, in sprite id order.
    virtual std::vector<TSpritesheetID> getSpritesheetIds() const;
    /// The spritesheet that holds a sprite: the first one whose last sprite id is
    /// not below it (remembered for the next lookup), or invalid() past the last one.
    virtual TSpritesheetMetadata getMetadataForSprite(uint32_t spriteId);

    void clear();

private:
    std::set<TSpritesheetID> m_spritesheetIds;
    std::vector<std::shared_ptr<TSpritesheetMetadata>> m_spritesheets;
    TSpritesheetMetadata m_lastLookup;
};

}
}
