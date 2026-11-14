#pragma once

#include "animation/animator.h"
#include "animation/movingoutfitanimator.h"
#include "appearances/appearanceinstance.h"
#include "creatures/creaturemovementtween.h"
#include "creatures/movementdirection.h"

#include <client/declarations.h>
#include <client/position.h>
#include <client/staticdata.h>
#include <framework/core/declarations.h>
#include <framework/core/timer.h>
#include <framework/util/color.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class LocalPlayerSessionThing;
class ThingType;

namespace tibia {
namespace creatures {

using TCreatureID = uint32_t;

enum class ECreatureUpdateAppearanceType : uint8_t {
    FullOutfit = 0,
    Direction = 1,
    Health = 2,
};

/// CIP live creature row (protocol + field stack hold outfit appearance + creature id).
class TCreature final : public cip::tibia::creatures::TCreature
{
    Q_OBJECT
    friend class ::LocalPlayerSessionThing;

public:
    explicit TCreature(TCreatureID id = 0, QObject* parent = nullptr);

    const TCreatureMovementTween& movementTween() const { return m_movementTween; }

    /// One step from `origin` in `direction` between `startMs` and `endMs`.
    /// When the position already moved off `origin` the creature slides in from
    /// behind it; otherwise (a predicted step) it moves ahead of its position.
    void move(qint64 startMs, qint64 endMs, const worldmap::TWorldMapCoordinate& origin,
              EMovementDirection direction, int elevationFrom, int elevationTo);
    /// Drop any motion (teleport, floor change).
    void resetMovement();
    /// Per-frame: advance the motion and the walking animation.
    void update(qint64 nowMs);
    /// The step's time has not run out yet.
    bool isMoving() const { return !m_movementTween.isTweenFinished(); }
    /// Drawn away from its own field: mid-step, or a predicted step the server
    /// has not confirmed yet, which stays drawn on its destination.
    bool isDisplaced() const;

    TCreatureID creatureId() const { return m_creatureId; }
    const QString& name() const { return m_name; }
    uint8_t healthPercent() const { return m_healthPercent; }
    uint8_t skull() const { return m_skull; }
    uint8_t shield() const { return m_shield; }
    uint8_t emblem() const { return m_emblem; }
    const Outfit& outfit() const { return m_outfit; }
    const Position& position() const { return m_position; }
    appearances::TAppearanceInstancePtr outfitAppearance() const { return m_outfitAppearance; }
    uint8_t direction() const { return m_direction; }
    uint16_t speed() const { return m_speed; }
    Light light() const { return m_light; }

    /// Pixel offset of the drawn creature from its position.
    Point walkOffset() const;
    /// Field the creature is drawn on while moving (draw order).
    Position coveredField() const;
    PointF jumpOffset() const { return m_jumpOffset; }
    bool isWalking() const { return isMoving(); }
    bool isPassable() const { return m_passable; }
    bool isPlayer() const { return m_isPlayer; }
    bool isLocalPlayer() const { return m_isLocalPlayer; }
    bool isSoftDead() const { return m_softDead; }
    bool canDraw() const { return m_canDraw; }
    bool canBeSeen() const { return m_canBeSeen; }
    bool isDead() const { return m_dead; }
    bool isHidden() const { return m_hidden; }
    bool isHided() const { return m_hided; }
    bool drawOutfitColor() const { return m_drawOutfitColor; }
    bool showTimedSquare() const { return m_showTimedSquare; }
    Color timedSquareColor() const { return m_timedSquareColor; }
    bool showStaticSquare() const { return m_showStaticSquare; }
    Color staticSquareColor() const { return m_staticSquareColor; }
    /// The step's elevation while moving, the field's otherwise.
    int drawElevation() const;
    /// The local player holds a movement intent other than Stop; keeps the
    /// walking sprite briefly between steps.
    void setHoldsMovementIntent(bool holds) { m_holdsMovementIntent = holds; }
    uint8_t patternX() const { return m_patternX; }
    /// The walking sprite is drawn shown.
    bool showsMovingSprite() const;
    /// The look the outfit is drawn with: the step's look for the walking
    /// sprite, the creature's look for the standing one.
    uint8_t drawnPatternX() const { return showsMovingSprite() ? m_movementLook : m_patternX; }
    /// The look of the last step: each step turns from it, while a turn from
    /// the server changes only the look (`patternX`).
    uint8_t movementLook() const { return m_movementLook; }
    uint8_t patternZ() const { return m_patternZ; }
    uint16_t animationPhase() const { return m_animationPhase; }
    uint16_t mountAnimationPhase() const { return m_mountAnimationPhase; }
    /// The standing sequence's phase (an outfit shown as an object, too).
    int idleAnimationPhase() const { return m_outfitIdleAnimator ? m_outfitIdleAnimator->phase() : 0; }
    Point displacement() const { return m_displacement; }

    uint8_t creatureType() const { return m_creatureType; }
    uint8_t icon() const { return m_icon; }
    bool isNpc() const { return m_isNpc; }
    uint8_t inspectionType() const { return m_inspectionType; }

    /// Local-player HUD (synced from `LocalPlayerSessionThing` on walk frames and stat packets).
    uint32_t mana() const { return m_mana; }
    uint32_t maxMana() const { return m_maxMana; }
    uint32_t manaShield() const { return m_manaShield; }
    uint32_t maxManaShield() const { return m_maxManaShield; }
    uint8_t harmony() const { return m_harmony; }
    bool isSerene() const { return m_serene; }
    uint8_t vocation() const { return m_vocation; }
    const Color& informationColor() const { return m_informationColor; }
    int8_t echoRaidColor() const { return m_echoRaidColor; }
    const std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& hudIcons() const { return m_hudIcons; }
    const std::vector<AttachedEffectPtr>& attachedEffects() const { return m_attachedEffects; }
    bool hasAttachedEffects() const { return !m_attachedEffects.empty(); }

    /// Freeze the current step where it is.
    void stopMovement() { m_movementTween.stop(); }

    /// Session LocalPlayerSessionThing draw/HUD snapshot (walk offset owned by storage tween).
    void syncWorldDrawFrom(LocalPlayerSessionThing& player);

    /// CipSoft-path protocol writes (storage is authoritative).
    void setName(const QString& name);
    void setPosition(const Position& position);
    void setCreatureType(uint8_t creatureType);
    void setIsPlayer(bool isPlayer);
    void setIsNpc(bool isNpc);
    void setHidden(bool hidden);
    void setHealthPercent(uint8_t healthPercent);
    void setOutfit(const Outfit& outfit);
    void setLight(const Light& light);
    void setSpeed(uint16_t speed);
    void setSkull(uint8_t skull);
    void setShield(uint8_t shield);
    void setEmblem(uint8_t emblem);
    void setPassable(bool passable);
    void setDirection(uint8_t direction);
    void setEchoRaidColor(int8_t color);
    void setIcons(const std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& icons);
    void setVocation(uint8_t vocation);
    void attachEffect(const AttachedEffectPtr& effect);
    void detachEffectById(uint16_t effectId);
    void clearTemporaryAttachedEffects();
    void setIcon(uint8_t icon);
    void setInspectionType(uint8_t inspectionType);
    void showStaticSquare(const Color& color);
    void hideStaticSquare();
    void addTimedSquare(uint8_t color8bit);
    void removeTimedSquare();

private:
    void rebuildOutfitAppearance();
    static Color informationColorForHealthPercent(uint8_t healthPercent);
    void restartWalkAnimators();
    void refreshWalkAnimators();
    void updateCoveredField();
    uint16_t animationPhaseFor(const ThingType* type, const std::optional<animation::TMovingOutfitAnimator>& walk,
                               const std::optional<animation::TAnimator>& idle) const;
    ThingType* outfitThingType() const;
    ThingType* mountThingType() const;

    TCreatureMovementTween m_movementTween;
    std::optional<animation::TMovingOutfitAnimator> m_outfitWalkAnimator;
    std::optional<animation::TMovingOutfitAnimator> m_mountWalkAnimator;
    /// The standing sprite sequences' animators, started at time 0.
    std::optional<animation::TAnimator> m_outfitIdleAnimator;
    std::optional<animation::TAnimator> m_mountIdleAnimator;

    TCreatureID m_creatureId{ 0 };
    QString m_name;
    uint8_t m_healthPercent{ 101 };
    uint8_t m_skull{ 0 };
    uint8_t m_shield{ 0 };
    uint8_t m_emblem{ 0 };
    Outfit m_outfit;
    Position m_position;
    appearances::TAppearanceInstancePtr m_outfitAppearance;
    uint8_t m_direction{ 2 };
    uint16_t m_speed{ 0 };
    Light m_light;

    PointF m_jumpOffset;
    Position m_walkingTilePos;
    bool m_hasWalkingTile{ false };
    bool m_passable{ true };
    bool m_isPlayer{ false };
    bool m_isLocalPlayer{ false };
    bool m_softDead{ false };
    bool m_canDraw{ true };
    bool m_canBeSeen{ true };
    bool m_dead{ false };
    bool m_hidden{ false };
    bool m_hided{ false };
    bool m_drawOutfitColor{ true };
    bool m_showTimedSquare{ false };
    Color m_timedSquareColor;
    bool m_showStaticSquare{ false };
    Color m_staticSquareColor;
    qint64 m_lastFrameMs{ 0 };
    qint64 m_lastMovingFrameMs{ 0 };
    bool m_suppressMovingSprite{ false };
    bool m_holdsMovementIntent{ false };
    uint8_t m_patternX{ 2 };
    uint8_t m_movementLook{ 0 };
    uint8_t m_patternZ{ 0 };
    uint16_t m_animationPhase{ 0 };
    uint16_t m_mountAnimationPhase{ 0 };
    Point m_displacement;

    uint8_t m_creatureType{ 0 };
    uint8_t m_icon{ 0 };
    bool m_isNpc{ false };
    uint8_t m_inspectionType{ 0 };
    uint32_t m_mana{ 0 };
    uint32_t m_maxMana{ 0 };
    uint32_t m_manaShield{ 0 };
    uint32_t m_maxManaShield{ 0 };
    uint8_t m_harmony{ 0 };
    bool m_serene{ false };
    uint8_t m_vocation{ 0 };
    Color m_informationColor{ Color::white };
    int8_t m_echoRaidColor{ -1 };
    std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> m_hudIcons;
    std::vector<AttachedEffectPtr> m_attachedEffects;

};

/// Session id → creature (CIP: TCreatureStorage). Installed while TGameClient is live.
class TCreatureStorage final : public cip::tibia::creatures::TCreatureStorage
{
    Q_OBJECT
public:
    explicit TCreatureStorage(QObject* parent = nullptr);
    ~TCreatureStorage() override;

    static TCreatureStorage* instance();
    static void setInstance(TCreatureStorage* storage);

    std::shared_ptr<TCreature> getOrCreate(TCreatureID id);
    std::shared_ptr<TCreature> creatureAt(TCreatureID id) const;
    void remove(TCreatureID id);

    /// CIP DeleteOnMap: leave the field, keep the known-creature row (name/outfit).
    /// Non-local rows clear position so battle list / spectators drop them; the
    /// LocalPlayerSessionThing session row is never destroyed — KnownCreature reappear does
    /// not resend the name.
    void leaveMap(TCreatureID id);

    /// Creatures whose position is in the aware-range rectangle around `center`
    /// (same XY/Z bounds as `Map::getSpectatorsInRangeEx`).
    std::vector<std::shared_ptr<TCreature>> spectatorsNear(const Position& center, bool multiFloor,
                                                           int left, int right, int top, int bottom) const;

    /// All registered creature ids (iteration authority for walk advance).
    QList<TCreatureID> creatureIds() const;

    /// Upsert identity (id/name/outfit/direction/position/type) into storage.
    /// Pass `position` only when known (tile place); empty outfit id skips outfit write.
    std::shared_ptr<TCreature> ensureFromProtocol(TCreatureID id,
                                                  const QString& name,
                                                  const Outfit& outfit = {},
                                                  uint8_t direction = 2,
                                                  uint8_t creatureType = 0,
                                                  bool isPlayer = false,
                                                  bool isNpc = false,
                                                  const Position* position = nullptr);

    /// Per-frame motion/animation sync for world draw (called from walk advance).
    void syncWorldDrawFrom(LocalPlayerSessionThing& player);

    void publishCreatureUpdated(const std::shared_ptr<TCreature>& row,
                                ECreatureUpdateAppearanceType appearanceType = ECreatureUpdateAppearanceType::Health);

    /// Push local-player mana/harmony/vocation into storage (stat packets between walk frames).
    void syncLocalPlayerStatsFromMap();

Q_SIGNALS:
    void creatureUpdated(std::weak_ptr<TCreature> creature);
    void creatureAppearanceUpdated(std::weak_ptr<TCreature> creature, ECreatureUpdateAppearanceType appearanceType);
    void playerAdded(std::weak_ptr<TCreature> player);
    /// Fired after a row leaves storage (forget / unaware / clean).
    void creatureRemoved(tibia::creatures::TCreatureID creatureId);

private:
    QHash<TCreatureID, std::shared_ptr<TCreature>> m_creatures;
};

/// CipSoft-path protocol: storage is authoritative.
std::shared_ptr<TCreature> ensureFromProtocol(TCreatureID id,
                                              const QString& name,
                                              const Outfit& outfit = {},
                                              uint8_t direction = 2,
                                              uint8_t creatureType = 0,
                                              bool isPlayer = false,
                                              bool isNpc = false,
                                              const Position* position = nullptr);
void protocolSetHealthPercent(TCreatureID id, uint8_t healthPercent);
void protocolSetLight(TCreatureID id, const Light& light);
void protocolSetOutfit(TCreatureID id, const Outfit& outfit);
void protocolSetSpeed(TCreatureID id, uint16_t speed, uint16_t baseSpeed);
void protocolSetSkull(TCreatureID id, uint8_t skull);
void protocolSetShield(TCreatureID id, uint8_t shield);
void protocolSetPassable(TCreatureID id, bool passable);
void protocolSetDirection(TCreatureID id, uint8_t direction);
void protocolSetEchoRaidColor(TCreatureID id, int8_t color);
void protocolSetIcons(TCreatureID id, const std::vector<std::tuple<uint8_t, uint8_t, uint16_t>>& icons);
void protocolSetVocation(TCreatureID id, uint8_t vocation);
void protocolAttachEffect(TCreatureID id, const AttachedEffectPtr& effect);
void protocolDetachEffect(TCreatureID id, uint16_t effectId);
void protocolClearTemporaryAttachedEffects(TCreatureID id);
void protocolSetIcon(TCreatureID id, uint8_t icon);
void protocolSetInspectionType(TCreatureID id, uint8_t inspectionType);
void protocolShowStaticSquare(TCreatureID id, const Color& color);
void protocolHideStaticSquare(TCreatureID id);
void protocolAddTimedSquare(TCreatureID id, uint8_t color8bit);
void protocolRemoveTimedSquare(TCreatureID id);

}
}
