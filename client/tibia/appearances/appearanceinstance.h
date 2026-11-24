#pragma once

#include "animation/animator.h"
#include "appearances/appearanceinstancespritesequence.h"
#include "appearances/genericstorage.h"
#include "renderer/renderinfo.h"
#include "shared/cip_typed.h"
#include "tibia/client/outfit.h"
#include "worldmap/coordinates.h"

#include <client/const.h>
#include <client/declarations.h>
#include <client/position.h>
#include "clockservice.h"
#include <QColor>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tibia::appearances {
class TAppearanceType;
class TAppearanceInstanceTracker;
class TAppearancesManager;
}
namespace tibia::protocol {
struct TObjectData;
}
namespace tibia::protobuf::protocol {
class AppearanceInstance;
class Outfit;
}
using TAppearanceType = tibia::appearances::TAppearanceType;
class Item;

// Classic nested braces (qtclient + pack TUs share /FI framework/pch.h).
namespace tibia {
namespace creatures {
using TCreatureID = uint32_t;
class TCreature;
} // namespace creatures

namespace appearances {

/// The sprite pattern of a stackable count or a fluid colour (CIP draws stacks and fluids this way).
void objectPatternsFor(TAppearanceType* type, int countOrSubType, int& patternX, int& patternY, int& patternZ);

/// The colour a liquid type draws with, the pattern cell of its fluid sprites;
/// an unknown liquid type draws as colour 1 (blue).
uint32_t liquidColor(uint32_t liquidType);
/// The pattern of liquid `color` on an appearance of `width` × `height`
/// patterns: four colours per row, wrapped to the appearance's patterns.
void liquidPattern(uint32_t color, uint32_t width, uint32_t height, int& patternX, int& patternY);
/// The pile (0..7) a stack of `count` draws: one item draws pile 0, two to four
/// items piles 1 to 3, and 5, 10, 25 and 50 items start piles 4 to 7.
uint32_t cumulativePileIndex(uint8_t count);
/// The pattern of pile `index`: four piles per row.
void cumulativePilePattern(uint32_t index, int& patternX, int& patternY);
/// The column a hangable object draws for its hook state (0 none, 1 south,
/// 2 east), limited to the appearance's `width` columns.
int hookPattern(uint8_t hookState, uint8_t width);

/// CIP appearances::TAppearanceInstance — thin view over a TAppearanceType record.
class TAppearanceInstance : public ::tibia::CipTyped
{
public:
    /// An instance of `type` whose animation starts at `animationStartMs`.
    explicit TAppearanceInstance(TAppearanceType* type, qint64 animationStartMs = 0);
    virtual ~TAppearanceInstance() = default;

    /// The instance's number: instances are numbered in the order they are made.
    int serialNumber() const { return m_serialNumber; }

    /// The appearance type the instance draws (CIP v2).
    virtual TAppearanceType* thingType() const { return m_type; }
    uint16_t id() const;
    ThingCategory category() const;
    bool isNull() const;
    /// The pixels the object is lifted by the objects below it on its field.
    uint8_t elevation() const { return m_elevation; }
    void setElevation(uint8_t elevation) { m_elevation = elevation; }
    /// The creature this instance draws on a field (0 for objects and effects).
    tibia::creatures::TCreatureID creatureId() const { return m_creatureId; }
    void linkCreature(tibia::creatures::TCreatureID creatureId) { m_creatureId = creatureId; }
    /// The sprites this instance draws (null for a type without frame groups).
    const std::shared_ptr<TAppearanceInstanceSpriteSequence>& spriteSequence() const { return m_spriteSequence; }
    /// Draws `pattern` from now on (CIP v11).
    void setSpritePattern(const TAppearancePattern& pattern);
    /// The sprite the sequence draws now (CIP v3); null without a sequence.
    virtual const TAppearanceInstanceSpriteInfoPtr& currentSpriteInfo() const;
    /// The sequence's first sprite (CIP v4); null without a sequence.
    virtual const TAppearanceInstanceSpriteInfoPtr& firstSpriteInfo() const;
    /// Advances the sprite sequence's animation to `nowMs`; true when the drawn
    /// phase changed (CIP v5).
    virtual bool updateAnimation(qint64 nowMs) { return m_spriteSequence && m_spriteSequence->update(nowMs); }
    /// The type's displacement, with the instance's elevation as depth (CIP v8).
    virtual worldmap::TWorldMapSubfieldExtent displacement() const;
    /// The offset of a moving creature drawn by this instance; objects and
    /// effects do not move (CIP v9).
    virtual worldmap::TWorldMapSubfieldExtent movementOffset() const { return { 0, 0, 0 }; }
    /// The instance draws a creature: an outfit or a creature placeholder (CIP v10).
    virtual bool isCreature() const { return false; }

protected:
    TAppearanceType* m_type = nullptr;
    std::shared_ptr<TAppearanceInstanceSpriteSequence> m_spriteSequence;
    uint8_t m_elevation = 0;
    int m_serialNumber = 0;
    tibia::creatures::TCreatureID m_creatureId = 0;
    /// When the instance's animation starts (objects: time 0).
    qint64 m_animationStartMs = 0;
};

using TAppearanceInstancePtr = std::shared_ptr<TAppearanceInstance>;

class TObjectAppearanceInstance : public TAppearanceInstance
{
public:
    /// An object of `type` whose animation starts at `animationStartMs`.
    explicit TObjectAppearanceInstance(TAppearanceType* type, qint64 animationStartMs = 0)
        : TAppearanceInstance(type, animationStartMs)
    {
    }
    /// CIP v12: appends what the object draws on `coordinate`: its current
    /// sprite with its displacement and movement offset.
    virtual void appendRenderInfos(const worldmap::TWorldMapCoordinate& coordinate, renderer::TRenderInfoList& infos) const;

    uint16_t count() const { return m_count; }
    uint8_t subType() const { return m_subType; }
    uint8_t tier() const { return m_tier; }
    virtual void setCount(uint16_t count) { m_count = count; }
    void setSubType(uint8_t subType) { m_subType = subType; }
    void setTier(uint8_t tier) { m_tier = tier; }

    bool drawable() const { return m_canDraw; }
    void setDrawable(const bool draw) { m_canDraw = draw; }
    void setItemColor(const QColor& color) { m_itemColor = color; }
    const QColor& itemColor() const { return m_itemColor; }
    uint8_t patternX() const { return m_patternX; }
    uint8_t patternY() const { return m_patternY; }
    uint8_t patternZ() const { return m_patternZ; }
    void setDrawPatterns(uint8_t x, uint8_t y, uint8_t z)
    {
        m_patternX = x;
        m_patternY = y;
        m_patternZ = z;
        setSpritePattern({ x, y, z });
    }
    /// The phase to draw now, from this instance's own animator.
    int calculateAnimationPhase() const;

protected:
    mutable std::optional<animation::TAnimator> m_animator;
    uint16_t m_count{ 0 };
    uint8_t m_subType{ 0 };
    uint8_t m_tier{ 0 };
    QColor m_itemColor{ QColor(Qt::white) };
    uint8_t m_patternX{ 0 };
    uint8_t m_patternY{ 0 };
    uint8_t m_patternZ{ 0 };
    uint8_t m_phase{ 0 };
    int64_t m_lastPhase{ 0 };
    bool m_async{ true };
    bool m_canDraw{ true };
};

class TFieldEffectInstance;

/// A container object: the two category sets of its protocol object (the
/// categories whose bits are set, 1 to 31), an optional value, and an optional
/// field effect that animates with it.
class TContainerObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    explicit TContainerObjectAppearanceInstance(TAppearanceType* type, std::set<int> firstCategories = {},
                                                std::set<int> secondCategories = {}, bool hasValue = false,
                                                int value = 0, std::shared_ptr<TFieldEffectInstance> fieldEffect = {},
                                                qint64 animationStartMs = 0);

    const std::set<int>& firstCategories() const { return m_firstCategories; }
    const std::set<int>& secondCategories() const { return m_secondCategories; }
    bool hasValue() const { return m_hasValue; }
    int value() const { return m_value; }
    const std::shared_ptr<TFieldEffectInstance>& fieldEffect() const { return m_fieldEffect; }

    /// The object's animation, then the field effect's: true when either
    /// drawn phase changed.
    bool updateAnimation(qint64 nowMs) override;

private:
    std::set<int> m_firstCategories;
    std::set<int> m_secondCategories;
    bool m_hasValue;
    int m_value;
    std::shared_ptr<TFieldEffectInstance> m_fieldEffect;
};

/// A stackable object: its count picks the pile it draws.
class TCumulativeObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    /// A stack of `count`; a count of 0 keeps the first pile.
    TCumulativeObjectAppearanceInstance(TAppearanceType* type, uint16_t count);

    /// A changed count draws its pile.
    void setCount(uint16_t count) override;

private:
    /// The pattern of the current count's pile; an appearance without
    /// patterns draws its only sprite.
    void pilePattern(int& patternX, int& patternY) const;
};

/// An object hung on a wall: its hook state picks the column it draws.
class THangableObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    explicit THangableObjectAppearanceInstance(TAppearanceType* type);

    uint8_t hangSide() const { return m_hangSide; }
    /// Hang the object on `side` (0 none, 1 south, 2 east) and draw that column.
    void setHangSide(uint8_t side);

private:
    void updateHookPattern();

    uint8_t m_hangSide{ 0 };
};

/// A fluid container or splash: its liquid type picks the colour it draws.
class TLiquidObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    TLiquidObjectAppearanceInstance(TAppearanceType* type, uint32_t liquidType);

    uint32_t fluid() const { return m_fluid; }

private:
    uint32_t m_fluid{ 0 };
};

/// An object whose pattern follows the field it lies on, so multi-pattern
/// grounds (e.g. cobbled pavement) tile as connected variants. Effects are
/// pattern objects too: they draw the pattern of the field they are shown on.
class TPatternObjectAppearanceInstance : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
    /// An object of `type` drawn with the patterns of `coordinate`.
    TPatternObjectAppearanceInstance(TAppearanceType* type, const Position& coordinate, qint64 animationStartMs)
        : TObjectAppearanceInstance(type, animationStartMs)
    {
        applyWorldMapCoordinate(coordinate);
    }

    /// Pattern x, y and z are the coordinate modulo the appearance's pattern
    /// width, height and depth.
    void applyWorldMapCoordinate(const Position& pos);
};

/// An object with charges.
class TWearoutObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    explicit TWearoutObjectAppearanceInstance(TAppearanceType* type, uint32_t charges = 0, bool brandNew = false)
        : TObjectAppearanceInstance(type), m_charges(charges), m_brandNew(brandNew)
    {
    }

    uint32_t charges() const { return m_charges; }
    bool isBrandNew() const { return m_brandNew; }

private:
    uint32_t m_charges = 0;
    bool m_brandNew = false;
};

/// An object that expires: its remaining time counts down while it is not stopped.
class TExpireObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    explicit TExpireObjectAppearanceInstance(TAppearanceType* type, int64_t remainingMilliseconds = 0,
                                             bool brandNew = false, bool stopped = false, int64_t nowMilliseconds = 0)
        : TObjectAppearanceInstance(type)
        , m_lastUpdateMilliseconds(nowMilliseconds)
        , m_remainingMilliseconds(remainingMilliseconds)
        , m_brandNew(brandNew)
        , m_stopped(stopped)
    {
    }

    /// Counts the remaining time down by the time passed since the last count,
    /// once a second has passed; true when it counted.
    bool updateRemainingTime(int64_t nowMilliseconds);
    int64_t remainingMilliseconds() const { return m_remainingMilliseconds; }
    bool isBrandNew() const { return m_brandNew; }
    bool isStopped() const { return m_stopped; }

private:
    int64_t m_lastUpdateMilliseconds = 0;
    int64_t m_remainingMilliseconds = 0;
    bool m_brandNew = false;
    bool m_stopped = false;
};

/// A deco item kit: the kit of the object it unpacks to.
class TDecoItemKitAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    /// A deco item kit of `type` that holds `decoItemId`; it draws its first pattern.
    explicit TDecoItemKitAppearanceInstance(TAppearanceType* type, uint16_t decoItemId = 0)
        : TObjectAppearanceInstance(type), m_decoItemId(decoItemId)
    {
        setSpritePattern(TAppearancePattern{ 0, 0, 0 });
    }

    uint16_t decoItemId() const { return m_decoItemId; }

private:
    uint16_t m_decoItemId = 0;
};

class TOutfitAppearanceInstance;

/// A show-off socket (house podium): the socket object, shown or hidden, with
/// the outfit and the mount standing on it.
class TShowOffSocketAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    /// The socket `type`, drawn when `visible`, carrying `outfit` and `mount`
    /// (either may be null).
    TShowOffSocketAppearanceInstance(TAppearanceType* type, bool visible, std::shared_ptr<TOutfitAppearanceInstance> outfit,
                                     std::shared_ptr<TOutfitAppearanceInstance> mount);

    /// The socket object itself is drawn.
    bool isVisible() const { return m_visible; }
    const std::shared_ptr<TOutfitAppearanceInstance>& outfit() const { return m_outfit; }
    const std::shared_ptr<TOutfitAppearanceInstance>& mount() const { return m_mount; }

    /// CIP v5: the socket's animation, else the outfit's, else the mount's;
    /// true at the first one whose drawn phase changed.
    bool updateAnimation(qint64 nowMs) override;
    /// CIP v13: where the figure stands: the mount's displacement while there
    /// is one, else the outfit's, with the socket's elevation as depth; none
    /// without an outfit.
    worldmap::TWorldMapSubfieldExtent figureDisplacement() const;
    /// CIP v12: the socket's own render info while it is shown; then the
    /// mount's and the outfit's idle sprite (drawn for the socket's type) at
    /// the figure's displacement plus the socket type's elevation on both
    /// axes while the socket is shown.
    void appendRenderInfos(const worldmap::TWorldMapCoordinate& coordinate, renderer::TRenderInfoList& infos) const override;

private:
    bool m_visible = false;
    std::shared_ptr<TOutfitAppearanceInstance> m_outfit;
    std::shared_ptr<TOutfitAppearanceInstance> m_mount;
};

/// An object of an upgrade classification, drawn with its tier.
class TUpgradeableObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    explicit TUpgradeableObjectAppearanceInstance(TAppearanceType* type, uint8_t tier = 0) : TObjectAppearanceInstance(type)
    {
        m_tier = tier;
    }
};

/// An outfit: an idle and a moving sprite sequence, both drawing the pattern of
/// their direction and mount (CIP appearances::TOutfitAppearanceInstance).
class TOutfitAppearanceInstance : public TAppearanceInstance
{
public:
    /// CIP: the idle (frame group 1) and moving (frame group 2)
    /// sequences, the idle one drawn by default, then v21 and v12(direction,
    /// direction, 0, forced).
    TOutfitAppearanceInstance(TAppearanceType* type, uint8_t direction);

    const Outfit& outfit() const { return m_outfit; }
    /// The look the instance was made for (host: the HUD and lighting read it).
    void setOutfit(const Outfit& outfit) { m_outfit = outfit; }

    bool isCreature() const override { return true; }
    /// CIP v12: the idle sequence draws (direction, 0, z), the moving one
    /// (movingDirection, 0, z); only on a change unless `force`.
    virtual void setPatterns(uint8_t direction, uint8_t movingDirection, uint8_t z, bool force);
    /// CIP v13 / v14: the current sprite of the idle / moving sequence.
    const TAppearanceInstanceSpriteInfoPtr& idleSpriteInfo() const;
    const TAppearanceInstanceSpriteInfoPtr& movingSpriteInfo() const;
    /// CIP v15: the outfit looking south (UI previews).
    const TAppearanceInstanceSpriteInfoPtr& previewSpriteInfo() const { return m_previewSpriteInfo; }
    /// CIP v16: advances the idle and / or moving sequence; true when a phase changed.
    bool updateSequences(qint64 nowMs, bool idle, bool moving);
    /// CIP v17: the moving sequence's walking animator starts a step of
    /// `keyMs` at `nowMs`.
    void setMovingAnimatorKey(qint64 keyMs, qint64 nowMs)
    {
        if (m_movingSequence)
            m_movingSequence->setAnimatorKey(keyMs, nowMs);
    }
    /// CIP v5.
    bool updateAnimation(qint64 nowMs) override { return updateSequences(nowMs, true, true); }
    /// CIP v18 / v19 / v20.
    uint8_t direction() const { return m_direction; }
    uint8_t movingDirection() const { return m_movingDirection; }
    uint8_t patternZ() const { return m_patternZ; }
    const std::shared_ptr<TAppearanceInstanceSpriteSequence>& idleSequence() const { return m_idleSequence; }
    const std::shared_ptr<TAppearanceInstanceSpriteSequence>& movingSequence() const { return m_movingSequence; }
    /// The sprite of `phase` counted over the idle phases, then the moving
    /// ones (host: the creature's animators pick it); null without sprites.
    TAppearanceInstanceSpriteInfoPtr spriteInfoAtPhase(int phase) const;

    /// The type's reverse addon flags (CIP packs +0x134..0x137 for v3).
    static TReverseAddons reverseAddonsOf(const TAppearanceType* type);

protected:
    /// Host: an object drawn as an outfit has no outfit sequences.
    explicit TOutfitAppearanceInstance(TAppearanceType* type);
    /// CIP v21: the preview sprite (a new sequence of the idle frame group
    /// looking south).
    virtual void updatePreviewSpriteInfo();
    template <class Sequence>
    void buildSequences();

    Outfit m_outfit;
    uint8_t m_direction = 0;
    uint8_t m_movingDirection = 0;
    uint8_t m_patternZ = 0;
    std::shared_ptr<TAppearanceInstanceSpriteSequence> m_idleSequence;
    std::shared_ptr<TAppearanceInstanceSpriteSequence> m_movingSequence;
    TAppearanceInstanceSpriteInfoPtr m_previewSpriteInfo;
};

/// An outfit with colours and addons (CIP appearances::TCustomizableOutfitAppearanceInstance).
class TCustomizableOutfitAppearanceInstance final : public TOutfitAppearanceInstance
{
public:
    /// CIP: the outfit with customizable sequences.
    TCustomizableOutfitAppearanceInstance(TAppearanceType* type, uint8_t direction,
                                          const TOutfitCustomization& customization);

    /// CIP v12: also hands the sequences the customization and always renews
    /// the preview.
    void setPatterns(uint8_t direction, uint8_t movingDirection, uint8_t z, bool force) override;
    /// CIP v22.
    const TOutfitCustomization& customization() const { return m_customization; }

protected:
    /// CIP v21.
    void updatePreviewSpriteInfo() override;

private:
    TOutfitCustomization m_customization;
};

/// An object worn as an outfit: it draws its own sequence while idle and
/// moving, in every direction.
class TObjectOutfitAppearanceInstance final : public TOutfitAppearanceInstance
{
public:
    /// CIP: the object's sequence is the idle and the moving one; then v21.
    explicit TObjectOutfitAppearanceInstance(TAppearanceType* type);

    /// CIP v12: an object keeps its pattern.
    void setPatterns(uint8_t, uint8_t, uint8_t, bool) override {}

protected:
    /// CIP v21: the preview is the sequence's current sprite.
    void updatePreviewSpriteInfo() override;
};

class TWorldMapAppearanceInstance final : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

/// An effect: a pattern object that is not part of a field's objects.
class TEffectInstance : public TPatternObjectAppearanceInstance
{
public:
    /// An effect of `type` placed nowhere yet (a missile takes its place from
    /// its path).
    explicit TEffectInstance(TAppearanceType* type, qint64 animationStartMs = 0);
    /// An effect of `type` on `field`, drawing the field's pattern, whose
    /// animation starts at `animationStartMs`.
    TEffectInstance(TAppearanceType* type, const Position& field, qint64 animationStartMs);

    /// Advance to `nowMs`: the clock, then the animation (updateAnimation).
    virtual void update(qint64 nowMs);
    /// The animation played its cycles, or there is none (TFieldEffectInstance::v14).
    virtual bool isFinished() const;
    /// The effect draws now; a delayed effect draws once its delay has passed.
    virtual bool isShown() const { return true; }
    /// The phase and the pattern of the field to draw.
    bool getFieldDrawFrame(int& animationPhase, int& xPattern, int& yPattern) const;
    /// The field the effect is on (TFieldEffectInstance::v13).
    virtual const Position& field() const { return m_fieldPosition; }
    /// The animation's phase, 0 without one.
    int phase() const;
    bool underCreatures() const { return m_underCreatures; }
    void setUnderCreatures(const bool under) { m_underCreatures = under; }
    uint16_t variantId() const { return m_variantId; }
    void setVariantId(const uint16_t variantId) { m_variantId = variantId; }
    tibia::client::MagicEffectSources source() const { return m_source; }
    void setSource(tibia::client::MagicEffectSources source);
    /// The opacity of the effect's source, from the options.
    float opacity() const { return m_opacity; }
    bool isTopEffectType() const;

protected:
    /// The sequence's animator (the effect's animation).
    const animation::TAnimator* animator() const;

    qint64 m_startMs{ 0 };
    qint64 m_nowMs{ 0 };
    uint16_t m_variantId{ 0 };
    tibia::client::MagicEffectSources m_source{ tibia::client::ME_SOURCE_DEFAULT };
    bool m_underCreatures{ false };
    Position m_fieldPosition;
    float m_opacity{ 1.f };
};

/// An effect shown on a field: it draws the field's pattern, from the field's
/// top-left sub-field lifted by an elevation, with an opacity.
class TFieldEffectInstance : public TEffectInstance
{
public:
    /// Effect `type` on `field` lifted by `elevation` sub-fields, whose
    /// animation starts at `animationStartMs`, drawn with `opacity`.
    TFieldEffectInstance(TAppearanceType* type, const Position& field, uint8_t elevation, qint64 animationStartMs,
                         double opacity);

    /// The field's top-left sub-field, lifted by the effect's elevation.
    const worldmap::TWorldMapSubfieldCoordinate& subfieldCoordinate() const { return m_subfieldCoordinate; }

private:
    worldmap::TWorldMapSubfieldCoordinate m_subfieldCoordinate;
};

class TMissileEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;

    const Position& pathFrom() const { return m_pathFrom; }
    const Position& pathTo() const { return m_pathTo; }
    /// Fly from `from` to `to` starting at `nowMs`; returns the flight time in ms.
    float setPath(const Position& from, const Position& to, qint64 nowMs);
    /// The pixel offset from `pathFrom` at the last update.
    QPoint flightOffset() const;
    float flightDuration() const { return m_flightDuration; }
    void update(qint64 nowMs) override;
    /// The flight is over (TMissileEffectInstance::v14).
    bool isFinished() const override { return m_nowMs >= m_startMs + static_cast<qint64>(m_flightDuration); }
    /// CIP: the flight time per whole unit of the square root of the distance
    /// in fields (the game client sets it from the client settings).
    static void setMillisecondsPerField(qint64 Milliseconds);

private:
    static qint64 s_millisecondsPerField;
    Position m_pathFrom;
    Position m_pathTo;
    std::shared_ptr<animation::TWorldMapLinearMovementTween> m_tween;
    float m_flightDuration{ 0.f };
};

/// An effect shown after a delay. Until the delay has passed it draws an empty
/// appearance of its own; then it draws the effect it wraps, which runs
/// `delay` behind the clock.
class TDelayedEffectInstance final : public TEffectInstance
{
public:
    /// `effect`, created at `createdMs`, shown `delayMs` later.
    TDelayedEffectInstance(std::shared_ptr<TEffectInstance> effect, qint64 createdMs, qint64 delayMs);
    ~TDelayedEffectInstance() override;

    /// CIP v2: the effect's type once shown, the empty type before.
    TAppearanceType* thingType() const override;
    /// CIP v3: the effect's sprite once shown, an empty sprite before.
    const TAppearanceInstanceSpriteInfoPtr& currentSpriteInfo() const override;
    /// CIP v5: remembers `nowMs`; once shown, advances the effect to `nowMs`
    /// minus the delay and reports whether its phase changed.
    bool updateAnimation(qint64 nowMs) override;
    /// CIP v13: the wrapped effect's field.
    const Position& field() const override;
    /// CIP v14: the wrapped effect finished.
    bool isFinished() const override;
    /// CIP v12: once shown, what the wrapped effect draws on `coordinate`;
    /// nothing before.
    void appendRenderInfos(const worldmap::TWorldMapCoordinate& coordinate, renderer::TRenderInfoList& infos) const override;
    /// The delay has passed since the effect was created.
    bool isShown() const override { return m_delayMs + m_createdMs < m_currentMs; }

private:
    std::shared_ptr<TEffectInstance> m_effect;
    /// The empty appearance drawn before the delay has passed.
    std::unique_ptr<TAppearanceType> m_emptyType;
    std::shared_ptr<TAppearanceInstance> m_emptyInstance;
    qint64 m_delayMs{ 0 };
    qint64 m_createdMs{ 0 };
    qint64 m_currentMs{ 0 };
    TAppearanceInstanceSpriteInfoPtr m_emptySpriteInfo;
};

/// The eight directions of a vector on the map (y grows southwards), as the
/// cell of a 3×3 grid: 0 north-west, 1 north, 2 north-east, 3 west, 5 east,
/// 6 south-west, 7 south, 8 south-east. A zero vector points east.
int fieldDirection(int dx, int dy);
/// The pattern drawing `direction` (column, row of its cell; the centre draws 0, 0).
void directionPattern(int direction, int& patternX, int& patternY);

/// A field effect drawn in a direction (the attack animation on a creature).
class TDirectionalFieldEffectInstance final : public TFieldEffectInstance
{
public:
    /// Effect `type` on `field` lifted by `elevation`, fully opaque, drawing
    /// the pattern of `direction` instead of the field's.
    TDirectionalFieldEffectInstance(TAppearanceType* type, const Position& field, uint8_t elevation, int direction,
                                    qint64 animationStartMs);
};

/// A tutorial field marker: shown until the tutorial removes its markers.
class TTutorialFieldEffectInstance final : public TFieldEffectInstance
{
public:
    /// A fully opaque tutorial marker of `type` on `field` lifted by
    /// `elevation`: its animation loops until it is stopped.
    TTutorialFieldEffectInstance(TAppearanceType* type, const Position& field, uint8_t elevation,
                                 qint64 animationStartMs);

    void stop() { m_stopped = true; }
    bool isFinished() const override { return m_stopped || TFieldEffectInstance::isFinished(); }

private:
    bool m_stopped = false;
};

/// CIP appearances::TAppearanceInstanceFactory — pick concrete instance from TAppearanceType / outfit.
class TAppearanceInstanceFactory final : public ::tibia::CipTyped
{
public:
    /// The type of `id` in `category`, or the category's dummy type: no flags
    /// and one empty frame group (it reports isNull()).
    TAppearanceType* appearanceType(ThingCategory category, uint16_t id) const;
    /// The appearances define `id` in `category`.
    bool hasAppearanceType(ThingCategory category, uint16_t id) const;
    /// The dummy type of `category` (every category has one).
    TAppearanceType* dummyAppearanceType(ThingCategory category) const;
    TAppearanceInstancePtr fromThingType(TAppearanceType* type);
    /// An object with no state of its own: hangable objects hang, every other
    /// one follows its field's pattern.
    TAppearanceInstancePtr createObject(TAppearanceType* type);
    /// The object `id` with no state of its own (createObject of its type).
    TAppearanceInstancePtr createObject(uint16_t id);
    std::shared_ptr<TCumulativeObjectAppearanceInstance> createCumulativeObject(uint16_t id, uint8_t count);
    std::shared_ptr<TLiquidObjectAppearanceInstance> createLiquidObject(uint16_t id, uint32_t liquidType);
    std::shared_ptr<TDecoItemKitAppearanceInstance> createDecoItemKitObject(uint16_t id, uint16_t decoItemId);
    std::shared_ptr<TUpgradeableObjectAppearanceInstance> createUpgradeableObject(uint16_t id, uint8_t tier);
    /// CIP v6: the container object `id` with its two category sets and
    /// optional value; `withFieldEffect` gives it field effect 252 (made like
    /// createFieldEffect, on field (0, 0, 0) from source 0).
    std::shared_ptr<TContainerObjectAppearanceInstance>
    createContainerObject(uint16_t id, std::set<int> firstCategories, std::set<int> secondCategories, bool hasValue,
                          int value, bool withFieldEffect, qint64 animationStartMs);
    std::shared_ptr<TWearoutObjectAppearanceInstance> createWearoutObject(TAppearanceType* type, uint32_t charges,
                                                                                bool brandNew);
    std::shared_ptr<TExpireObjectAppearanceInstance> createExpireObject(TAppearanceType* type,
                                                                              int64_t remainingMilliseconds,
                                                                              bool brandNew, bool stopped);
    /// The instance of a protocol object, by its kind: liquid, cumulative,
    /// upgradeable, deco item kit, else a plain object (a hangable one takes
    /// the hook state of a hangable kind).
    TAppearanceInstancePtr createObject(const tibia::protocol::TObjectData& object);
    /// CIP v20: the instance of a protocol map object by its extension:
    /// liquid, cumulative, container, show-off socket, upgradeable, wearout,
    /// expire (the duration in seconds) or deco item kit; a plain object
    /// without one. `nowMs` starts a container's field effect.
    TAppearanceInstancePtr createFromProtobuf(const tibia::protobuf::protocol::AppearanceInstance& object, qint64 nowMs);
    /// CIP v19: the outfit of a protocol outfit: with a look type that outfit
    /// (its colours and addons), else with an object type that object drawn
    /// as an outfit, else none.
    std::shared_ptr<TOutfitAppearanceInstance> createOutfitFromProtobuf(const tibia::protobuf::protocol::Outfit& outfit,
                                                                               uint8_t direction);
    /// Effect `id` on `field` from `source`, created at `nowMs` (CIP v7).
    std::shared_ptr<TFieldEffectInstance> createFieldEffect(uint16_t id, const Position& field, uint8_t source,
                                                                   qint64 nowMs);
    /// Effect `id` on `field` drawn towards `direction` (CIP v8).
    std::shared_ptr<TDirectionalFieldEffectInstance> createDirectionalFieldEffect(uint16_t id, const Position& field,
                                                                                         int direction, qint64 nowMs);
    /// Missile `id` flying from `from` to `to` (CIP v9).
    std::shared_ptr<TMissileEffectInstance> createMissile(uint16_t id, const Position& from, const Position& to,
                                                                 uint8_t source, qint64 nowMs);
    /// An object drawn as an outfit (an outfit look of an object id).
    std::shared_ptr<TObjectOutfitAppearanceInstance> createObjectOutfit(uint16_t id);
    /// Creates `T` the way every factory instance is made: the appearance
    /// instance tracker counts it while it lives.
    template <class T, class... Args>
    std::shared_ptr<T> makeTracked(TAppearanceType* type, Args&&... args);
    /// An outfit looking towards `direction`: outfits with colour layers, or
    /// with addon patterns while an addon is worn, are customizable.
    std::shared_ptr<TOutfitAppearanceInstance> createOutfit(TAppearanceType* type, const Outfit& outfit,
                                                                   uint8_t direction);
    /// CIP v15: a customizable outfit.
    std::shared_ptr<TCustomizableOutfitAppearanceInstance>
    createCustomizableOutfit(TAppearanceType* type, uint8_t direction, const TOutfitCustomization& customization);
    /// CIP v16: an outfit without colours or addons.
    std::shared_ptr<TOutfitAppearanceInstance> createPlainOutfit(TAppearanceType* type, uint8_t direction);
    /// CIP v17: the plain outfit `id` (or the outfit category's dummy).
    std::shared_ptr<TOutfitAppearanceInstance> createPlainOutfit(uint16_t id, uint8_t direction);
    /// CIP v23: the tracker counting every instance the factory makes.
    std::shared_ptr<TAppearanceInstanceTracker> instanceTracker() const;
    TAppearanceInstancePtr fromProtocolObject(TAppearanceType* type, uint16_t count = 0, uint8_t subType = 0,
                                                     uint8_t tier = 0, uint8_t fluid = 0, uint8_t hangSide = 0);
    TAppearanceInstancePtr fromOutfit(const Outfit& outfit, TAppearanceType* type);
    /// A tutorial marker on `field`: kind 1 is effect 57, any other kind effect 56.
    std::shared_ptr<TTutorialFieldEffectInstance> createTutorialFieldEffect(int kind, const Position& field);

    /// The client's factory: it makes its instances from `AppearancesManager`'s
    /// types, keeps one dummy type per category and counts every instance it
    /// makes in its own tracker (over the manager's spritesheet mapping).
    explicit TAppearanceInstanceFactory(std::shared_ptr<TAppearancesManager> AppearancesManager);
    ~TAppearanceInstanceFactory();

    /// Host: the game client's factory while a client exists (install), else
    /// one factory over the process-wide appearances manager. For code that
    /// does not hold the factory CipSoft hands it.
    static const std::shared_ptr<TAppearanceInstanceFactory>& installed();
    /// Host: installs the game client's factory; null goes back to the fallback.
    static void install(std::shared_ptr<TAppearanceInstanceFactory> factory);

private:
    /// One dummy type per category: a type without flags whose only frame
    /// group is an empty initial one.
    static std::map<ThingCategory, std::unique_ptr<TAppearanceType>> makeDummyAppearanceTypes();

    std::shared_ptr<TAppearancesManager> m_pAppearancesManager;
    std::map<ThingCategory, std::unique_ptr<TAppearanceType>> m_dummyAppearanceTypes;
    std::shared_ptr<TAppearanceInstanceTracker> m_instanceTracker;
};


} // namespace appearances
} // namespace tibia
