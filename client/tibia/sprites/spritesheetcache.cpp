#include "sprites/spritesheetcache.h"

namespace tibia::sprites {

TSpritesheetCache::TSpritesheetCache(const uint32_t capacity)
    : m_capacity(capacity)
{
}

void TSpritesheetCache::insert(const TSpritesheetID& spritesheetId, const TSpritesheetPointer& spritesheet)
{
    if (m_entries.size() == m_capacity && !m_entries.empty()) {
        m_index.erase(m_entries.back().spritesheetId);
        m_entries.pop_back();
    }
    if (m_index.contains(spritesheetId))
        return;
    m_entries.push_front(Entry{ spritesheetId, spritesheet });
    m_index.emplace(spritesheetId, m_entries.begin());
}

TSpritesheetPointer TSpritesheetCache::get(const TSpritesheetID& spritesheetId)
{
    const auto it = m_index.find(spritesheetId);
    if (it == m_index.end())
        return nullptr;
    m_entries.splice(m_entries.begin(), m_entries, it->second);
    return it->second->spritesheet;
}

}
