#include "appearances/creatureplaceholderappearanceinstance.h"

#include "appearances/appearancetype.h"
#include "creatures/creaturestorage.h"

namespace tibia::appearances {

TCreaturePlaceholderAppearanceInstance::TCreaturePlaceholderAppearanceInstance(std::weak_ptr<creatures::TCreature> creature)
    : TAppearanceInstance(TAppearanceInstanceFactory::dummyAppearanceType(ThingCategoryCreature), 0)
    , m_creature(std::move(creature))
{
}

bool TCreaturePlaceholderAppearanceInstance::updateAnimation(const qint64 nowMs)
{
    if (const std::shared_ptr<creatures::TCreature> creature = this->creature()) {
        creature->update(nowMs);
        m_lastUpdateMs = nowMs;
        if (creature->isMoving()) {
            m_lastMovingMs = nowMs;
            m_movingSpriteSuppressed = false;
        }
    }
    return true;
}

worldmap::TWorldMapSubfieldExtent TCreaturePlaceholderAppearanceInstance::movementOffset() const
{
    if (const std::shared_ptr<creatures::TCreature> creature = this->creature())
        return creature->movementOffset();
    return { 0, 0, 0 };
}

worldmap::TWorldMapSubfieldExtent TCreaturePlaceholderAppearanceInstance::creatureDisplacement() const
{
    const std::shared_ptr<creatures::TCreature> creature = this->creature();
    if (!creature)
        return { 0, 0, 0 };
    // Host: qtclient keeps no mount instance; the mount's displacement is its
    // appearance type's (the instance adds only its elevation as depth).
    worldmap::TWorldMapSubfieldExtent mount{ 0, 0, 0 };
    if (creature->isMounted()) {
        const TAppearanceType* type =
            TAppearanceInstanceFactory::appearanceType(ThingCategoryCreature, creature->outfit().getMount());
        mount = { type->getDisplacementX(), type->getDisplacementY(), 0 };
    }
    worldmap::TWorldMapSubfieldExtent outfit{ 0, 0, 0 };
    // Host: a creature without a known outfit has no outfit instance (or one
    // without a type); it is not displaced.
    if (const TAppearanceInstancePtr instance = creature->outfitAppearance(); instance && instance->thingType())
        outfit = instance->displacement();
    const worldmap::TWorldMapSubfieldExtent& drawn = creature->isMounted() ? mount : outfit;
    return { drawn.width, drawn.height, creature->drawElevation() };
}

bool TCreaturePlaceholderAppearanceInstance::showsMovingSprite() const
{
    const std::shared_ptr<creatures::TCreature> creature = this->creature();
    if (!creature)
        return false;
    if (!creature->isLocalPlayer())
        return creature->isMoving();
    if (creature->isMoving() || (m_lastUpdateMs <= m_lastMovingMs + 75 && m_holdsMovementIntent))
        return !m_movingSpriteSuppressed;
    return false;
}

} // namespace tibia::appearances
