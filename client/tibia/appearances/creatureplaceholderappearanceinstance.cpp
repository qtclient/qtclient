#include "appearances/creatureplaceholderappearanceinstance.h"

#include "appearances/appearancetype.h"
#include "creatures/creaturestorage.h"

namespace tibia::appearances {

namespace {

/// Every placeholder's type: outfit 99 with one empty frame group (the
/// creature draws itself; the type is never looked up).
TAppearanceType* placeholderAppearanceType()
{
    static TAppearanceType type = [] {
        TAppearanceType placeholder;
        placeholder.setCategory(EAppearanceCategory::Outfit);
        placeholder.setId(99);
        placeholder.addFrameGroup(EFixedFrameGroup::ObjectInitial, TAppearanceTypeSpriteInfo());
        placeholder.completeFrameGroups();
        return placeholder;
    }();
    return &type;
}

} // namespace

TCreaturePlaceholderAppearanceInstance::TCreaturePlaceholderAppearanceInstance(std::weak_ptr<creatures::TCreature> creature)
    : TAppearanceInstance(placeholderAppearanceType(), 0)
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
    // A mounted creature is displaced like its mount, else like its outfit;
    // the depth is its elevation.
    worldmap::TWorldMapSubfieldExtent mount{ 0, 0, 0 };
    if (creature->isMounted()) {
        if (const std::shared_ptr<TOutfitAppearanceInstance> instance = creature->mountAppearance(); instance && instance->thingType())
            mount = instance->displacement();
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

void TCreaturePlaceholderAppearanceInstance::appendRenderInfos(const worldmap::TWorldMapCoordinate& coordinate,
                                                               renderer::TRenderInfoList& infos) const
{
    const std::shared_ptr<creatures::TCreature> creature = this->creature();
    if (!creature)
        return;
    const creatures::TCreatureMovementTween& tween = creature->movementTween();
    worldmap::TWorldMapCoordinate field = coordinate + tween.coveredFieldOffset();
    worldmap::TWorldMapSubfieldExtent offset = tween.drawOffset();
    if (offset.width > 0) {
        offset.width -= 32;
        field.x += 1;
    }
    if (offset.height > 0) {
        offset.height -= 32;
        field.y += 1;
    }
    if (creature->hasMarks()) {
        infos.push_back({ .type = m_type,
                          .spriteInfo = std::make_shared<TCreatureMarkAppearanceInstanceSpriteInfo>(creature->shownMarks()),
                          .coordinate = field, .displacement = creatureDisplacement(), .movementOffset = offset,
                          .kind = renderer::TRenderInfo::Self, .isCreature = true });
    }
    const bool moving = showsMovingSprite();
    if (creature->isMounted()) {
        if (const std::shared_ptr<TOutfitAppearanceInstance> mount = creature->mountAppearance()) {
            infos.push_back({ .type = m_type, .spriteInfo = moving ? mount->movingSpriteInfo() : mount->idleSpriteInfo(),
                              .coordinate = field, .displacement = creatureDisplacement(), .movementOffset = offset,
                              .kind = renderer::TRenderInfo::FigureMount, .isCreature = true });
        }
    }
    // Host: qtclient's creature keeps its outfit as a plain instance; only an
    // outfit instance has idle and moving sprites.
    if (const auto outfit = std::dynamic_pointer_cast<TOutfitAppearanceInstance>(creature->outfitAppearance())) {
        infos.push_back({ .type = m_type, .spriteInfo = moving ? outfit->movingSpriteInfo() : outfit->idleSpriteInfo(),
                          .coordinate = field, .displacement = creatureDisplacement(), .movementOffset = offset,
                          .kind = renderer::TRenderInfo::FigureOutfit, .isCreature = true });
    }
}

} // namespace tibia::appearances
