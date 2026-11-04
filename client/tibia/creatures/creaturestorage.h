#pragma once

#include "appearances/appearanceinstance.h"

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
#include <string>
#include <vector>

class Creature;
class ThingType;

namespace tibia::creatures {

using TCreatureID = uint32_t;

enum class ECreatureUpdateAppearanceType : uint8_t {
    FullOutfit = 0,
    Direction = 1,
    Health = 2,
};

/// CIP live creature row (mirrored from map `Creature`; field stack holds outfit appearance + creature id).
class TCreature final : public cip::tibia::creatures::TCreature
{
    Q_OBJECT
public:
    explicit TCreature(TCreatureID id = 0, QObject* parent = nullptr);

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

    /// World-map draw snapshot (synced from the gameplay `Creature` each walk frame).
    Point walkOffset() const { return m_walkOffset; }
    PointF jumpOffset() const { return m_jumpOffset; }
    Position lastStepFromPosition() const { return m_lastStepFrom; }
    Position lastStepToPosition() const { return m_lastStepTo; }
    bool isWalking() const { return m_walking; }
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
    int drawElevation() const { return m_drawElevation; }
    uint8_t patternX() const { return m_patternX; }
    uint8_t patternZ() const { return m_patternZ; }
    uint16_t animationPhase() const { return m_animationPhase; }
    uint16_t mountAnimationPhase() const { return m_mountAnimationPhase; }
    Point displacement() const { return m_displacement; }

    uint8_t creatureType() const { return m_creatureType; }
    uint8_t icon() const { return m_icon; }
    bool isNpc() const { return m_isNpc; }
    uint8_t inspectionType() const { return m_inspectionType; }

    /// Local-player HUD (synced from `LocalPlayer` on walk frames and stat packets).
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
    const std::vector<PaperdollPtr>& paperdolls() const { return m_paperdolls; }
    const std::vector<AttachedEffectPtr>& attachedEffects() const { return m_attachedEffects; }
    bool hasAttachedEffects() const { return !m_attachedEffects.empty(); }

    /// Local-player walk mutation authority (Phase 5.0-B). `LocalPlayer` delegates here.
    bool isWalkLocked() const;
    size_t preWalkingSize() const { return m_preWalks.size(); }
    bool isPreWalking() const { return !m_preWalks.empty(); }
    bool isAutoWalking() const { return m_autoWalkDestination.isValid(); }
    const Position& serverPosition() const { return m_serverPosition; }
    const Position& autoWalkDestination() const { return m_autoWalkDestination; }
    const Position& lastAutoWalkPosition() const { return m_lastAutoWalkPosition; }
    tibia::client::AutoWalkPathMode autoWalkPathMode() const { return m_autoWalkPathMode; }
    uint8_t autoWalkRetries() const { return m_autoWalkRetries; }
    ticks_t walkLockExpiration() const { return m_walkLockExpiration; }
    const std::deque<Position>& preWalks() const { return m_preWalks; }
    /// Predicted tile while pre-walking; otherwise live `position()`.
    Position predictedPosition() const
    {
        return m_preWalks.empty() ? m_position : m_preWalks.back();
    }
    /// Live walk timer (storage is tween authority — Phase 5.0-B).
    ticks_t walkTicksElapsed() const { return m_walkTimer.ticksElapsed(); }
    uint16_t stepDuration() const { return m_stepDuration; }

    void lockWalk(uint16_t millis = 250);
    void unlockWalk() { m_walkLockExpiration = 0; }
    void clearPreWalks() { m_preWalks.clear(); }
    const Position& pushPreWalk(const Position& pos);
    void popPreWalkFront();
    void setAutoWalkDestination(const Position& destination) { m_autoWalkDestination = destination; }
    void setLastAutoWalkPosition(const Position& position) { m_lastAutoWalkPosition = position; }
    void setAutoWalkPathMode(tibia::client::AutoWalkPathMode mode) { m_autoWalkPathMode = mode; }
    void setAutoWalkRetries(uint8_t retries) { m_autoWalkRetries = retries; }
    void clearAutoWalk();

    /// Walk tween authority (offset / walking tile / foot phase).
    void beginWalk(const Position& from, const Position& to);
    void advanceWalkTween();
    void terminateWalk();
    void stopWalk();
    void setWalkTurnDirection(tibia::client::Direction direction) { m_walkTurnDirection = direction; }
    /// Idle / mount phases for world draw (no Map `Creature` required).
    void refreshAnimationPhases();

    /// Copy protocol-facing fields from the live map `Creature` (gameplay registry).
    /// Phase 5.0-C: does not overwrite storage-authoritative protocol fields (health,
    /// outfit, speed, skull/shield, passable, light, isPlayer/isNpc).
    void applyFrom(::Creature& creature);

    /// Non-tween draw/HUD snapshot from map keep-alive (walk offset owned by storage).
    void syncWorldDrawFrom(::Creature& creature);

    /// Paperdolls / attached FX / mark squares / icon / hidden — Map keep-alive extras
    /// that protocol still mutates on the hollow `Creature` before field place.
    void syncKeepAliveExtrasFrom(::Creature& creature);

    /// CipSoft-path protocol writes (storage is authoritative; map `Creature` mirrors for walk/input).
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
    void setShader(const std::string& shader);
    void setTyping(bool typing);
    bool hasPaperdoll(uint16_t id) const;
    void attachPaperdoll(const PaperdollPtr& paperdoll);
    bool detachPaperdollById(uint16_t id);
    bool detachPaperdollByPriority(uint8_t priority);
    void attachEffect(const AttachedEffectPtr& effect);
    void detachEffectById(uint16_t effectId);
    void clearTemporaryAttachedEffects();
    void setIcon(uint8_t icon);
    void setInspectionType(uint8_t inspectionType);
    void showStaticSquare(const Color& color);
    void hideStaticSquare();
    void addTimedSquare(uint8_t color8bit);
    void removeTimedSquare();
    const std::string& shader() const { return m_shader; }
    bool isTyping() const { return m_typing; }

private:
    void rebuildOutfitAppearance();
    static Color informationColorForHealthPercent(uint8_t healthPercent);
    void syncAttachedFxFrom(::Creature& creature);
    void syncHudMetadataFrom(::Creature& creature);
    void syncLocalPlayerFrom(class LocalPlayer& player);

    void updateWalkOffset(uint8_t totalPixelsWalked);
    void updateWalkingTile();
    void updateWalkAnimation();
    void clearWalkingTile();
    uint16_t computeStepDuration(bool ignoreDiagonal = false);
    uint16_t computeAnimationPhase(bool mount) const;
    ThingType* outfitThingType() const;
    ThingType* mountThingType() const;
    int displacementXForWalk() const;
    int displacementYForWalk() const;

    struct CachedStep
    {
        uint16_t speed{ 0 };
        uint16_t groundSpeed{ 0 };
        uint16_t duration{ 0 };
        uint16_t walkDuration{ 0 };
        uint16_t diagonalDuration{ 0 };

        uint16_t getDuration(const tibia::client::Direction dir) const
        {
            return Position::isDiagonal(dir) ? diagonalDuration : duration;
        }
    };

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

    Point m_walkOffset;
    PointF m_jumpOffset;
    Position m_lastStepFrom;
    Position m_lastStepTo;
    bool m_walking{ false };
    Timer m_walkTimer;
    Timer m_footTimer;
    CachedStep m_stepCache;
    tibia::client::Direction m_lastStepDirection{ tibia::client::InvalidDirection };
    tibia::client::Direction m_walkTurnDirection{ tibia::client::InvalidDirection };
    uint8_t m_walkedPixels{ 0 };
    uint8_t m_walkAnimationPhase{ 0 };
    uint16_t m_calculatedStepSpeed{ 0 };
    uint16_t m_walkingAnimationSpeed{ 0 };
    Position m_walkingTilePos;
    bool m_hasWalkingTile{ false };
    ScheduledEventPtr m_walkFinishAnimEvent;
    bool m_terminatingWalk{ false };
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
    int m_drawElevation{ 0 };
    uint8_t m_patternX{ 2 };
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
    std::vector<PaperdollPtr> m_paperdolls;
    std::vector<AttachedEffectPtr> m_attachedEffects;
    std::string m_shader;
    bool m_typing{ false };

    std::deque<Position> m_preWalks;
    ticks_t m_walkLockExpiration{ 0 };
    Position m_autoWalkDestination;
    Position m_lastAutoWalkPosition;
    tibia::client::AutoWalkPathMode m_autoWalkPathMode{ tibia::client::AutoWalkPathMinimap };
    uint8_t m_autoWalkRetries{ 0 };
    Position m_serverPosition;
    uint16_t m_stepDuration{ 0 };
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
    /// LocalPlayer session row is never destroyed — KnownCreature reappear does
    /// not resend the name.
    void leaveMap(TCreatureID id);

    /// Creatures whose position is in the aware-range rectangle around `center`
    /// (same XY/Z bounds as `Map::getSpectatorsInRangeEx`).
    std::vector<std::shared_ptr<TCreature>> spectatorsNear(const Position& center, bool multiFloor,
                                                           int left, int right, int top, int bottom) const;

    /// All registered creature ids (iteration authority for walk advance).
    QList<TCreatureID> creatureIds() const;

    /// Map walk/input registry → storage (walk frames and legacy addThing bootstrap only).
    void upsertFromMapCreature(::Creature& creature);

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
    void syncWorldDrawFrom(::Creature& creature);

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

/// No-op when storage is not installed (editor/tests).
void syncMapCreatureIfLive(::Creature& creature);

/// CipSoft-path protocol: storage is authoritative. Map `Creature` keep-alive is not
/// mirrored for these fields (Phase 5.0-C); field stack sync uses storage position.
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
void protocolAttachPaperdoll(TCreatureID id, const PaperdollPtr& paperdoll);
void protocolDetachPaperdollById(TCreatureID id, uint16_t paperdollId);
void protocolDetachPaperdollByPriority(TCreatureID id, uint8_t priority);
void protocolSetShader(TCreatureID id, const std::string& shader);
void protocolSetTyping(TCreatureID id, bool typing);
void protocolSetIcon(TCreatureID id, uint8_t icon);
void protocolSetInspectionType(TCreatureID id, uint8_t inspectionType);
void protocolShowStaticSquare(TCreatureID id, const Color& color);
void protocolHideStaticSquare(TCreatureID id);
void protocolAddTimedSquare(TCreatureID id, uint8_t color8bit);
void protocolRemoveTimedSquare(TCreatureID id);
/// Push Map keep-alive extras (paperdolls / FX / marks) into storage without clobbering
/// protocol-owned stats. No-op when storage is missing.
void syncKeepAliveExtrasIfLive(::Creature& creature);

} // namespace tibia::creatures
