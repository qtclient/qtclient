#pragma once

#include "animation/animator.h"
#include "animation/movingoutfitanimator.h"
#include "appearances/appearanceinstance.h"
#include "appearances/creatureplaceholderappearanceinstance.h"
#include "creatures/creaturemovementtween.h"
#include "creatures/movementdirection.h"
#include "worldmap/coordinates.h"

#include <client/declarations.h>
#include <client/position.h>
#include <client/staticdata.h>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include "clockservice.h"
#include <QColor>

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tibia::appearances {
class TAppearanceType;
}
using TAppearanceType = tibia::appearances::TAppearanceType;

namespace tibia {
namespace creatures {

using TCreatureID = uint32_t;

/// Which appearance of a creature the server replaced.
enum class ECreatureUpdateAppearanceType : uint8_t {
    Outfit = 0,
    Mount = 1,
    FullOutfit = Outfit,
    /// Host: a creature update that replaced no appearance.
    None = 0xFF,
};

/// One icon over a creature: its kind (0 player icon, 1 creature icon), the
/// icon, the number shown with it and the flag the server sends with it.
struct TCreatureIcon
{
    uint32_t kind = 0;
    uint8_t icon = 0;
    uint16_t value = 0;
    bool flag = false;

    /// Views bind an icon as (icon, kind, value).
    template<std::size_t I>
    const auto& get() const
    {
        if constexpr (I == 0)
            return icon;
        else if constexpr (I == 1)
            return kind;
        else
            return value;
    }
};

/// The client options that decide which frames around a creature show.
struct TCreatureMarkOptions
{
    /// gameWindowShowTragetFrame: the attack, follow and selection frame.
    bool targetFrame = true;
    /// combatShowPvpFrames: the mark the server keeps on a creature.
    bool staticSquare = true;
    /// combatShowFrames: the mark the server shows for a moment.
    bool timedSquare = true;
    /// mouseHighlightTarget: the creature under the mouse is highlighted
    /// instead of getting the white selection frame.
    bool mouseHighlightTarget = true;
};

/// The frames drawn around a creature: the target frame (attack, follow,
/// selection), the server's static square and its timed square. A frame is
/// hidden while its colour is fully transparent.
struct TCreatureMarks
{
    QColor targetFrame{ 0, 0, 0, 0 };
    QColor staticSquare{ 0, 0, 0, 0 };
    QColor timedSquare{ 0, 0, 0, 0 };
    /// The timed square shows until then.
    qint64 timedSquareEndMs = 0;
    bool timedSquareShown = false;
};

/// CIP live creature row (protocol + field stack hold outfit appearance + creature id).
class TCreature final : public cip::tibia::creatures::TCreature, public std::enable_shared_from_this<TCreature>
{
    Q_OBJECT

public:
    explicit TCreature(TCreatureID id = 0, QObject* parent = nullptr);

    /// A new creature with the placeholder that stands for it on the map.
    static std::shared_ptr<TCreature> create(TCreatureID id = 0);
    /// Makes the placeholder that stands for the creature on the map (needs the
    /// creature to be owned by a shared pointer).
    void createPlaceholderAppearance();
    /// The placeholder that stands for the creature on the map.
    const std::shared_ptr<appearances::TCreaturePlaceholderAppearanceInstance>& placeholderAppearance() const
    {
        return m_placeholderAppearance;
    }
    /// The creature's offset within its current step.
    worldmap::TWorldMapSubfieldExtent movementOffset() const { return m_movementTween.drawOffset(); }

    const TCreatureMovementTween& movementTween() const { return m_movementTween; }
    /// The subfield the creature's field is drawn from: its position moved by the
    /// step's covered field, plus the offset within the step.
    worldmap::TWorldMapSubfieldCoordinate drawSubfieldCoordinate() const;
    /// The same subfield moved up and left by the placeholder's displacement and elevation.
    worldmap::TWorldMapSubfieldCoordinate displacedDrawSubfieldCoordinate() const;
    /// When the current step ends.
    qint64 movementEndTime() const { return m_movementTween.endTime(); }
    /// The outfit looking south (UI previews); null without an outfit.
    appearances::TAppearanceInstanceSpriteInfoPtr previewSpriteInfo() const;

    /// One step from `origin` in `direction` between `startMs` and `endMs`.
    /// When the position already moved off `origin` the creature slides in from
    /// behind it; otherwise (a predicted step) it moves ahead of its position.
    void move(qint64 startMs, qint64 endMs, const worldmap::TWorldMapCoordinate& origin,
              EMovementDirection direction, int elevationFrom, int elevationTo);
    /// Drop any motion (teleport, floor change).
    void resetMovement();
    /// Per-frame: advance the motion and the outfit's and mount's sequences.
    void update(qint64 nowMs);
    /// The step's time has not run out yet.
    bool isMoving() const { return !m_movementTween.isTweenFinished(); }
    /// Drawn away from its own field: mid-step, or a predicted step the server
    /// has not confirmed yet, which stays drawn on its destination.
    bool isDisplaced() const;

    TCreatureID creatureId() const { return m_creatureId; }
    /// A player's name as the server sent it; every other creature's name
    /// with each word capitalised.
    const QString& name() const;
    /// Changes the creature's identity: a creature the storage replaced gets a
    /// new one, so views keyed by it start over.
    int uniqueId() const { return m_uniqueId; }
    void renewUniqueId();
    /// The own character (set when the storage adds the player's creature).
    void markAsLocalPlayer() { m_isLocalPlayer = true; }
    uint8_t healthPercent() const { return m_healthPercent; }
    float healthRatio() const { return m_healthPercent / 100.0f; }
    /// A party member's mana.
    uint8_t manaPercent() const { return m_manaPercent; }
    float manaRatio() const { return m_manaPercent / 100.0f; }
    void setManaPercent(uint8_t manaPercent) { m_manaPercent = manaPercent; }
    /// A party member's status is shown (party list).
    bool showsStatus() const { return m_showStatus; }
    void setShowStatus(bool show) { m_showStatus = show; }
    uint32_t masterId() const { return m_masterId; }
    void setMasterId(uint32_t masterId) { m_masterId = masterId; }
    uint8_t skull() const { return m_skull; }
    uint8_t shield() const { return m_shield; }
    /// Party shields: the creature leads a party (or invited this player to one).
    bool isPartyLeader() const { return m_shield < 11 && (0x552u >> m_shield & 1) != 0; }
    /// In a party, or inviting this player to one.
    bool isInParty() const { return isPartyMember() || m_shield == 1; }
    bool isPartyMember() const { return static_cast<uint8_t>(m_shield - 3) < 8; }
    /// The party shares experience (active or not).
    bool isSharingExperience() const { return static_cast<uint8_t>(m_shield - 5) < 6; }
    uint8_t emblem() const { return m_emblem; }
    /// The creature's guild is at war: the green, red, blue and "other" emblems
    /// (1, 2, 3 and 5).
    bool isInGuildWar() const { return m_emblem < 6 && ((0x2eu >> m_emblem) & 1u) != 0; }
    const Outfit& outfit() const { return m_outfit; }
    const Position& position() const { return m_position; }
    appearances::TAppearanceInstancePtr outfitAppearance() const { return m_outfitAppearance; }
    /// Replaces the outfit instance; on a change the outfit and the mount draw
    /// the creature's look again.
    void setOutfitAppearance(const appearances::TAppearanceInstancePtr& appearance);
    /// The mount drawn under the creature while it rides (kept while dismounted).
    std::shared_ptr<appearances::TOutfitAppearanceInstance> mountAppearance() const { return m_mountAppearance; }
    /// Replaces the mount instance; on a change the outfit and the mount draw
    /// the creature's look again.
    void setMountAppearance(const std::shared_ptr<appearances::TOutfitAppearanceInstance>& appearance);
    uint8_t direction() const { return m_direction; }
    uint16_t speed() const { return m_speed; }
    Light light() const { return m_light; }

    /// Pixel offset of the drawn creature from its position.
    QPoint walkOffset() const;
    /// Field the creature is drawn on while moving (draw order).
    Position coveredField() const;
    bool isWalking() const { return isMoving(); }
    bool isUnpassable() const { return m_unpassable; }
    void setUnpassable(bool unpassable) { m_unpassable = unpassable; }
    bool isPassable() const { return !m_unpassable; }
    bool isPlayer() const;
    bool isLocalPlayer() const { return m_isLocalPlayer; }
    /// Host: an extension keeps the creature (and its HUD) off the map.
    bool isDrawSuppressed() const { return m_drawSuppressed; }
    bool isDead() const { return m_healthPercent == 0; }
    bool isHidden() const;
    /// Some frame shows: one of the colours is set and its option is on.
    bool hasMarks() const;
    /// The colours of the frames that show, target frame first.
    std::vector<QColor> marks() const;
    /// As `marks`, without the white selection frame while the creature under
    /// the mouse is highlighted instead.
    std::vector<QColor> shownMarks() const;
    /// Which frames show, for every creature.
    static const TCreatureMarkOptions& markOptions();
    static void setMarkOptions(const TCreatureMarkOptions& options);
    /// The game client's option switches, one frame each: the mark the server
    /// keeps on a creature (combatShowPvpFrames), the one it shows for a
    /// moment (combatShowFrames), the attack / follow / selection frame
    /// (gameWindowShowTragetFrame), and the white selection frame, shown
    /// unless the creature under the mouse is highlighted instead.
    static void setShowStaticSquare(bool show) { markOptionsForWrite().staticSquare = show; }
    static void setShowTimedSquare(bool show) { markOptionsForWrite().timedSquare = show; }
    static void setShowTargetFrame(bool show) { markOptionsForWrite().targetFrame = show; }
    static void setShowSelectionFrame(bool show) { markOptionsForWrite().mouseHighlightTarget = !show; }
    /// CIP: how long a timed square shows (the game client sets it from the client settings).
    static void setTimedSquareDuration(qint64 Milliseconds);
    /// The step's elevation while moving, the field's otherwise.
    int drawElevation() const;
    uint8_t patternX() const { return m_patternX; }
    /// The walking sprite is drawn: the placeholder shows the moving sprites.
    bool showsMovingSprite() const;
    /// The look the outfit is drawn with: the step's look for the walking
    /// sprite, the creature's look for the standing one.
    uint8_t drawnPatternX() const { return showsMovingSprite() ? m_movementLook : m_patternX; }
    /// The look of the last step: each step turns from it, while a turn from
    /// the server changes only the look (`patternX`).
    uint8_t movementLook() const { return m_movementLook; }
    /// The look the moving sprites draw; the outfit and the mount follow it.
    void setMovementLook(uint8_t look);
    /// The outfit's pattern depth: the mounted sprites while riding.
    uint8_t patternZ() const { return m_mounted ? 1 : 0; }
    /// The creature rides its mount: the outfit shows its mounted sprites and
    /// the mount is drawn under it.
    bool isMounted() const { return m_mounted; }
    /// The outfit shows its mounted sprites; the mount keeps its own.
    void setMounted(bool mounted);

    uint8_t creatureType() const { return m_creatureType; }
    /// The NPC category shown over the creature.
    uint8_t icon() const { return m_icon; }
    bool isNpc() const;
    uint8_t inspectionType() const { return m_inspectionType; }
    /// Inspection between this player and the creature: permission granted,
    /// asked for, this player may inspect it, it asks to inspect this player.
    bool hasInspectionState(uint8_t mask) const { return (mask & m_inspectionType) != 0; }
    bool inspectionGranted() const { return m_inspectionType & 1; }
    bool inspectionRequested() const { return (m_inspectionType & 2) >> 1; }
    bool canBeInspected() const { return (m_inspectionType & 4) >> 2; }
    bool requestsInspection() const { return (m_inspectionType & 8) >> 3; }

    /// The local player's HUD values (copied from the player data on stat messages).
    uint32_t mana() const { return m_mana; }
    uint32_t maxMana() const { return m_maxMana; }
    uint32_t manaShield() const { return m_manaShield; }
    uint32_t maxManaShield() const { return m_maxManaShield; }
    uint8_t harmony() const { return m_harmony; }
    bool isSerene() const { return m_serene; }
    uint8_t vocation() const { return m_vocation; }
    const std::vector<TCreatureIcon>& hudIcons() const { return m_hudIcons; }
    void clearIcons() { m_hudIcons.clear(); }
    void addIcon(const TCreatureIcon& icon) { m_hudIcons.push_back(icon); }
    /// The status marks the server sets on the creature.
    const std::set<uint8_t>& extraIcons() const { return m_extraIcons; }
    void setExtraIcons(std::set<uint8_t> icons) { m_extraIcons = std::move(icons); }
    /// The server set status mark `type` (0 a raid leader, 1 its minion).
    bool hasStatusMark(uint8_t type) const { return m_extraIcons.count(type) != 0; }
    /// The creature stands on a map field (its field placed it there); a
    /// creature known only by id is not.
    bool isPlacedOnField() const { return m_placedOnField; }
    void setPlacedOnField(bool placed) { m_placedOnField = placed; }

    /// Freeze the current step where it is.
    void stopMovement() { m_movementTween.stop(); }

    /// Copies the player data's HUD values onto the local player's creature.
    void syncLocalPlayerStats();
    void setDrawSuppressed(bool suppressed) { m_drawSuppressed = suppressed; }

    /// Protocol writes (storage is authoritative).
    void setName(const QString& name);
    void setPosition(const Position& position);
    void setCreatureType(uint8_t creatureType) { m_creatureType = creatureType; }
    void setHealthPercent(uint8_t healthPercent) { m_healthPercent = healthPercent; }
    void setOutfit(const Outfit& outfit);
    void setLight(const Light& light) { m_light = light; }
    void setSpeed(uint16_t speed);
    void setSkull(uint8_t skull);
    void setShield(uint8_t shield);
    void setEmblem(uint8_t emblem);
    void setDirection(uint8_t direction);
    void setVocation(uint8_t vocation) { m_vocation = vocation; }
    void setIcon(uint8_t icon) { m_icon = icon; }
    void setInspectionType(uint8_t inspectionType);
    /// The attack, follow or selection frame in `color`.
    void setTargetFrame(const QColor& color);
    void hideTargetFrame();
    /// The mark the server keeps on the creature until it clears it.
    void showStaticSquare(const QColor& color);
    void hideStaticSquare();
    /// A mark shown for a moment from `nowMs`.
    void addTimedSquare(const QColor& color, qint64 nowMs);

Q_SIGNALS:
    /// Every position write, with the new position.
    void positionWasUpdated(tibia::worldmap::TWorldMapCoordinate coordinate);

private:
    void rebuildOutfitAppearance();
    /// The outfit draws the creature's look, movement look and mounted sprites;
    /// the mount draws the looks without a mount of its own.
    void updateOutfitPatterns(bool force);
    static QColor informationColorForHealthPercent(uint8_t healthPercent);
    void updateCoveredField();
    static TCreatureMarkOptions& markOptionsForWrite();

    TCreatureMovementTween m_movementTween;

    TCreatureID m_creatureId{ 0 };
    QString m_name;
    QString m_formattedName;
    int m_uniqueId{ 0 };
    uint8_t m_healthPercent{ 100 };
    uint8_t m_manaPercent{ 100 };
    bool m_showStatus{ true };
    uint32_t m_masterId{ 0 };
    uint8_t m_skull{ 0 };
    uint8_t m_shield{ 0 };
    uint8_t m_emblem{ 0 };
    Outfit m_outfit;
    Position m_position;
    std::shared_ptr<appearances::TCreaturePlaceholderAppearanceInstance> m_placeholderAppearance;
    appearances::TAppearanceInstancePtr m_outfitAppearance;
    std::shared_ptr<appearances::TOutfitAppearanceInstance> m_mountAppearance;
    uint8_t m_direction{ 2 };
    uint16_t m_speed{ 0 };
    Light m_light{ 0, 2 };

    Position m_walkingTilePos;
    bool m_hasWalkingTile{ false };
    bool m_unpassable{ false };
    bool m_isLocalPlayer{ false };
    bool m_drawSuppressed{ false };
    TCreatureMarks m_marks;
    bool m_placedOnField{ false };
    uint8_t m_patternX{ 2 };
    uint8_t m_movementLook{ 0 };
    bool m_mounted{ false };

    uint8_t m_creatureType{ 0 };
    uint8_t m_icon{ 0 };
    uint8_t m_inspectionType{ 0 };
    uint32_t m_mana{ 0 };
    uint32_t m_maxMana{ 0 };
    uint32_t m_manaShield{ 0 };
    uint32_t m_maxManaShield{ 0 };
    uint8_t m_harmony{ 0 };
    bool m_serene{ false };
    uint8_t m_vocation{ 0 };
    std::vector<TCreatureIcon> m_hudIcons;
    std::set<uint8_t> m_extraIcons;

};

/// Session id → creature (CIP: TCreatureStorage). Installed while TGameClient is live.
class TCreatureStorage final : public cip::tibia::creatures::TCreatureStorage
{
    Q_OBJECT
public:
    explicit TCreatureStorage(QObject* parent = nullptr);
    ~TCreatureStorage() override;

    std::shared_ptr<TCreature> getOrCreate(TCreatureID id);
    std::shared_ptr<TCreature> creatureAt(TCreatureID id) const;
    /// A new creature with `id`, replacing any stored one.
    std::shared_ptr<TCreature> createCreature(TCreatureID id);
    /// Store `creature` under its id (replacing any stored one); the player's
    /// own creature is marked as such, and the attack or follow target gets
    /// its frame again.
    void add(const std::shared_ptr<TCreature>& creature);
    void remove(TCreatureID id);
    /// The own character's creature id (the login names it).
    void setPlayerID(TCreatureID id);
    TCreatureID playerID() const;
    std::shared_ptr<TCreature> player() const;

    /// The creature under the mouse or a hovered battle-list entry: white
    /// frame, or the lighter attack/follow colour on the attack or follow
    /// target. 0 (or an id not stored) clears it.
    void setTemporarySelectedID(TCreatureID id);
    TCreatureID temporarySelectedID() const { return m_temporarySelectedID; }
    /// The followed creature: green frame. Following stops attacking.
    void setFollowID(TCreatureID id);
    TCreatureID followID() const { return m_followID; }
    /// The attacked creature: red frame. Attacking stops following.
    void setAttackID(TCreatureID id);
    TCreatureID attackID() const { return m_attackID; }

    /// The creature with `id` (if stored) stands on a map field / left it.
    void placeOnField(TCreatureID id);
    void removeFromField(TCreatureID id);
    /// No stored creature stands on a field (the map is rebuilt).
    void clearPlacedOnFieldFlags();


    /// Creatures whose position is in the rectangle around `center` (inclusive;
    /// `multiFloor`: the floors the player sees from there).
    std::vector<std::shared_ptr<TCreature>> spectatorsNear(const Position& center, bool multiFloor,
                                                           int left, int right, int top, int bottom) const;

    /// The creatures the battle list shows, by id: every creature except the
    /// player and the dead; party members wherever they are, the others only
    /// when they stand on a field inside `view` (sheared onto its first floor).
    std::map<TCreatureID, std::shared_ptr<TCreature>> battleListCreatures(const worldmap::TWorldMapCuboid& view) const;
    /// All registered creature ids (iteration authority for walk advance).
    QList<TCreatureID> creatureIds() const;

    void notifyPlayerAdded(const std::shared_ptr<TCreature>& player);
    void notifyCreatureUpdated(const std::shared_ptr<TCreature>& creature);
    void notifyCreatureAppearanceUpdated(const std::shared_ptr<TCreature>& creature,
                                         ECreatureUpdateAppearanceType appearanceType);
    /// Host: creatureUpdated, then creatureAppearanceUpdated unless `None`.
    void publishCreatureUpdated(const std::shared_ptr<TCreature>& row,
                                ECreatureUpdateAppearanceType appearanceType = ECreatureUpdateAppearanceType::None);

    /// Push the local player's mana/harmony/vocation onto its creature (stat messages).
    void syncLocalPlayerStatsFromMap();

Q_SIGNALS:
    void creatureUpdated(std::weak_ptr<TCreature> creature);
    void creatureAppearanceUpdated(std::weak_ptr<TCreature> creature, ECreatureUpdateAppearanceType appearanceType);
    void playerAdded(std::weak_ptr<TCreature> player);
    /// Fired after a row leaves storage (forget / unaware / clean).
    void creatureRemoved(tibia::creatures::TCreatureID creatureId);

private:
    QHash<TCreatureID, std::shared_ptr<TCreature>> m_creatures;
    TCreatureID m_playerID{ 0 };
    TCreatureID m_temporarySelectedID{ 0 };
    TCreatureID m_followID{ 0 };
    TCreatureID m_attackID{ 0 };
};

}
}

namespace std {
template<>
struct tuple_size<tibia::creatures::TCreatureIcon> : integral_constant<size_t, 3>
{};
template<>
struct tuple_element<0, tibia::creatures::TCreatureIcon>
{
    using type = uint8_t;
};
template<>
struct tuple_element<1, tibia::creatures::TCreatureIcon>
{
    using type = uint32_t;
};
template<>
struct tuple_element<2, tibia::creatures::TCreatureIcon>
{
    using type = uint16_t;
};
}
