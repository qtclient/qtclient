#include "appearances/appearanceinstancetracker.h"

#include "appearances/appearancetype.h"
#include "sprites/appearancetypespritesheetmapping.h"

#include <QDebug>
#include <QMetaType>

#include <algorithm>
#include <vector>

namespace tibia::appearances {

TAppearanceInstanceTracker::TAppearanceInstanceTracker(std::shared_ptr<sprites::TAppearanceTypeSpritesheetMapping> mapping)
    : m_mapping(std::move(mapping))
{
    qRegisterMetaType<sprites::TSpritesheetID>("tibia::sprites::TSpritesheetID");
    for (const EAppearanceCategory category :
         { EAppearanceCategory::Effect, EAppearanceCategory::Missile, EAppearanceCategory::Object, EAppearanceCategory::Outfit })
        m_instanceCounts[category];
}

TAppearanceInstanceTracker::~TAppearanceInstanceTracker() = default;

void TAppearanceInstanceTracker::instanceCreated(const TAppearanceType& type)
{
    int previous = 0;
    {
        std::lock_guard lock(m_instanceCountsMutex);
        previous = m_instanceCounts[type.category()][type.getId()]++;
    }
    if (previous != 0)
        return;
    const std::set<sprites::TSpritesheetID>& spritesheetIds = m_mapping->spritesheetIdsOf(type);
    for (const sprites::TSpritesheetID& spritesheetId : spritesheetIds)
        Q_EMIT loadSpritesheetWithID(spritesheetId);
    std::lock_guard lock(m_spritesheetCountsMutex);
    for (const sprites::TSpritesheetID& spritesheetId : spritesheetIds)
        ++m_spritesheetCounts[spritesheetId];
}

void TAppearanceInstanceTracker::loadSpritesheetsOf(const std::set<const TAppearanceType*>& types)
{
    std::vector<sprites::TSpritesheetID> spritesheetIds;
    for (const TAppearanceType* type : types) {
        const std::set<sprites::TSpritesheetID>& ofType = m_mapping->spritesheetIdsOf(*type);
        spritesheetIds.insert(spritesheetIds.end(), ofType.begin(), ofType.end());
    }
    std::sort(spritesheetIds.begin(), spritesheetIds.end());
    spritesheetIds.erase(std::unique(spritesheetIds.begin(), spritesheetIds.end()), spritesheetIds.end());
    for (const sprites::TSpritesheetID& spritesheetId : spritesheetIds)
        Q_EMIT loadSpritesheetWithID(spritesheetId);
}

void TAppearanceInstanceTracker::instanceDestroyed(const TAppearanceType& type)
{
    instanceDestroyed(type.category(), type.getId());
}

void TAppearanceInstanceTracker::instanceDestroyed(const EAppearanceCategory category, const uint16_t id)
{
    {
        std::lock_guard lock(m_instanceCountsMutex);
        int& count = m_instanceCounts[category][id];
        if (count == 0) {
            qWarning() << "Tried to decrease instance count of " << static_cast<int>(category) << id << " below zero.";
            return;
        }
        if (--count != 0)
            return;
    }
    const std::set<sprites::TSpritesheetID>& spritesheetIds = m_mapping->spritesheetIdsOf(category, id);
    std::lock_guard lock(m_spritesheetCountsMutex);
    for (const sprites::TSpritesheetID& spritesheetId : spritesheetIds) {
        if (--m_spritesheetCounts[spritesheetId] == 0)
            m_spritesheetCounts.erase(spritesheetId);
    }
}

int TAppearanceInstanceTracker::instanceCount(const TAppearanceType& type)
{
    std::lock_guard lock(m_instanceCountsMutex);
    return m_instanceCounts[type.category()][type.getId()];
}

}
