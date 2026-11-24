#pragma once

#include "animation/animationinfo.h"

#include <client/const.h>
#include <client/declarations.h>
#include <client/staticdata.h>

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tibia::protobuf::appearances {
class Appearance;
class SpriteInfo;
}

namespace tibia::appearances {

/// The four banks of appearances.dat; the numbers are CipSoft's.
enum class EAppearanceCategory : int { Object = 0, Outfit = 1, Effect = 2, Missile = 3 };

/// A frame group of an appearance. CipSoft renumbers the protobuf values:
/// object initial 0, outfit idle 1, outfit moving 2, anything else 3.
enum class EFixedFrameGroup : int { ObjectInitial = 0, OutfitIdle = 1, OutfitMoving = 2, Invalid = 3 };

/// The objects appearances.dat names for the client (coins, letter, stash, …).
struct TSpecialMeaningAppearanceIds
{
    uint16_t goldCoin = 0;
    uint16_t platinumCoin = 0;
    uint16_t crystalCoin = 0;
    uint16_t tibiaCoin = 0;
    uint16_t stampedLetter = 0;
    uint16_t supplyStash = 0;
    uint16_t standardRewardChest = 0;
    uint16_t blankImbuementScroll = 0;
};

/// The special ids of the loaded appearances (all 0 before).
const TSpecialMeaningAppearanceIds& specialMeaningAppearanceIds();
void setSpecialMeaningAppearanceIds(const TSpecialMeaningAppearanceIds& ids);

/// The fixed frame group a protobuf FIXED_FRAME_GROUP value stands for.
EFixedFrameGroup fixedFrameGroupFromProtobuf(uint32_t value);


/// How the frames of one frame group are laid out and animated.
/// The pixel size of a sprite, as the spritesheets know it (empty when unknown).
using TSpriteSizeLookup = std::function<QSize(uint32_t spriteId)>;

class TAppearanceTypeSpriteInfo
{
public:
    TAppearanceTypeSpriteInfo() = default;
    /// Builds the frame group from its protobuf description; the sprites' pixel
    /// size is the one of the first sprite.
    static TAppearanceTypeSpriteInfo fromProtobuf(const tibia::protobuf::appearances::SpriteInfo& message,
                                                  const TSpriteSizeLookup& spriteSize);

    /// The sprite count is a non-zero multiple of the patterns times the layers.
    bool isValid() const;

    uint32_t patternWidth() const { return m_patternWidth; }
    uint32_t patternHeight() const { return m_patternHeight; }
    uint32_t patternDepth() const { return m_patternDepth; }
    uint32_t layers() const { return m_layers; }
    /// The sprites of one phase: one per pattern and layer.
    uint32_t spritesPerPhase() const { return m_patternWidth * m_patternHeight * m_patternDepth * m_layers; }
    /// The index of a sprite in a group of `width` × `height` × `depth`
    /// patterns and `layers` layers: phases outermost, then z, y, x and layer.
    static int spriteIndex(int width, int height, int depth, int layers, int x, int y, int z, int layer, int phase)
    {
        return (((phase * depth + z) * height + y) * width + x) * layers + layer;
    }
    /// The bounding box per direction (one per x pattern at least).
    const std::vector<QRect>& boundingBoxes() const { return m_boundingBoxes; }
    const std::vector<uint32_t>& spriteIds() const { return m_spriteIds; }
    /// The pixel size of the group's sprites (from its first sprite).
    QSize spriteSize() const { return m_spriteSize; }
    bool isAnimated() const { return m_animated; }
    bool isOpaque() const { return m_opaque; }
    /// The animation; null when the group does not animate.
    const tibia::animation::TAnimationInfo* animation() const { return m_animation ? &*m_animation : nullptr; }
    void setAnimation(std::optional<tibia::animation::TAnimationInfo> animation);

    /// The frames of one pattern: the animation's phases, else 1.
    int phaseCount() const;
    /// The sprite of layer/pattern/phase; patterns and phase past the last one
    /// use the last one, a missing layer has none.
    std::optional<uint32_t> spriteId(int layer, int x, int y, int z, int phase) const;

private:
    uint32_t m_patternWidth = 1;
    uint32_t m_patternHeight = 1;
    uint32_t m_patternDepth = 1;
    uint32_t m_layers = 1;
    std::vector<QRect> m_boundingBoxes;
    std::vector<uint32_t> m_spriteIds;
    bool m_animated = false;
    bool m_opaque = false;
    std::optional<tibia::animation::TAnimationInfo> m_animation;
    QSize m_spriteSize;
};

/// What the market shows for an object it trades under the object's own id.
struct TAppearanceMarketProperties
{
    QString name;
    int category = 0;
    uint16_t showAs = 0;
};

/// An NPC that buys or sells an object, with every place it does.
struct TNpcTradeData
{
    QString name;
    QStringList locations;
    uint32_t salePrice = 0;
    uint32_t buyPrice = 0;
    /// The currency when it is not gold: an object id or a quest flag name.
    std::optional<std::variant<uint16_t, QString>> currency;

    uint16_t currencyObjectId() const;
    QString currencyQuestFlagName() const;
};

/// One appearance of appearances.dat with what CipSoft keeps of it.
class TAppearanceType
{
public:
    TAppearanceType() = default;
    /// Converts one protobuf appearance of `category`.
    static TAppearanceType fromProtobuf(EAppearanceCategory category,
                                        const tibia::protobuf::appearances::Appearance& appearance,
                                        const TSpriteSizeLookup& spriteSize);

    // Identity.
    uint16_t getId() const { return m_id; }
    EAppearanceCategory category() const { return m_category; }
    ThingCategory getCategory() const { return m_null ? ThingInvalidCategory : static_cast<ThingCategory>(m_category); }
    bool isNull() const { return m_null; }
    bool isItem() const { return !m_null && m_category == EAppearanceCategory::Object; }
    bool isCreature() const { return !m_null && m_category == EAppearanceCategory::Outfit; }
    bool isEffect() const { return !m_null && m_category == EAppearanceCategory::Effect; }
    bool isMissile() const { return !m_null && m_category == EAppearanceCategory::Missile; }
    const QString& name() const { return m_name; }
    const QString& description() const { return m_description; }
    std::string getName() const { return m_name.toStdString(); }
    std::string getDescription() const { return m_description.toStdString(); }

    // Flags.
    bool isGround() const { return m_bank; }
    uint16_t getGroundSpeed() const { return m_waypoints; }
    bool isGroundBorder() const { return m_clip; }
    bool isOnBottom() const { return m_bottom; }
    bool isOnTop() const { return m_top; }
    bool isContainer() const { return m_container; }
    bool isStackable() const { return m_cumulative; }
    bool isForceUse() const { return m_forceUse; }
    bool isMultiUse() const { return m_multiUse; }
    bool isWritable() const { return m_write; }
    bool isWritableOnce() const { return m_writeOnce; }
    bool isReportable() const { return m_reportable; }
    bool isWrapable() const { return m_wrap; }
    bool isUnwrapable() const { return m_unwrap; }
    bool isTopEffect() const { return m_topEffect; }
    bool isCorpse() const { return m_corpse; }
    bool isPlayerCorpse() const { return m_playerCorpse; }
    bool isSplash() const { return m_liquidPool; }
    bool isFluidContainer() const { return m_liquidContainer; }
    bool isNotWalkable() const { return m_unpass; }
    bool isNotMoveable() const { return m_unmove; }
    bool blockProjectile() const { return m_unsight; }
    bool isNotPathable() const { return m_avoid; }
    bool hasNoMovementAnimation() const { return m_noMovementAnimation; }
    bool isPickupable() const { return m_take; }
    bool isHangable() const { return m_hang; }
    bool isAmmo() const { return m_ammo; }
    bool isDualWield() const { return m_dualWielding; }
    bool isHookSouth() const { return m_hookDirection == HookSouth; }
    bool isHookEast() const { return m_hookDirection == HookEast; }
    bool isRotateable() const { return m_rotate; }
    bool hasLight() const { return m_hasLight; }
    const Light& getLight() const { return m_light; }
    bool isDontHide() const { return m_dontHide; }
    bool isTranslucent() const { return m_translucent; }
    bool hasElevation() const { return m_elevation != 0; }
    int getElevation() const { return m_elevation; }
    bool hasDisplacement() const { return m_displacement != QPoint(); }
    const QPoint& getDisplacement() const { return m_displacement; }
    int getDisplacementX() const { return m_displacement.x(); }
    int getDisplacementY() const { return m_displacement.y(); }
    bool isLyingCorpse() const { return m_lyingObject; }
    bool hasMiniMapColor() const { return m_minimapColor.has_value(); }
    int getMinimapColor() const { return m_minimapColor.value_or(0); }
    bool hasLensHelp() const { return m_lensHelp != 0; }
    int getLensHelp() const { return m_lensHelp; }
    bool isFullGround() const { return m_fullBank; }
    bool isIgnoreLook() const { return m_ignoreLook; }
    bool isImbueable() const { return m_imbueSlots.has_value(); }
    uint32_t getImbueSlots() const { return m_imbueSlots.value_or(0); }
    bool isCloth() const { return m_clothSlot.has_value(); }
    int getClothSlot() const { return m_clothSlot.value_or(0); }
    bool hasAction() const { return m_defaultAction.has_value(); }
    PLAYER_ACTION getDefaultAction() const { return static_cast<PLAYER_ACTION>(m_defaultAction.value_or(PLAYER_ACTION_NONE)); }
    bool isUsable() const { return m_usable; }
    bool hasCyclopediaItem() const { return m_cyclopediaType.has_value(); }
    uint32_t getCyclopediaType() const { return m_cyclopediaType.value_or(0); }
    /// The market category: the market's, 28 for cyclopedia-only objects, 30 for coins.
    int marketCategory() const { return m_marketCategory; }
    void setMarketCategory(int category) { m_marketCategory = category; }
    /// The market data, when the market trades the object under its own id.
    const TAppearanceMarketProperties* marketProperties() const { return m_market.get(); }
    bool isMarketable() const { return m_market != nullptr; }
    const std::vector<TNpcTradeData>& npcTradeData() const;
    void addNpcTradeData(TNpcTradeData data);
    /// The average buy price over the NPCs (fixed for the three coins).
    int getMeanPrice() const;
    bool hasUpgradeClassification() const { return m_upgradeClassification.has_value(); }
    uint16_t getClassification() const { return m_upgradeClassification.value_or(0); }
    bool hasProficiency() const { return m_proficiencyId.has_value(); }
    uint32_t getProficiencyId() const { return m_proficiencyId.value_or(0); }
    uint32_t getMinimumLevel() const { return m_minimumLevel; }
    const std::vector<uint32_t>& getRestrictVocation() const { return m_restrictVocations; }
    /// The restricted vocations as bits (vocation v is bit v-1).
    uint16_t restrictVocationMask() const;
    uint32_t getAppearanceWeaponType() const { return m_weaponType; }
    /// The market weapon category of the weapon type (swords, axes, …), 0 for none.
    uint32_t getWeaponType() const;
    bool hasChangedToExpire() const { return m_formerObjectId.has_value(); }
    uint16_t formerObjectId() const { return m_formerObjectId.value_or(0); }
    /// The object this one expires into or from (both of a changed-to-expire pair).
    std::optional<uint16_t> expireCounterpart() const { return m_expireCounterpart; }
    void setExpireCounterpart(uint16_t id) { m_expireCounterpart = id; }
    bool isPodium() const { return m_showOffSocket; }
    bool hasWearOut() const { return m_wearOut; }
    /// Expires, with or without a clock.
    bool hasExpire() const { return m_expire; }
    bool hasExpireStop() const { return m_expireStop; }
    bool reverseAddonsEast() const { return m_reverseAddonsEast; }
    bool reverseAddonsWest() const { return m_reverseAddonsWest; }
    bool reverseAddonsNorth() const { return m_reverseAddonsNorth; }
    bool reverseAddonsSouth() const { return m_reverseAddonsSouth; }
    bool isDecoKit() const { return m_decoKit; }
    /// appearances.dat's animate-always flag (CipSoft does not read it; the attached effects do).
    bool isAnimateAlways() const { return m_animateAlways; }
    bool hasSkillWheelGem() const { return m_skillWheelGem.has_value(); }
    uint32_t getSkillWheelGemQualityId() const { return m_skillWheelGem ? m_skillWheelGem->first : 0; }
    uint32_t getSkillWheelGemVocationId() const { return m_skillWheelGem ? m_skillWheelGem->second : 0; }

    // Frame groups.
    const std::map<EFixedFrameGroup, TAppearanceTypeSpriteInfo>& frameGroups() const { return m_frameGroups; }
    bool hasFrameGroup(EFixedFrameGroup group) const { return m_frameGroups.contains(group); }
    /// Throws std::logic_error when the group does not exist.
    const TAppearanceTypeSpriteInfo& frameGroup(EFixedFrameGroup group) const;
    void addFrameGroup(EFixedFrameGroup group, TAppearanceTypeSpriteInfo info);
    /// A type without frame groups gets an empty initial group; an outfit missing
    /// its idle or moving group gets a copy of the other one.
    void completeFrameGroups();
    /// Every frame group is opaque (true without frame groups).
    bool isOpaque() const { return m_opaque; }

    // Frame access over the frame groups in order (an outfit's idle phases come
    // before its moving ones). Patterns are the largest of the groups.
    int getNumPatternX() const;
    int getNumPatternY() const;
    int getNumPatternZ() const;
    int getLayers() const;
    /// Every phase of every frame group.
    int getTotalPhases() const;
    /// The moving group's phases when it animates, else every phase.
    int getAnimationPhases() const;
    /// The phases before the moving group's (the standing ones).
    int getIdleAnimationPhases() const;
    /// The standing frame group's animation, null when it does not animate.
    const tibia::animation::TAnimationInfo* idleAnimation() const;
    /// The walking frame group's animation, null when there is none.
    const tibia::animation::TAnimationInfo* movingAnimation() const;
    /// The sprite of layer/pattern/phase, phases counted over the groups in order.
    std::optional<uint32_t> spriteIdAt(int layer, int x, int y, int z, int phase) const;
    /// The bounding boxes of the first frame group.
    const std::vector<QRect>& getBoundingBoxes() const;
    /// The frame size in fields (of 32 px).
    const QSize& getSize() const { return m_size; }
    int getWidth() const { return m_size.width(); }
    int getHeight() const { return m_size.height(); }
    bool isSingleDimension() const { return m_size.width() * m_size.height() == 1; }

    // Builder setters (postprocess).
    void setCategory(EAppearanceCategory category) { m_category = category; m_null = false; }
    void setId(uint16_t id) { m_id = id; }
    void setDisplacement(int x, int y) { m_displacement = QPoint(x, y); }

private:
    enum EHookDirection : int { HookNone = 0, HookSouth = 1, HookEast = 2 };

    void updateDerivedSize();

    uint16_t m_id = 0;
    EAppearanceCategory m_category = EAppearanceCategory::Object;
    QString m_name;
    QString m_description;
    bool m_bank = false;
    uint16_t m_waypoints = 0;
    bool m_clip = false;
    bool m_bottom = false;
    bool m_top = false;
    bool m_container = false;
    bool m_cumulative = false;
    bool m_forceUse = false;
    bool m_multiUse = false;
    bool m_write = false;
    bool m_writeOnce = false;
    bool m_reportable = false;
    bool m_wrap = false;
    bool m_unwrap = false;
    bool m_topEffect = false;
    bool m_corpse = false;
    bool m_playerCorpse = false;
    bool m_liquidPool = false;
    bool m_liquidContainer = false;
    bool m_unpass = false;
    bool m_unmove = false;
    bool m_unsight = false;
    bool m_avoid = false;
    bool m_noMovementAnimation = false;
    bool m_take = false;
    bool m_hang = false;
    bool m_ammo = false;
    bool m_dualWielding = false;
    int m_hookDirection = HookNone;
    bool m_rotate = false;
    Light m_light;
    bool m_hasLight = false;
    bool m_dontHide = false;
    bool m_translucent = false;
    int m_elevation = 0;
    QPoint m_displacement;
    bool m_lyingObject = false;
    std::optional<int> m_minimapColor;
    int m_lensHelp = 0;
    bool m_fullBank = false;
    bool m_ignoreLook = false;
    std::optional<uint8_t> m_imbueSlots;
    std::optional<uint16_t> m_clothSlot;
    std::optional<int> m_defaultAction;
    bool m_usable = false;
    std::optional<uint16_t> m_cyclopediaType;
    int m_marketCategory = 0;
    std::shared_ptr<TAppearanceMarketProperties> m_market;
    std::shared_ptr<std::vector<TNpcTradeData>> m_npcTradeData;
    std::optional<uint8_t> m_upgradeClassification;
    std::optional<uint32_t> m_proficiencyId;
    uint16_t m_minimumLevel = 0;
    std::vector<uint32_t> m_restrictVocations;
    uint32_t m_weaponType = 0;
    std::map<EFixedFrameGroup, TAppearanceTypeSpriteInfo> m_frameGroups;
    bool m_opaque = true;
    std::optional<uint16_t> m_formerObjectId;
    std::optional<uint16_t> m_expireCounterpart;
    bool m_showOffSocket = false;
    bool m_wearOut = false;
    bool m_expire = false;
    bool m_expireStop = false;
    bool m_reverseAddonsEast = false;
    bool m_reverseAddonsWest = false;
    bool m_reverseAddonsNorth = false;
    bool m_reverseAddonsSouth = false;
    bool m_decoKit = false;
    std::optional<std::pair<uint32_t, uint32_t>> m_skillWheelGem;

    // Not CipSoft's: an empty slot for ids nothing defines, and the frame size
    // in fields the host's draw path and the idleot pack read.
    bool m_null = true;
    bool m_animateAlways = false;
    QSize m_size{ 1, 1 };
};

}

