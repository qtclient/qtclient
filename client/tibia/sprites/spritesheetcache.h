#pragma once

#include "sprites/spritesheetsmetadatamanager.h"

#include <QImage>

#include <list>
#include <memory>
#include <unordered_map>

namespace tibia {
namespace sprites {

using TSpritesheetPointer = std::shared_ptr<QImage>;

/// Decompressed spritesheets, least recently used dropped first once the
/// capacity is reached (CIP sprites::TSpritesheetCache).
class TSpritesheetCache
{
public:
    explicit TSpritesheetCache(uint32_t capacity);
    virtual ~TSpritesheetCache() = default;

    /// Stores a spritesheet as the most recently used one; a full cache drops its
    /// least recently used spritesheet first. An id already stored keeps its image.
    virtual void insert(const TSpritesheetID& spritesheetId, const TSpritesheetPointer& spritesheet);
    /// The stored spritesheet (now the most recently used one), or null.
    virtual TSpritesheetPointer get(const TSpritesheetID& spritesheetId);

    uint32_t capacity() const { return m_capacity; }
    size_t size() const { return m_entries.size(); }
    bool contains(const TSpritesheetID& spritesheetId) const { return m_index.contains(spritesheetId); }

private:
    struct Entry
    {
        TSpritesheetID spritesheetId;
        TSpritesheetPointer spritesheet;
    };
    struct Hash
    {
        size_t operator()(const QString& id) const noexcept { return qHash(id); }
    };

    uint32_t m_capacity = 0;
    /// Most recently used first.
    std::list<Entry> m_entries;
    std::unordered_map<TSpritesheetID, std::list<Entry>::iterator, Hash> m_index;
};

}
}
