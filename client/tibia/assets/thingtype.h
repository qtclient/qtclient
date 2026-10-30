#pragma once


#include <client/animator.h>
#include <client/const.h>
#include <client/declarations.h>
#include <client/heightstamp.h>
#include <client/staticdata.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// Appearance record filled from CIP appearances.dat. ThingType is the thin
/// view the rest of the client already calls.
struct AppearanceRecord
{
    uint16_t id = 0;
    ThingCategory category = ThingInvalidCategory;
    bool null = true;
    std::string name;
    std::string description;
    uint64_t flags = 0;
    Size size{ 1, 1 };
    Point displacement;
    Light light;
    MarketData market{};
    std::vector<NPCData> npcSale;
    std::vector<Rect> boundingBoxes;
    std::vector<uint32_t> sprites;
    std::vector<uint32_t> restrictVocation;
    int numPatternX = 1;
    int numPatternY = 1;
    int numPatternZ = 1;
    int layers = 1;
    int animationPhases = 1;
    int elevation = 0;
    int groundSpeed = 0;
    int maxTextLength = 0;
    int minimapColor = 0;
    int lensHelp = 0;
    int clothSlot = 0;
    int upgradeClassification = 0;
    int realSize = 0;
    int8_t opaque = -1;
    uint32_t cyclopediaType = 0;
    uint32_t proficiencyId = 0;
    uint32_t weaponType = 0;
    uint32_t appearanceWeaponType = 0;
    uint32_t minimumLevel = 0;
    uint32_t imbueSlots = 0;
    PLAYER_ACTION defaultAction = PLAYER_ACTION_NONE;
    uint32_t gemQualityId = 0;
    uint32_t gemVocationId = 0;
    float opacity = 1.f;
};

class ThingType
{
public:
    struct SkillWheelGem
    {
        uint32_t gem_quality_id = 0;
        uint32_t vocation_id = 0;
        uint32_t getGemQualityId() const { return gem_quality_id; }
        uint32_t getVocationId() const { return vocation_id; }
    };

    void adopt(AppearanceRecord record);
    void setAnimators(std::unique_ptr<Animator> moving, std::unique_ptr<Animator> idle);

    uint16_t getId() const { return m_record.id; }
    ThingCategory getCategory() const { return m_record.category; }
    bool isNull() const { return m_record.null; }
    bool hasAttr(ThingAttr attr) const;

    int getWidth() const { return m_record.size.width(); }
    int getHeight() const { return m_record.size.height(); }
    int getExactSize(int layer = 0, int xPattern = 0, int yPattern = 0, int zPattern = 0, int animationPhase = 0);
    int getRealSize() const { return m_record.realSize; }
    int getLayers() const { return m_record.layers; }
    int getNumPatternX() const { return m_record.numPatternX; }
    int getNumPatternY() const { return m_record.numPatternY; }
    int getNumPatternZ() const { return m_record.numPatternZ; }
    int getAnimationPhases() const;
    int getIdleAnimationPhases() const;
    Animator* getAnimator() const { return m_moving.get(); }
    Animator* getIdleAnimator() const { return m_idle.get(); }

    const Size& getSize() const { return m_record.size; }
    const Point& getDisplacement() const { return m_record.displacement; }
    const Light& getLight() const { return m_record.light; }
    const MarketData& getMarketData() const { return m_record.market; }
    const std::vector<NPCData>& getNpcSaleData() const { return m_record.npcSale; }
    int getMeanPrice() const;

    int getDisplacementX() const { return m_record.displacement.x; }
    int getDisplacementY() const { return m_record.displacement.y; }
    int getElevation() const { return m_record.elevation; }
    uint16_t getGroundSpeed() const { return static_cast<uint16_t>(m_record.groundSpeed); }
    int getMaxTextLength() const { return m_record.maxTextLength; }
    int getMinimapColor() const { return m_record.minimapColor; }
    int getLensHelp() const { return m_record.lensHelp; }
    int getClothSlot() const { return m_record.clothSlot; }

    bool isGround() const { return m_record.flags & ThingFlagAttrGround; }
    bool isGroundBorder() const { return m_record.flags & ThingFlagAttrGroundBorder; }
    bool isOnBottom() const { return m_record.flags & ThingFlagAttrOnBottom; }
    bool isOnTop() const { return m_record.flags & ThingFlagAttrOnTop; }
    bool isContainer() const { return m_record.flags & ThingFlagAttrContainer; }
    bool isStackable() const { return m_record.flags & ThingFlagAttrStackable; }
    bool isForceUse() const { return m_record.flags & ThingFlagAttrForceUse; }
    bool isMultiUse() const { return m_record.flags & ThingFlagAttrMultiUse; }
    bool isWritable() const { return m_record.flags & ThingFlagAttrWritable; }
    bool isChargeable() const { return m_record.flags & ThingFlagAttrChargeable; }
    bool isWritableOnce() const { return m_record.flags & ThingFlagAttrWritableOnce; }
    bool isFluidContainer() const { return m_record.flags & ThingFlagAttrFluidContainer; }
    bool isSplash() const { return m_record.flags & ThingFlagAttrSplash; }
    bool isNotWalkable() const { return m_record.flags & ThingFlagAttrNotWalkable; }
    bool isNotMoveable() const { return m_record.flags & ThingFlagAttrNotMoveable; }
    bool blockProjectile() const { return m_record.flags & ThingFlagAttrBlockProjectile; }
    bool isNotPathable() const { return m_record.flags & ThingFlagAttrNotPathable; }
    bool isPickupable() const { return m_record.flags & ThingFlagAttrPickupable; }
    bool isHangable() const { return m_record.flags & ThingFlagAttrHangable; }
    bool isHookSouth() const { return m_record.flags & ThingFlagAttrHookSouth; }
    bool isHookEast() const { return m_record.flags & ThingFlagAttrHookEast; }
    bool isRotateable() const { return m_record.flags & ThingFlagAttrRotateable; }
    bool hasLight() const { return m_record.flags & ThingFlagAttrLight; }
    bool isDontHide() const { return m_record.flags & ThingFlagAttrDontHide; }
    bool isTranslucent() const { return m_record.flags & ThingFlagAttrTranslucent; }
    bool hasDisplacement() const { return m_record.flags & ThingFlagAttrDisplacement; }
    bool hasElevation() const { return m_record.flags & ThingFlagAttrElevation; }
    bool hasFloorChange() const { return m_record.flags & ThingFlagAttrFloorChange; }
    bool isLyingCorpse() const { return m_record.flags & ThingFlagAttrLyingCorpse; }
    bool isAnimateAlways() const { return m_record.flags & ThingFlagAttrAnimateAlways; }
    bool hasMiniMapColor() const { return m_record.flags & ThingFlagAttrMinimapColor; }
    bool hasLensHelp() const { return m_record.flags & ThingFlagAttrLensHelp; }
    bool isFullGround() const { return m_record.flags & ThingFlagAttrFullGround; }
    bool isIgnoreLook() const { return m_record.flags & ThingFlagAttrLook; }
    bool isCloth() const { return m_record.flags & ThingFlagAttrCloth; }
    bool isMarketable() const { return m_record.flags & ThingFlagAttrMarket; }
    bool isUsable() const { return m_record.flags & ThingFlagAttrUsable; }
    bool isWrapable() const { return m_record.flags & ThingFlagAttrWrapable; }
    bool isUnwrapable() const { return m_record.flags & ThingFlagAttrUnwrapable; }
    bool hasWearOut() const { return m_record.flags & ThingFlagAttrWearOut; }
    bool hasClockExpire() const { return m_record.flags & ThingFlagAttrClockExpire; }
    bool hasExpire() const { return m_record.flags & ThingFlagAttrExpire; }
    bool hasExpireStop() const { return m_record.flags & ThingFlagAttrExpireStop; }
    bool isPodium() const { return m_record.flags & ThingFlagAttrPodium; }
    bool isTopEffect() const { return m_record.flags & ThingFlagAttrTopEffect; }
    bool hasAction() const { return m_record.flags & ThingFlagAttrDefaultAction; }
    bool isCorpse() const { return m_record.flags & ThingFlagAttrCorpse; }
    bool isPlayerCorpse() const { return m_record.flags & ThingFlagAttrPlayerCorpse; }
    bool isReportable() const { return m_record.flags & ThingFlagAttrReportable; }
    bool hasNoMovementAnimation() const { return m_record.flags & ThingFlagAttrNoMovementAnimation; }
    bool hasCyclopediaItem() const { return m_record.flags & ThingFlagAttrCyclopediaItem; }
    bool isOpaque() const { return m_record.opaque == 1; }
    bool isDecoKit() const { return m_record.flags & ThingFlagAttrDecoKit; }
    bool isAmmo() const { return m_record.flags & ThingFlagAttrAmmo; }
    bool isDualWield() const { return m_record.flags & ThingFlagAttrDualWield; }
    bool hasSkillWheelGem() const { return m_record.flags & ThingFlagAttrSkillWheelGem; }
    bool hasProficiency() const { return m_record.flags & ThingFlagAttrProficiency; }
    bool isItem() const { return m_record.category == ThingCategoryItem; }
    bool isEffect() const { return m_record.category == ThingCategoryEffect; }
    bool isMissile() const { return m_record.category == ThingCategoryMissile; }
    bool isCreature() const { return m_record.category == ThingCategoryCreature; }
    bool isTopGround() const { return isGround() && m_record.size.dimension() == 4; }
    bool isTopGroundBorder() const { return isGroundBorder() && m_record.size.dimension() == 4; }
    bool isSingleDimension() const { return m_record.size.area() == 1; }
    bool isSingleGround() const { return isGround() && isSingleDimension(); }
    bool isSingleGroundBorder() const { return isGroundBorder() && isSingleDimension(); }
    bool isTall(bool useRealSize = false) const;
    bool isLoading() const;

    uint32_t getSkillWheelGemQualityId() const { return m_record.gemQualityId; }
    uint32_t getSkillWheelGemVocationId() const { return m_record.gemVocationId; }
    uint32_t getCyclopediaType() const { return m_record.cyclopediaType; }
    uint32_t getProficiencyId() const { return m_record.proficiencyId; }
    uint32_t getWeaponType() const { return m_record.weaponType; }
    uint32_t getAppearanceWeaponType() const { return m_record.appearanceWeaponType; }
    uint32_t getMinimumLevel() const { return m_record.minimumLevel; }
    uint32_t getImbueSlots() const { return m_record.imbueSlots; }
    const std::vector<uint32_t>& getRestrictVocation() const { return m_record.restrictVocation; }
    SkillWheelGem getSkillWheelGem() const { return { m_record.gemQualityId, m_record.gemVocationId }; }
    PLAYER_ACTION getDefaultAction() const { return m_record.defaultAction; }
    uint16_t getClassification() const { return static_cast<uint16_t>(m_record.upgradeClassification); }
    const std::vector<uint32_t>& getSprites() const { return m_record.sprites; }
    uint32_t getSpriteIndex(int w, int h, int l, int x, int y, int z, int a) const;
    const std::vector<Rect>& getBoundingBoxes() const { return m_record.boundingBoxes; }
    float getOpacity() const { return m_record.opacity; }
    std::string getName() const { return m_record.name; }
    std::string getDescription() const { return m_record.description; }

    bool castsShadow();
    void setThingShape(ThingShape shape) { m_thingShape.store(shape, std::memory_order_relaxed); }
    ThingShape getThingShape() const { return m_thingShape.load(std::memory_order_relaxed); }
    const AppearanceRecord& record() const { return m_record; }

private:
    bool castsShadowByFlags() const;

    AppearanceRecord m_record;
    std::unique_ptr<Animator> m_moving;
    std::unique_ptr<Animator> m_idle;
    std::atomic<ThingShape> m_thingShape{ ThingShape::Auto };
    std::atomic<int8_t> m_castsByFlags{ -1 };
};
