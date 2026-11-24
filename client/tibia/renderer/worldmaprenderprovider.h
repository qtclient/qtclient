#pragma once

#include "renderer/weatherframe.h"
#include "renderer/lightmodel.h"
#include "mods/worldview.h"
#include "renderer/spritetexturecache.h"
#include "renderer/worldgeometry.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapobjectcounter.h"

#include <client/declarations.h>
#include <client/position.h>
#include <QPoint>
#include <QPointF>

#include <QImage>
#include <QObject>
#include <QPoint>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace tibia {
namespace appearances {
class TAppearanceInstance;
class TSpritesheetAppearanceInstanceSpriteInfo;
class TAppearanceInstanceSpriteInfo;
using TAppearanceInstanceSpriteInfoPtr = std::shared_ptr<TAppearanceInstanceSpriteInfo>;
using TAppearanceInstancePtr = std::shared_ptr<TAppearanceInstance>;
}
}

namespace tibia {
namespace appearances {
class TAppearanceInstanceTracker;
}
namespace config {
class TClientOptions;
}
namespace creatures {
class TCreature;
class TCreatureStorage;
}
namespace effects {
class TEffectStorage;
}
namespace renderer {
class TRenderVolumeProvider;
struct TRenderInfo;
}
namespace worldmap {
class TAmbientLightStorage;
class TWorldMapViewport;
class TLightMap;
class TCacheForWorldmapStorageLookupMembers;
class TWorldMapStorage;
}
}

namespace tibia {
namespace qmlcomponents {

/// One sprite of the world (CIP: renderer::TAppearanceInstanceSpriteDrawCommandInfoCompact):
/// the frame build collects them for every floor, marks the ones hidden under
/// opaque objects of higher floors and sorts the rest by their key.
struct TAppearanceInstanceSpriteDrawCommandInfoCompact
{
    /// The packed sort key (floor, group, field, layer, draw index).
    uint64_t key = 0;
    /// One past the sprite's bottom-right pixel: things are anchored at their
    /// bottom-right corner, so the sprite's size is not needed to place it.
    QPoint anchor;
    /// The floor of the field the sprite is drawn for.
    int floor = 0;
    /// Every field the sprite covers is covered by an opaque object of a higher
    /// floor: the sprite is not drawn.
    bool obscured = false;
    /// Highlight drawn over the sprite.
    enum Highlight : uint8_t { NoHighlight, MouseTarget, AttackTarget, FollowTarget };
    Highlight highlight = NoHighlight;
    /// The sprite info drawn (its sprite size places it from the anchor).
    SpriteIdentifier sprite;
    float opacity = 1.f;
};
using SpriteRecord = TAppearanceInstanceSpriteDrawCommandInfoCompact;

/// The fields of the render volume, each holding the highest floor whose opaque
/// object covers it (CIP: the frame build's occlusion grid; 15 when none does).
/// Indexed by map pixel / 32 modulo its size.
struct TOcclusionGrid
{
    TOcclusionGrid(int volumeWidth, int volumeHeight);
    int& at(int column, int row);

    int width = 0;
    int height = 0;
    std::vector<int> floors;
};

/// Turns the game model into the world's frame (CIP: TWorldMapRenderProvider):
/// the sprite list, the light map and the creatures' HUD anchors. Built on the
/// GUI thread once per frame; the WorldMap reads it during the scene graph sync.
class TWorldMapRenderProvider : public QObject
{
    Q_OBJECT
public:
    /// A creature whose HUD (name, bars) is shown (CIP: TCreatureHUDRenderProvider's records).
    struct CreatureHud
    {
        uint32_t creatureId = 0;
        /// Map pixel of the creature's top-left, walk and elevation included; the
        /// HUD stands on it, centred one field to the right.
        QPoint point;
    };

    struct Frame
    {
        /// False while there is no world to show; everything else is then empty.
        bool valid = false;
        std::vector<SpriteRecord> sprites;
        std::vector<CreatureHud> creatures;
        /// One pixel per walked field (CIP: TLightMap), multiplied over the world.
        /// Pixel (0, 0) is the field at map pixel (0, 0).
        QImage light;
        /// Map pixel shown at the WorldMap's top-left.
        QPoint camera;
        /// How strongly highlighted sprites take the highlight colour; pulses
        /// with the game clock.
        float highlightStrength = 0.f;
        /// Visible FOV in fields (CIP MapWindowPane is always 15×11).
        int visibleWidth = 15;
        int visibleHeight = 11;
        /// CIP map scale (snapped height / 352). Overlays place with this, centered.
        double viewScale = 1;
        /// Shared roof / outdoor gather (weather + lighting). Empty when unused.
        WorldGeometry geometry;
    };

    /// Fallback FOV before the map pane reports a size (CIP 15×11).
    static constexpr int DefaultVisibleWidth = 15;
    static constexpr int DefaultVisibleHeight = 11;
    static constexpr int FieldSize = 32;

    int visibleWidth() const { return m_visibleWidth; }
    int visibleHeight() const { return m_visibleHeight; }
    int drawWidth() const { return m_visibleWidth + 3; }
    int drawHeight() const { return m_visibleHeight + 3; }
    int centerX() const { return drawWidth() / 2 - 1; }
    int centerY() const { return drawHeight() / 2 - 1; }

    void setVisibleFields(int width, int height);
    void setViewScale(double scale);

    const Frame& frame() const { return m_frame; }
    /// The last frame's sorted sprite records (CIP: the provider's draw command list).
    const std::vector<SpriteRecord>& drawCommands() const { return m_frame.sprites; }
    /// The last frame's light map; null before the first frame.
    std::shared_ptr<worldmap::TLightMap> lightMap() const { return m_lightMap; }
    /// Time the next frame is shown at; the light map's cloud waves run on it.
    void setFrameTimestamp(const qint64& timestamp) { m_frameTimestamp = timestamp; }
    /// The player's field of the last frame.
    const Position& camera() const { return m_camera; }
    /// The session's view (CIP: the render volume provider's viewport): the
    /// picker reads the player's floor and the visible area from it.
    void setViewport(worldmap::TWorldMapViewport* viewport) { m_viewport = viewport; }
    worldmap::TWorldMapViewport* viewport() const { return m_viewport; }
    /// The session's render volume provider: the drawn floors come from its volume.
    void setRenderVolumeProvider(std::shared_ptr<renderer::TRenderVolumeProvider> renderVolumeProvider)
    {
        m_renderVolumeProvider = std::move(renderVolumeProvider);
    }
    /// The field at the top-left of the walked area of floor `z`.
    Position firstField(int z) const;

    /// Light at a map pixel, 0 (dark) to 1, bilinear between field centres
    /// (CIP: TLightMap::v7).
    float lightnessAt(const QPoint& mapPixel) const;
    /// QColor::lightness of a field's light, 0 outside the light map and 255
    /// without one (CIP: TWorldMapRenderProvider::v14, TLightMap).
    int fieldLightness(const Position& field) const;

    /// Floors of the last frame, top-down: the picker looks through them in
    /// this order.
    uint8_t firstFloor() const { return m_firstFloor; }
    uint8_t lastFloor() const { return m_lastFloor; }
    /// The field of floor `z` drawn at a map pixel of the last frame.
    Position fieldAt(const QPoint& mapPixel, int z) const;
    /// Map pixel of a field's top-left corner.
    QPoint mapPixelOf(const Position& field) const;

    /// Item hover by CIP object client id + field + stackpos (CIP: v17).
    /// Clears creature mouse target. `typeId == 0` clears the object target.
    void setMouseTargetObject(uint16_t typeId, const Position& field, int stackPos);
    uint16_t mouseTargetObjectClientId() const { return m_mouseTargetTypeId; }
    const Position& mouseTargetObjectField() const { return m_mouseTargetField; }
    int mouseTargetObjectStackPos() const { return m_mouseTargetStackPos; }
    /// Creature hover by storage id (battle list / TEMPORARY_SELECT).
    void setMouseTargetCreatureId(uint32_t id);
    uint32_t mouseTargetCreatureId() const { return m_mouseTargetCreatureId; }
    /// Field under the cursor for the highlight baseplate (CIP: v21 / +0xe8).
    void setMouseHighlightField(const Position& field) { m_mouseHighlightField = field; }
    /// No field under the mouse: the highlight baseplate is not drawn.
    void clearMouseHighlightField() { m_mouseHighlightField = Position{}; }
    const Position& mouseHighlightField() const { return m_mouseHighlightField; }
    /// Option mouseHighlightTarget: highlight the mouse target; without it the
    /// selected creature keeps its white frame instead.
    void setMouseHighlightEnabled(bool enabled) { m_mouseHighlight = enabled; }
    /// Option gameWindowShowAttackAnimation: highlight the attack and follow targets.
    void setTargetHighlightEnabled(bool enabled) { m_targetHighlight = enabled; }
    /// What the last advance took (full build including light/raycast).
    qint64 buildNs() const { return m_buildNs; }
    /// Phase costs from the last build, for QTCLIENT_PERF / QTCLIENT_PERF.
    qint64 tilesNs() const { return m_tilesNs; }
    qint64 lightNs() const { return m_lightNs; }
    /// beginFrame / beginFloor / endFrame setup (excludes gather inside the tile walk).
    qint64 raycastNs() const { return m_raycastNs; } // pack lighting phase
    /// addFrame / resetShade during the tile walk.
    qint64 raycastGatherNs() const { return m_raycastGatherNs; }
    /// updateRoofs inside endFrame.
    qint64 raycastRoofsNs() const { return m_raycastRoofsNs; }
    /// Shared roof / outdoor geometry for the last frame (weather + lighting).
    const WorldGeometry& geometry() const { return m_frame.geometry; }
    /// Weather overlay inputs for the WorldMap node (empty when weather is off).
    WeatherFrame weatherFrame() const;

    /// Host: the session's provider, reading the session's storages, options
    /// and instance tracker.
    explicit TWorldMapRenderProvider(QObject* parent = nullptr);
    /// A provider of the volume of `renderVolumeProvider` drawn from the given
    /// storages; the appearance types of each frame go to `instanceTracker`,
    /// which loads their spritesheets.
    TWorldMapRenderProvider(std::shared_ptr<renderer::TRenderVolumeProvider> renderVolumeProvider,
                            std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                            std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                            std::shared_ptr<worldmap::TAmbientLightStorage> ambientLightStorage,
                            std::shared_ptr<effects::TEffectStorage> effectStorage,
                            std::shared_ptr<config::TClientOptions> clientOptions,
                            std::shared_ptr<appearances::TAppearanceInstanceTracker> instanceTracker,
                            QObject* parent = nullptr);

    /// The last frame's objects around the player, per type and distance ring
    /// (CIP: v4); the object sound controller reads it after the frame.
    const worldmap::TWorldMapObjectCounter& objectCounter() const { return m_objectCounter; }
    /// The appearance types the last frame drew.
    const std::set<const TAppearanceType*>& frameAppearanceTypes() const { return m_frameTypes; }

public Q_SLOTS:
    /// Builds the next frame from the game model; emits frameAdvanced.
    void advance();

Q_SIGNALS:
    void frameAdvanced();
    /// Sprite list and camera pixel are unchanged. Highlight nodes still pulse.
    void highlightAdvanced();
    /// HUD, speech, and light overlays.
    void overlaysAdvanced();
    void visibleFieldsChanged();

private:
    /// Where a sprite sorts (CIP: the record's 64-bit sort key). The frame's
    /// sprites are emitted in any order and sorted by floor (deepest first),
    /// group, field x+y, field x, layer, draw index; ties keep emission order.
    struct DrawKey
    {
        /// 1 bank/clip, 2 bottom/common/creature/top, 3 effect, 4 topeffect, 5 missile.
        uint8_t group = 2;
        /// Draw-grid field the sprite sorts with.
        int x = 0;
        int y = 0;
        /// The floor of the field.
        int z = 0;
        /// Layer within the field: 0 bank, 1 clip, 2 bottom, 3 common,
        /// 5 creature, 6 top, 7 effect (and the mouse baseplate), 8 missile.
        uint8_t layer = 3;
        /// Position in the field's draw list (stack with everything after the
        /// bank/clip/bottom prefix reversed); inverted in group 4.
        uint8_t index = 0;
        /// A creature or top object moved to a lying object's slot keeps its
        /// own draw index here.
        uint8_t own = 0;
        /// The record's flag (0 for every sprite qtclient draws).
        uint8_t flag = 0;
        uint64_t packed() const;
    };
    /// A lying object's slot taken by the creatures and top objects of the
    /// fields up, left and up-left of it.
    struct LyingSlot
    {
        /// The lying object's field and its draw-grid cell.
        worldmap::TWorldMapCoordinate field;
        int x = 0;
        int y = 0;
        uint8_t index = 0;
    };
    /// A field of the render volume and its draw-grid cell on its floor
    /// (CIP: the storage's field entries, coordinate and field).
    struct FieldEntry
    {
        worldmap::TWorldMapCoordinate coordinate;
        tibia::worldmap::TWorldMapField* field = nullptr;
        int x = 0;
        int y = 0;
    };
    /// What the sprite emitters produce: draw commands, the pack's lighting
    /// stamps, or both.
    enum Emit : uint8_t { EmitRecords = 1, EmitStamps = 2, EmitAll = EmitRecords | EmitStamps };

    /// The frame build: every floor's draw commands, sorted, and the light map.
    bool build();
    /// The provider draws the session's world (its world map storage is the
    /// live one), centred on the local player.
    bool drawsLiveWorld() const;
    void buildLight();
    /// Shared outdoor / roof gather when weather or raycast needs it.
    void updateWorldGeometry();
    /// Cool outdoor cells from active weather (D).
    void applyWeatherAmbientTint();
    void loadLightOptions();
    /// gameWindowShowLootHighlighting: containers draw their field effect.
    void loadLootHighlightOption();
    /// Recomputes the render volume for the camera's field; first and last
    /// drawn floor from it.
    void updateVisibleFloors(uint8_t& first, uint8_t& last);
    QPoint toScreen(const Position& pos) const;
    /// The field drawn at light map cell (x, y) of floor z.
    Position lightField(int x, int y, int z) const;

    /// The drawn fields of floors `last` up to `first`, in draw order.
    std::vector<FieldEntry> fieldsInVolume(uint8_t first, uint8_t last) const;
    /// Every field's draw commands, lights and occluders.
    void addFields(const std::vector<FieldEntry>& fields, uint8_t first, uint8_t last, TOcclusionGrid& grid);
    /// One field's draw commands.
    void addField(const FieldEntry& field, TOcclusionGrid& grid);
    void addTileBelow(const FieldEntry& field, const QPoint& dest, uint8_t& elevation, TOcclusionGrid& grid);
    void addTileCreatures(const FieldEntry& field, const QPoint& dest, uint8_t elevation, TOcclusionGrid& grid);
    void addTileEffects(tibia::worldmap::TWorldMapField* tile, const QPoint& dest);
    void addTileTop(const FieldEntry& field, const QPoint& dest, TOcclusionGrid& grid);
    void addLootHighlight(const FieldEntry& field, const QPoint& dest);
    /// One object of a field: its draw commands with their key, its light,
    /// the fields it hides, its count near the player and its type.
    void addObject(const FieldEntry& field, int stackIndex, const QPoint& dest, uint8_t& elevation, bool castHeight,
                   TOcclusionGrid& grid);
    /// The missiles of the drawn floors (after every field).
    void addMissiles(uint8_t first, uint8_t last);
    /// The pack lighting's stamps and lights of floor `z`'s missiles.
    void gatherMissileLighting(int z);
    /// The square under the mouse.
    void addHighlightBaseplate(std::vector<SpriteRecord>& records);
    /// Lowers the floor of the whole fields `record` covers to `floor`.
    void occlude(TOcclusionGrid& grid, const SpriteRecord& record, int floor) const;
    /// Marks the draw commands every field of which is covered from a higher floor.
    void markObscuredDrawCommands(TOcclusionGrid& grid, std::vector<SpriteRecord>& records) const;
    /// The visible draw commands of `records` in key order into `sorted`.
    void sortDrawCommands(std::vector<SpriteRecord>& records, std::vector<SpriteRecord>& sorted);
    /// Appends a draw command with the current key and floor.
    void pushRecord(SpriteRecord record);
    /// The key of a sprite of `kind` at draw-grid (x, y).
    static DrawKey keyOfKind(int kind, int x, int y, uint8_t index, int floor);
    static bool takesLyingSlot(int kind);
    /// Index of `stackIndex` in the field's draw list.
    static uint8_t drawIndex(const tibia::worldmap::TWorldMapField& tile, int stackIndex);
    /// Draw-grid cell of a field on the current floor.
    QPoint gridCell(const Position& field) const;
    /// Rebuilds m_lyingSlots for the drawn fields.
    void mapLyingObjects(const std::vector<FieldEntry>& fields);
    /// Key for a creature or top object of `field` at draw-grid (x, y): moved
    /// into the slot of a lying object down or right of it.
    DrawKey slotKey(const worldmap::TWorldMapCoordinate& field, int x, int y, uint8_t layer, uint8_t index) const;
    /// What one instance on `coordinate` draws, render info by render info
    /// (an object's, a creature placeholder's, else the instance's own sprite):
    /// each info with a sprite counts its type near the player, draws with the
    /// current key at `dest` (the field's corner lifted by `lift`) moved to the
    /// field the info lands on, tinted by `highlight` unless it is a creature
    /// mark or the baseplate (a creature's infos otherwise by its attack /
    /// follow state), lights the map when its type shines, a creature's also
    /// with its own, its outfit's and its mount's light, hides the lower floors
    /// under the fields an opaque sprite at its field's corner covers, and
    /// keeps its type (a creature's outfit and mount types).
    void addRenderInfos(const appearances::TAppearanceInstance& instance, const worldmap::TWorldMapCoordinate& coordinate,
                        const QPoint& dest, SpriteRecord::Highlight highlight, uint8_t lift,
                        tibia::worldmap::TWorldMapField* tile, bool castHeight, TOcclusionGrid& grid);
    /// CIP v10: mouse-target tint when stack entry matches hover object id (not creature).
    SpriteRecord::Highlight itemHighlightOf(const Position& field, int stackIndex, uint16_t typeId) const;
    void addEffectFromAppearance(const appearances::TAppearanceInstancePtr& appearance, const QPoint& dest,
                                 uint8_t& elevation);
    /// A creature standing on `coordinate` (its corner at `dest`, lifted by
    /// `lift`): its placeholder's render infos, with the host's HUD entry,
    /// jump, attached effects and pack lighting context.
    void addCreature(const tibia::creatures::TCreature& creature, const worldmap::TWorldMapCoordinate& coordinate,
                     const QPoint& dest, uint8_t lift, bool castHeight, TOcclusionGrid& grid);
    void addFieldStackCreature(tibia::worldmap::TWorldMapField* tile, int stackIndex, const QPoint& dest, uint8_t elevation,
                               bool castHeight, TOcclusionGrid& grid);
    void addMissile(const appearances::TAppearanceInstancePtr& appearance);
    /// Effects below this opacity are not shown at all (CIP:).
    /// Opacity itself is baked on the appearance at arrival (CIP).
    static constexpr float MinEffectOpacity = 0.01f;
    /// The sprites of `info` (each layer of a layered info) at `dest`; with
    /// raycast lighting, `stamp`'s sprites stamp the light frame.
    /// Draws one render info handed by an appearance instance on the field at `dest`.
    void addRenderInfo(const renderer::TRenderInfo& info, const QPoint& dest, SpriteRecord::Highlight highlight);
    void addSpriteInfo(TAppearanceType* type, const QPoint& dest, const appearances::TAppearanceInstanceSpriteInfoPtr& info,
                       const appearances::TAppearanceInstanceSpriteInfoPtr& stamp, int xPattern,
                       SpriteRecord::Highlight highlight, float opacity = 1.f, uint8_t tint = 0);
    /// The current sprite of `instance`'s sequence at `dest` (effects and
    /// missiles); `phase` picks the raycast stamp.
    void addSequenceSprite(const appearances::TAppearanceInstance& instance, const QPoint& dest, int phase, int xPattern,
                           float opacity, uint8_t tint, SpriteRecord::Highlight highlight = SpriteRecord::NoHighlight);
    /// Host: the sprite of `type`'s pattern and phase (things without an
    /// instance's sprite sequence: placeholders, mounts, show-off figures,
    /// attached effects); outfits colourized through their mask layer.
    void addSprites(TAppearanceType* type, const QPoint& dest, int xPattern, int yPattern, int zPattern,
                    int animationPhase, bool colorize = false, uint32_t colors = 0,
                    SpriteRecord::Highlight highlight = SpriteRecord::NoHighlight, float opacity = 1.f, uint8_t tint = 0);
    /// The creature's attached effects on one side of its outfit (product
    /// Creature::internalDraw): at its walked field, or where it is drawn
    /// (`jumped`) for those following it.
    void addAttachedEffects(const tibia::creatures::TCreature& stored, const QPoint& walked, const QPoint& jumped,
                            bool onTop);
    SpriteRecord::Highlight highlightOf(uint32_t creatureId) const;
    void addLight(const QPoint& center, uint8_t intensity, uint8_t color);
    /// Builds the frame's light map (CIP: v7); without `withLights` only the floors' light.
    std::shared_ptr<worldmap::TLightMap> buildLightMap(bool withLights);

    /// The storages drawn from (the session's when not given).
    worldmap::TWorldMapStorage* worldMapStorage() const;
    creatures::TCreatureStorage* creatureStorage() const;
    worldmap::TAmbientLightStorage* ambientLightStorage() const;
    /// The tracker the frame's appearance types go to (the factory's when not given).
    std::shared_ptr<appearances::TAppearanceInstanceTracker> instanceTracker() const;

    std::shared_ptr<renderer::TRenderVolumeProvider> m_renderVolumeProvider;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<worldmap::TAmbientLightStorage> m_ambientLightStorage;
    std::shared_ptr<effects::TEffectStorage> m_effectStorage;
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<appearances::TAppearanceInstanceTracker> m_instanceTracker;
    Frame m_frame;
    Position m_camera;
    worldmap::TWorldMapViewport* m_viewport = nullptr;
    int m_visibleWidth = DefaultVisibleWidth;
    int m_visibleHeight = DefaultVisibleHeight;
    double m_viewScale = 1;
    int m_floor = 0;
    uint8_t m_firstFloor = 0;
    uint8_t m_lastFloor = 0;
    uint16_t m_mouseTargetTypeId = 0;
    Position m_mouseTargetField;
    int m_mouseTargetStackPos = -1;
    uint32_t m_mouseTargetCreatureId = 0;
    Position m_mouseHighlightField;
    bool m_mouseHighlight = true;
    bool m_targetHighlight = true;
    uint32_t m_attackTarget = 0;
    uint32_t m_followTarget = 0;
    /// The frame's draw commands of every floor, before the sort.
    std::vector<SpriteRecord> m_records;
    /// The visible records' keys and indices, sorted.
    std::vector<std::pair<uint64_t, uint32_t>> m_sortOrder;
    /// Key given to the sprites emitted next.
    DrawKey m_key;
    Emit m_emit = EmitAll;
    /// Map pixel of draw-grid pixel (0, 0) on the camera's floor.
    QPoint m_mapPixelOrigin;
    /// The pack's lighting gathers this frame.
    bool m_gatherRaycast = false;
    /// Lying-object slots by the field they are lent to.
    std::map<worldmap::TWorldMapCoordinate, LyingSlot> m_lyingSlots;
    worldmap::TWorldMapObjectCounter m_objectCounter;
    std::set<const TAppearanceType*> m_frameTypes;
    std::vector<light::PointLight> m_lights;
    light::LightOptions m_lightOptions;
    bool m_showLootHighlighting{ true };
    uint32_t m_coverRevision = 0;
    qint64 m_buildNs = 0;
    qint64 m_tilesNs = 0;
    qint64 m_lightNs = 0;
    qint64 m_raycastNs = 0;
    qint64 m_raycastGatherNs = 0;
    qint64 m_raycastRoofsNs = 0;
    /// Why this build ran (for slow-tile / ~1 Hz dumps).
    bool m_cameraTileMoved = false;
    bool m_coverDirty = false;
    /// Cached first/last floor while camera tile and cover revision hold.
    uint8_t m_cachedFirstFloor = 0;
    uint8_t m_cachedLastFloor = 0;
    bool m_floorsCached = false;
    /// Classic light before weather tint (D); tint reapplies each frame from this.
    QImage m_lightUntinted;
    /// The light of the last frame (CIP: the provider's TLightMap).
    std::shared_ptr<worldmap::TLightMap> m_lightMap;
    qint64 m_frameTimestamp = 0;
    /// Memos of the storage questions the light map asks: banks, translucent
    /// fields, and (while indoor fields are darkened) open sky.
    worldmap::TWorldMapStorage* m_lightCacheStorage = nullptr;
    std::shared_ptr<worldmap::TCacheForWorldmapStorageLookupMembers> m_bankCache;
    std::shared_ptr<worldmap::TCacheForWorldmapStorageLookupMembers> m_translucentCache;
    std::shared_ptr<worldmap::TCacheForWorldmapStorageLookupMembers> m_openSkyCache;
    /// Cache key for shared world geometry.
    Position m_geometryCamera;
    uint32_t m_geometryRevision = 0;
    int m_geometryWidth = 0;
    int m_geometryHeight = 0;
    uint8_t m_geometryFirst = 0;
    uint8_t m_geometryLast = 0;
    int m_geometryOrigin = -1;
    /// Cached at advance start — null when no pack registered a driver.
    tibia::mods::WorldLightingDriver* m_lighting = nullptr;
};

}
}
