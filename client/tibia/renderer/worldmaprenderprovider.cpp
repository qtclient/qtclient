#include <tibia/appearances/appearancetype.h>
#include "worldmaprenderprovider.h"
#include "tibia/runtime/host.h"

#include "appearances/appearanceinstance.h"
#include "appearances/appearanceinstancetracker.h"
#include "appearances/creatureplaceholderappearanceinstance.h"
#include "appearances/spriteinfo.h"
#include "renderer/spritetextureprovider.h"
#include "appearances/worldmaphighlightbaseplate.h"
#include "app/bootstrap.h"
#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "mods/registry.h"
#include "mods/worldview.h"
#include "renderer/lightmodel.h"
#include "gamewindow/contextmenucontroller.h"
#include "renderer/renderinfo.h"
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldframeview.h"
#include "worldmap/lightmap.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/cacheforworldmapstoragelookupmembers.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>

#include <tibia/creatures/creaturedecoration.h>
#include <tibia/creatures/movementtweens.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "creatures/creaturestorage.h"
#include "worldmap/worldmapfield.h"
#include "clockservice.h"

#include <QColor>

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <vector>
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "game/mapsession.h"
#include <QByteArray>
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

namespace tibia::qmlcomponents {


namespace {

constexpr int FieldSize = TWorldMapRenderProvider::FieldSize;

/// Replay / screenshot harness only: `QTCLIENT_DEBUG_WORLD_LIGHT=<intensity>[:<color>]`
/// pins the world ambient light (session dumps keep whatever time they were recorded at).
struct DebugWorldLight
{
    bool active = false;
    uint8_t intensity = 0;
    bool hasColor = false;
    uint8_t color = 0;
};

const DebugWorldLight& debugWorldLight()
{
    static const DebugWorldLight pinned = [] {
        DebugWorldLight out;
        const QByteArray spec = qgetenv("QTCLIENT_DEBUG_WORLD_LIGHT");
        if (spec.isEmpty())
            return out;
        const QList<QByteArray> parts = spec.split(':');
        bool ok = false;
        const int intensity = parts.value(0).trimmed().toInt(&ok);
        if (!ok)
            return out;
        out.active = true;
        out.intensity = static_cast<uint8_t>(std::clamp(intensity, 0, 255));
        if (parts.size() > 1) {
            out.hasColor = true;
            out.color = static_cast<uint8_t>(std::clamp(parts.at(1).trimmed().toInt(), 0, 255));
        }
        return out;
    }();
    return pinned;
}

tibia::mods::LightingMapPos lightingCamera(const Position& camera)
{
    return { camera.x, camera.y, camera.z };
}

/// Host (pack lighting): an object of a field's stack, with its CipSoft type data.
tibia::mods::LightingThingContext itemLightingContext(const TAppearanceType& type, const uint8_t lift,
                                                      const int stackIndex)
{
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.loose = !type.isNotMoveable();
    ctx.typeId = type.getId();
    ctx.typeFlags = appearanceFlagsOf(&type);
    ctx.stackIndex = static_cast<int16_t>(stackIndex);
    return ctx;
}

/// Host (pack lighting): a creature; `stackIndex` -1 when it is drawn on a field it does not stand on.
tibia::mods::LightingThingContext creatureLightingContext(const tibia::creatures::TCreature& stored,
                                                          const uint8_t lift, const int stackIndex)
{
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.creature = true;
    ctx.walking = stored.isWalking();
    ctx.localPlayer = stored.isLocalPlayer();
    ctx.creatureId = stored.creatureId();
    ctx.stackIndex = static_cast<int16_t>(stackIndex);
    return ctx;
}

/// The creature carries creature icon `icon` (not a player icon) with a value.
bool hasCreatureIcon(const tibia::creatures::TCreature& creature, const uint8_t icon)
{
    return std::any_of(creature.hudIcons().begin(), creature.hudIcons().end(), [icon](const auto& entry) {
        const auto& [id, category, count] = entry;
        return category != 0 && id == icon;
    });
}

/// The light a creature casts. A creature that casts less than 2 but must stay
/// visible in the dark (the local player, raid leaders and minions, a creature
/// with icon 5) casts a white light of 2 instead.
Light creatureLight(const tibia::creatures::TCreature& creature)
{
    Light light = creature.light();
    if (light.intensity < 2
        && (creature.hasStatusMark(0) || creature.hasStatusMark(1) || hasCreatureIcon(creature, 5)
            || creature.isLocalPlayer())) {
        light.intensity = 2;
        light.color = 215; // white
    }
    return light;
}

tibia::mods::LightingThingContext transientLightingContext(const uint8_t lift = 0)
{
    // Effects / missiles.
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.transient = true;
    return ctx;
}

/// Host (pack lighting): the CipSoft data of a frame the driver stamps.
tibia::mods::LightingFrameDesc peelLightingFrame(const TAppearanceType& type, const QPoint& anchor, const uint32_t spriteId,
                                                 const int xPattern)
{
    tibia::mods::LightingFrameDesc desc;
    desc.spriteId = spriteId;
    desc.anchorX = anchor.x();
    desc.anchorY = anchor.y();
    desc.displacementX = type.getDisplacement().x();
    desc.displacementY = type.getDisplacement().y();
    desc.elevation = static_cast<uint8_t>(std::clamp(type.getElevation(), 0, 255));
    desc.hook = static_cast<int8_t>(type.isHangable() ? (type.getNumPatternX() >= 2 ? xPattern : 0) : -1);
    desc.onTop = type.isOnTop();
    desc.lyingCorpse = type.isLyingCorpse() || type.isCorpse() || type.isPlayerCorpse();
    desc.hasLight = type.hasLight();
    if (desc.hasLight) {
        desc.lightIntensity = type.getLight().intensity;
        desc.lightColor = type.getLight().color;
    }
    desc.typeId = type.getId();
    desc.typeFlags = appearanceFlagsOf(&type);
    return desc;
}

}

TWorldMapRenderProvider::TWorldMapRenderProvider(QObject* parent)
    : TWorldMapRenderProvider(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, parent)
{
}

TWorldMapRenderProvider::TWorldMapRenderProvider(
    std::shared_ptr<renderer::TRenderVolumeProvider> renderVolumeProvider,
    std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage, std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
    std::shared_ptr<worldmap::TAmbientLightStorage> ambientLightStorage, std::shared_ptr<effects::TEffectStorage> effectStorage,
    std::shared_ptr<config::TClientOptions> clientOptions,
    std::shared_ptr<appearances::TAppearanceInstanceTracker> instanceTracker, QObject* parent)
    : QObject(parent)
    , m_renderVolumeProvider(std::move(renderVolumeProvider))
    , m_worldMapStorage(std::move(worldMapStorage))
    , m_creatureStorage(std::move(creatureStorage))
    , m_ambientLightStorage(std::move(ambientLightStorage))
    , m_effectStorage(std::move(effectStorage))
    , m_clientOptions(std::move(clientOptions))
    , m_instanceTracker(std::move(instanceTracker))
{
    loadLightOptions();
    loadLootHighlightOption();
    if (!m_clientOptions)
        return;
    connect(m_clientOptions.get(), &config::TClientOptions::optionChanged, this, [this](const config::EClientOption id) {
        switch (id) {
        case config::EClientOption::lightAmbientLevel:
        case config::EClientOption::lightLevelSeparatorLevel:
        case config::EClientOption::lightAttenuationCloudsIndoor:
        case config::EClientOption::lightEffectsEnabled:
            loadLightOptions();
            break;
        case config::EClientOption::gameWindowShowLootHighlighting:
            loadLootHighlightOption();
            break;
        default:
            break;
        }
    });
}

worldmap::TWorldMapStorage* TWorldMapRenderProvider::worldMapStorage() const
{
    return m_worldMapStorage.get();
}

creatures::TCreatureStorage* TWorldMapRenderProvider::creatureStorage() const
{
    return m_creatureStorage.get();
}

worldmap::TAmbientLightStorage* TWorldMapRenderProvider::ambientLightStorage() const
{
    return m_ambientLightStorage.get();
}

std::shared_ptr<appearances::TAppearanceInstanceTracker> TWorldMapRenderProvider::instanceTracker() const
{
    return m_instanceTracker;
}

void TWorldMapRenderProvider::setVisibleFields(const int width, const int height)
{
    const int w = std::max(3, width);
    const int h = std::max(3, height);
    if (w == m_visibleWidth && h == m_visibleHeight)
        return;
    m_visibleWidth = w;
    m_visibleHeight = h;
    Q_EMIT visibleFieldsChanged();
}

void TWorldMapRenderProvider::setViewScale(const double scale)
{
    m_viewScale = scale > 0 ? scale : 1;
}

void TWorldMapRenderProvider::setMouseTargetObject(const uint16_t typeId, const Position& field, const int stackPos)
{
    // The target under the mouse stays while a context menu is shown.
    if (const gamewindow::TContextMenuController* menu = gamewindow::TContextMenuController::instance(); menu && menu->isShown())
        return;
    m_mouseTargetTypeId = typeId;
    m_mouseTargetField = typeId ? field : Position{};
    m_mouseTargetStackPos = typeId ? stackPos : -1;
    m_mouseTargetCreatureId = 0;
}


void TWorldMapRenderProvider::setMouseTargetCreatureId(const uint32_t id)
{
    if (const gamewindow::TContextMenuController* menu = gamewindow::TContextMenuController::instance(); menu && menu->isShown())
        return;
    m_mouseTargetCreatureId = id;
    m_mouseTargetTypeId = 0;
    m_mouseTargetField = {};
    m_mouseTargetStackPos = -1;
}

void TWorldMapRenderProvider::loadLightOptions()
{
    m_lightOptions = light::loadOptions();
}

void TWorldMapRenderProvider::loadLootHighlightOption()
{
    m_showLootHighlighting =
        app::ClientOptions::load().options().value(QLatin1String("gameWindowShowLootHighlighting")).toBool(true);
}

void TWorldMapRenderProvider::advance()
{
    m_lighting = tibia::mods::worldLightingDriver();

    QElapsedTimer cost;
    cost.start();
    m_frame.valid = build();
    m_buildNs = cost.nsecsElapsed();
    // The view follows the drawn frame's field, so the picker's floor and
    // visible area match the frame's pixels (the full map centres it on the
    // server's field; qtclient draws around the player, steps included).
    if (m_frame.valid && m_viewport)
        m_viewport->setCenter(worldmap::TWorldMapSubfieldCoordinate::fromField(worldmap::TWorldMapCoordinate(m_camera), 0, 0));
    if (!m_frame.valid) {
        m_frame.sprites.clear();
        m_frame.creatures.clear();
        // Host: an extension's frame hooks learn the world is gone.
        const tibia::mods::WorldFrameHooks& hooks = tibia::mods::worldFrameHooks();
        if (hooks.afterBuild && drawsLiveWorld()) {
            const QtClientWorldFrameView view{};
            hooks.afterBuild(hooks.user, &view);
        }
    }
    // CIP rebuilds the sprite list every frame; QSG reconciles in place.
    if (m_frame.valid) {
        Q_EMIT frameAdvanced();
        Q_EMIT overlaysAdvanced();
    }
}

TOcclusionGrid::TOcclusionGrid(const int volumeWidth, const int volumeHeight)
    : width(volumeWidth + 2)
    , height(volumeHeight + 2)
    , floors(static_cast<size_t>(std::max(0, width * height)), 15)
{
}

int& TOcclusionGrid::at(const int column, const int row)
{
    return floors.at(static_cast<size_t>(column % width + (row % height) * width));
}

namespace {

/// The pixels a draw command covers, inclusive (CIP: the record's anchor and
/// its sprite's size): left, top, right, bottom.
std::array<int, 4> drawCommandRect(const SpriteRecord& record)
{
    const QSize size = spriteSizeOf(record.sprite);
    const QPoint topLeft = record.anchor - QPoint(size.width(), size.height());
    return { topLeft.x(), topLeft.y(), topLeft.x() + size.width() - 1, topLeft.y() + size.height() - 1 };
}

}

void TWorldMapRenderProvider::occlude(TOcclusionGrid& grid, const SpriteRecord& record, const int floor) const
{
    // Only the whole fields the sprite covers; the grid is in map pixels.
    const auto [left, top, right, bottom] = drawCommandRect(record);
    const int x = m_mapPixelOrigin.x() + left;
    const int y = m_mapPixelOrigin.y() + top;
    const int height = bottom - top + 1;
    const int width = right - left + 1;
    if (height < FieldSize || width < FieldSize)
        return;
    for (int row = 0; row < height / FieldSize; ++row) {
        for (int column = 0; column < width / FieldSize; ++column) {
            int& cell = grid.at(x / FieldSize + column, y / FieldSize + row);
            cell = std::min(cell, floor);
        }
    }
}

void TWorldMapRenderProvider::markObscuredDrawCommands(TOcclusionGrid& grid, std::vector<SpriteRecord>& records) const
{
    for (SpriteRecord& record : records) {
        const auto [left, top, right, bottom] = drawCommandRect(record);
        const int x = m_mapPixelOrigin.x() + left;
        const int y = m_mapPixelOrigin.y() + top;
        // Only sprites at a field's corner can be covered by whole fields; a
        // sprite whose size is not known yet stays.
        if ((x & 0x1f) != 0 || (y & 0x1f) != 0 || right < left || bottom < top) {
            record.obscured = false;
            continue;
        }
        const int height = bottom - top + 1;
        const int width = right - left + 1;
        bool obscured = true;
        if (height >= FieldSize && width >= FieldSize) {
            for (int row = 0; obscured && row < height / FieldSize; ++row) {
                for (int column = 0; column < width / FieldSize; ++column) {
                    if (grid.at((x >> 5) + column, (y >> 5) + row) >= record.floor) {
                        obscured = false;
                        break;
                    }
                }
            }
        }
        record.obscured = obscured;
    }
}

void TWorldMapRenderProvider::sortDrawCommands(std::vector<SpriteRecord>& records, std::vector<SpriteRecord>& sorted)
{
    // Obscured sprites are dropped; the rest sort by key, then emission order.
    m_sortOrder.clear();
    for (uint32_t i = 0; i < records.size(); ++i) {
        if (!records[i].obscured)
            m_sortOrder.emplace_back(records[i].key, i);
    }
    std::sort(m_sortOrder.begin(), m_sortOrder.end());
    sorted.clear();
    sorted.reserve(m_sortOrder.size());
    for (const auto& [key, index] : m_sortOrder)
        sorted.push_back(std::move(records[index]));
}

namespace {

/// The render volume holds fields, and its first and last floor's origins,
/// moved onto its shear floor, are not below (0, 0, 0).
bool isDrawableRenderVolume(const worldmap::TShearedWorldMapCuboid& volume)
{
    if (volume.getExtent().volume() == 0)
        return false;
    const worldmap::TWorldMapCoordinate zero(0, 0, 0);
    if (volume.getOrigin().shiftedTowardsFloor(volume.shearFloor) < zero)
        return false;
    const worldmap::TWorldMapCoordinate lastFloorEnd = volume.getEnd() - worldmap::TWorldMapExtent(0, 0, 1);
    return !(lastFloorEnd.shiftedTowardsFloor(volume.shearFloor) < zero);
}

int floorDivFieldSize(const int subfield)
{
    return subfield >= 0 ? subfield / 32 : -((-subfield + 31) / 32);
}

}

bool TWorldMapRenderProvider::drawsLiveWorld() const
{
    const worldmap::TWorldMapStorage* current = game::currentWorldMapStorage();
    return current && worldMapStorage() == current;
}

bool TWorldMapRenderProvider::build()
{
    // The session's world is drawn around the local player; any other world
    // (a preview volume) around its render volume's viewport centre.
    const bool live = drawsLiveWorld();
    m_lighting = live ? tibia::mods::worldLightingDriver() : nullptr;

    Frame& frame = m_frame;
    const int DrawWidth = drawWidth();
    const int DrawHeight = drawHeight();
    const int CenterX = centerX();
    const int CenterY = centerY();
    frame.visibleWidth = m_visibleWidth;
    frame.visibleHeight = m_visibleHeight;
    frame.viewScale = m_viewScale;
    const size_t previousSprites = frame.sprites.size();
    const size_t previousCreatures = frame.creatures.size();
    m_tilesNs = m_lightNs = m_raycastNs = m_raycastGatherNs = m_raycastRoofsNs = 0;
    m_cameraTileMoved = false;
    m_coverDirty = false;
    // The objects near the player are counted anew every frame, around the
    // local player when there is one.
    m_objectCounter.reset();
    const std::shared_ptr<worldmap::TWorldMapViewport> volumeViewport =
        m_renderVolumeProvider ? m_renderVolumeProvider->worldMapViewport() : nullptr;
    const game::TGameSessionBase* session = game::currentGameSession();
    worldmap::TWorldMapStorage* const map = worldMapStorage();
    if (!map
        || (live ? (!session || !session->hasLocalPlayer() || !session->isOnline() || !session->localPlayerPosition().isValid())
                 : !volumeViewport)) {
        frame.sprites.clear();
        frame.creatures.clear();
        m_lights.clear();
        return false;
    }

    // Cover caches invalidate per opaque cell (TWorldMapStorage::invalidateCoverCachesFrom).
    const uint32_t coverRevision = live ? map->coverRevision() : m_coverRevision;
    Position camera;
    QPoint walk;
    if (live) {
        camera = session->localPlayerPosition();
        const creatures::TCreatureStorage* creatureRows = creatureStorage();
        walk = tibia::creatures::walkOffsetFor(creatureRows ? creatureRows->playerID() : 0);
    } else {
        // The field under the viewport's centre, and the centre's offset in it.
        const worldmap::TWorldMapSubfieldCoordinate centre = volumeViewport->center();
        const worldmap::TWorldMapCoordinate field(floorDivFieldSize(centre.subfieldX), floorDivFieldSize(centre.subfieldY),
                                                  centre.z);
        camera = field.toPosition();
        walk = QPoint(centre.subfieldX - field.x * FieldSize, centre.subfieldY - field.y * FieldSize);
    }
    const bool cameraTileMoved = m_camera != camera;
    const bool coverDirty = coverRevision != m_coverRevision;
    m_cameraTileMoved = cameraTileMoved;
    m_coverDirty = coverDirty;
    m_coverRevision = coverRevision;
    m_camera = camera;
    frame.camera = QPoint((CenterX - (m_visibleWidth - 1) / 2) * FieldSize + walk.x(),
                          (CenterY - (m_visibleHeight - 1) / 2) * FieldSize + walk.y());

    // The render volume is recomputed on a camera step or an opaque cover change.
    // No hysteresis — delayed hide left upper floors stuck on.
    uint8_t first;
    uint8_t last;
    if (!m_floorsCached || cameraTileMoved || coverDirty) {
        updateVisibleFloors(first, last);
        m_cachedFirstFloor = first;
        m_cachedLastFloor = last;
        m_floorsCached = true;
    } else {
        first = m_cachedFirstFloor;
        last = m_cachedLastFloor;
    }
    m_firstFloor = first;
    m_lastFloor = last;
    // Nothing is built from an empty render volume or one that reaches below
    // the origin: the last frame stays.
    if (m_renderVolumeProvider && !isDrawableRenderVolume(m_renderVolumeProvider->renderVolume()))
        return frame.valid;

    frame.highlightStrength = renderer::highlightStrength();
    m_attackTarget = session ? session->attackingCreatureId() : 0;
    m_followTarget = session ? session->followingCreatureId() : 0;

    QElapsedTimer phase;
    phase.start();
    // CIP every-frame full emit (classic and raycast). Drain map dirty for cover-flood
    // coalescing; positions are unused because the whole FOV is remitted.
    (void)map->clearStaticDirtyTiles();

    m_objectCounter.setCentre(worldmap::TWorldMapCoordinate(m_camera));

    const Position originField = m_camera.translated(-CenterX, -CenterY);
    m_mapPixelOrigin = QPoint(originField.x * FieldSize, originField.y * FieldSize);

    frame.creatures.clear();
    if (previousCreatures)
        frame.creatures.reserve(previousCreatures);
    // The previous frame's count is a good guess for this one.
    m_records.clear();
    m_records.reserve(std::max(previousSprites, static_cast<size_t>(DrawWidth * DrawHeight)));
    m_lights.clear();
    m_frameTypes.clear();

    m_gatherRaycast = (m_lighting && m_lighting->enabled())
        && m_lighting->beginFrame(lightingCamera(m_camera), DrawWidth, DrawHeight, first, last);

    // Share opaque/top-ground probes across FOV cover walks (cold-cache cliffs).
    map->beginCoverProbeBatch();
    const std::vector<FieldEntry> fields = fieldsInVolume(first, last);
    // The grid spans the render volume plus a field on every side (at least
    // the draw grid the fields are walked in, so no two cells share an entry).
    const worldmap::TWorldMapExtent volume = m_renderVolumeProvider
        ? m_renderVolumeProvider->renderVolume().unsheared().cuboid().getExtent()
        : worldmap::TWorldMapExtent(DrawWidth, DrawHeight);
    TOcclusionGrid grid(std::max(volume.width, DrawWidth), std::max(volume.height, DrawHeight));
    addFields(fields, first, last, grid);
    map->endCoverProbeBatch();
    addMissiles(first, last);
    addHighlightBaseplate(m_records);
    markObscuredDrawCommands(grid, m_records);
    // The frame's appearance types keep their spritesheets loaded.
    if (const std::shared_ptr<appearances::TAppearanceInstanceTracker> tracker = instanceTracker())
        tracker->loadSpritesheetsOf(m_frameTypes);
    sortDrawCommands(m_records, frame.sprites);
    const qint64 walkNs = phase.nsecsElapsed();
    m_raycastGatherNs = m_lighting ? m_lighting->gatherNs() : 0;
    m_tilesNs = std::max<qint64>(0, walkNs - m_raycastGatherNs);

    phase.restart();
    buildLight();
    m_lightNs = phase.nsecsElapsed();

    phase.restart();
    if (m_lighting)
        m_lighting->endFrame();
    m_raycastRoofsNs = m_lighting ? m_lighting->roofsNs() : 0;
    m_raycastNs += phase.nsecsElapsed();
    return true;
}

QtClientWorldFrameView TWorldMapRenderProvider::frameView() const
{
    QtClientWorldFrameView view{};
    view.valid = 1;
    view.camera = { m_camera.x, m_camera.y, m_camera.z };
    view.draw_width = drawWidth();
    view.draw_height = drawHeight();
    view.center_x = centerX();
    view.center_y = centerY();
    view.first_floor = m_firstFloor;
    view.last_floor = m_lastFloor;
    const worldmap::TWorldMapStorage* storage = worldMapStorage();
    view.item_revision = storage ? storage->itemRevision() : 0;
    view.clock_start_millis = static_cast<uint64_t>(tibia::shared::TClockService::startMillis());
    return view;
}

std::shared_ptr<worldmap::TLightMap> TWorldMapRenderProvider::buildLightMap(const bool withLights)
{
    using worldmap::TCacheForWorldmapStorageLookupMembers;
    using worldmap::TWorldMapCoordinate;
    worldmap::TWorldMapStorage* storage = worldMapStorage();
    if (storage != m_lightCacheStorage) {
        m_bankCache.reset();
        m_translucentCache.reset();
        m_openSkyCache.reset();
        m_lightCacheStorage = storage;
    }

    const int sea = worldmap::TWorldMapCentralPosition::GroundFloor;
    const tibia::worldmap::TAmbientLightStorage* ambient = ambientLightStorage();
    const DebugWorldLight& pinned = debugWorldLight();
    const worldmap::TWorldMapStorage* worldStorage = worldMapStorage();
    const Light serverLight = worldStorage ? worldStorage->worldLight() : Light{};
    const Light world{ pinned.active ? pinned.intensity : serverLight.intensity, pinned.hasColor ? pinned.color : serverLight.color };
    const QColor ambientColor = ambient ? ambient->color() : QColor(light::lightColor(world.color, world.intensity / 255.f));
    // The grid's origin: the visible area's top-left, slid along the floors' shear to the camera's floor.
    const TWorldMapCoordinate origin(m_camera.x - centerX(), m_camera.y - centerY(), m_camera.z);
    auto map = std::make_shared<worldmap::TLightMap>(
        m_lightOptions.ambientLevel, m_lightOptions.separator, m_lightOptions.attenuation, m_lightOptions.attenuation,
        origin, drawWidth(), drawHeight(), ambientColor, m_camera.z <= sea, m_frameTimestamp);

    std::map<int, std::vector<const light::PointLight*>> lightsByFloor;
    if (withLights) {
        for (const light::PointLight& source : m_lights)
            lightsByFloor[source.z].push_back(&source);
    }

    if (!m_bankCache) {
        m_bankCache = std::make_shared<TCacheForWorldmapStorageLookupMembers>(
            storage, [storage](const TWorldMapCoordinate& c) { return storage->hasBankAt(c); });
    }
    if (!m_translucentCache) {
        m_translucentCache = std::make_shared<TCacheForWorldmapStorageLookupMembers>(
            storage, [storage](const TWorldMapCoordinate& c) { return storage->hasTranslucentAt(c); });
    }
    if (map->darkensIndoors()) {
        if (!m_openSkyCache) {
            // A field's sky depends on the fields above it and on its down-right neighbour's column.
            std::vector<worldmap::TWorldMapExtent> column;
            for (int z = 1; z <= 7; ++z)
                column.emplace_back(0, 0, z);
            for (int z = 0; z <= 7; ++z)
                column.emplace_back(-1, -1, z);
            m_openSkyCache = std::make_shared<TCacheForWorldmapStorageLookupMembers>(
                storage, [storage](const TWorldMapCoordinate& c) { return storage->isUnderOpenSkyAt(c); }, column);
        }
    } else {
        m_openSkyCache.reset();
    }

    const auto fieldOf = [this](const int x, const int y, const int z) {
        return TWorldMapCoordinate(light::lightField(m_camera, centerX(), centerY(), x, y, z));
    };
    const int width = drawWidth();
    const int height = drawHeight();
    for (int z = m_lastFloor; z >= m_firstFloor; --z) {
        if (z == m_camera.z)
            map->applyFloorSeparator();
        if (z >= 0) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const TWorldMapCoordinate field = fieldOf(x, y, z);
                    if (!m_bankCache->lookup(field))
                        continue;
                    const bool openSky = m_openSkyCache && m_openSkyCache->lookup(field);
                    map->applyLevelColor(field, openSky);
                    if (!m_translucentCache->lookup(field))
                        continue;
                    // A translucent field lets the floors below shine through.
                    const QColor color = map->colorAt(field);
                    map->setColorAt(field,
                                    QColor::fromRgbF(std::max(color.redF(), color.redF() * m_lightOptions.separator),
                                                     std::max(color.greenF(), color.greenF() * m_lightOptions.separator),
                                                     std::max(color.blueF(), color.blueF() * m_lightOptions.separator), 1.f));
                }
            }
        }
        if (z == sea + 1) {
            // The first floor underground takes the daylight under a translucent field above it.
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const TWorldMapCoordinate field = fieldOf(x, y, z);
                    const TWorldMapCoordinate above(field.x, field.y, sea);
                    if (!m_translucentCache->lookup(above))
                        continue;
                    const bool openSky = m_openSkyCache && m_openSkyCache->lookup(above);
                    map->applyLevelColor(field, 1, openSky);
                }
            }
        }
        const auto floorLights = lightsByFloor.find(z);
        if (floorLights == lightsByFloor.end())
            continue;
        const auto hasBank = [this](const TWorldMapCoordinate& c) { return m_bankCache->lookup(c); };
        for (const light::PointLight* source : floorLights->second) {
            // The light's position in the world: its map pixel without the floor's shear.
            const int dz = source->z - origin.z;
            const worldmap::TWorldMapSubfieldCoordinate position(origin.x * FieldSize + source->center.x() - FieldSize * dz,
                                                                 origin.y * FieldSize + source->center.y() - FieldSize * dz,
                                                                 source->z);
            const QRgb rgb = light::lightColor(source->color, 1.f);
            map->addLightSource(position, { position, source->intensity, QColor(rgb) }, hasBank);
        }
    }

    // The visible area starts a field off the stored one: its edge repeats its neighbour.
    const worldmap::TWorldMapCoordinate stored = storage->storedVolume().origin;
    const int storedX = stored.x - (m_camera.z - stored.z);
    const int storedY = stored.y - (m_camera.z - stored.z);
    if (origin.x != storedX)
        map->copySecondColumnToFirst();
    if (origin.y != storedY)
        map->copySecondRowToFirst();
    return map;
}

/// CIP allocates a fresh TLightMap every frame — no idle light-image cache.
void TWorldMapRenderProvider::buildLight()
{
    // Host: an extension's frame hooks see the frame before its light map
    // (the session's world only; preview volumes have none).
    const tibia::mods::WorldFrameHooks& hooks = tibia::mods::worldFrameHooks();
    const bool hooked = (hooks.afterBuild || hooks.filterLightMap) && drawsLiveWorld();
    const QtClientWorldFrameView view = hooked ? frameView() : QtClientWorldFrameView{};
    if (hooked && hooks.afterBuild)
        hooks.afterBuild(hooks.user, &view);

    // Without light effects, or on the software graphics engine, the frame has
    // no light map.
    const bool lightEffectsEnabled = m_clientOptions
        ? m_clientOptions->boolOption(config::EClientOption::lightEffectsEnabled)
        : m_lightOptions.effectsEnabled;
    if (lightEffectsEnabled && !app::isSoftwareGraphicsEngineActive())
        m_lightMap = buildLightMap(true);
    else
        m_lightMap.reset();
    const QImage untinted = m_lightMap ? m_lightMap->toImage() : QImage();
    m_frame.light = untinted;
    if (hooked && hooks.filterLightMap && !m_frame.light.isNull()) {
        // Host: an extension recolours the frame's light map in place (its own
        // copy: `untinted` stays as built).
        QtClientImageView light{};
        light.pixels = m_frame.light.bits();
        light.width = m_frame.light.width();
        light.height = m_frame.light.height();
        light.bytes_per_line = static_cast<int>(m_frame.light.bytesPerLine());
        light.format = QTCLIENT_IMAGE_FORMAT_ARGB32_PREMULTIPLIED;
        hooks.filterLightMap(hooks.user, &view, &light);
    }
    if (m_lighting && m_lighting->enabled()) {
        // Host (pack lighting): the driver reads the floors' light even when
        // the frame has no light map.
        const QImage classic = m_lightMap ? untinted : buildLightMap(true)->toImage();
        // The floors' light without the point lights.
        m_lighting->setFieldAmbient(m_lights.empty() ? classic : buildLightMap(false)->toImage());
        // The grid the light map multiplies (after the frame hooks' filter).
        m_lighting->setFieldClassic(m_lightMap ? m_frame.light : classic);
    }
}

float TWorldMapRenderProvider::lightnessAt(const QPoint& mapPixel) const
{
    // With Light Effects off the LightMap is not drawn — keep HUD at full brightness.
    if (!m_lightOptions.effectsEnabled || !m_lightMap)
        return 1.f;
    // The map pixel's subfield on the camera's floor, where the grid is unsheared.
    const worldmap::TWorldMapCoordinate origin = m_lightMap->origin();
    const worldmap::TWorldMapSubfieldCoordinate subfield(origin.x * FieldSize + mapPixel.x(),
                                                         origin.y * FieldSize + mapPixel.y(), origin.z);
    return m_lightMap->lightnessAt(subfield) / 255.f;
}

int TWorldMapRenderProvider::fieldLightness(const Position& field) const
{
    // CIP v14: no light map → 0xff; else the field's lightness.
    if (!m_lightOptions.effectsEnabled || !m_lightMap)
        return 255;
    return m_lightMap->lightnessAt(worldmap::TWorldMapCoordinate(field));
}

void TWorldMapRenderProvider::addLight(const QPoint& center, const uint8_t intensity, const uint8_t color)
{
    if (intensity > 0)
        m_lights.push_back({ QPoint(center.x(), center.y()), intensity, color, static_cast<uint8_t>(m_floor) });
}

void TWorldMapRenderProvider::updateVisibleFloors(uint8_t& first, uint8_t& last)
{
    worldmap::TWorldMapViewport* viewport = m_viewport;
    if (!viewport && m_renderVolumeProvider)
        viewport = m_renderVolumeProvider->worldMapViewport().get();
    if (!m_renderVolumeProvider || !viewport) {
        first = last = m_camera.z;
        return;
    }
    // The view centres on the camera's field before the volume is taken from it.
    viewport->setCenter(worldmap::TWorldMapSubfieldCoordinate::fromField(worldmap::TWorldMapCoordinate(m_camera), 0, 0));
    m_renderVolumeProvider->updateRenderVolume();
    const worldmap::TShearedWorldMapCuboid volume = m_renderVolumeProvider->renderVolume();
    if (volume.getExtent().depth <= 0) {
        first = last = m_camera.z;
        return;
    }
    first = static_cast<uint8_t>(volume.getOrigin().z);
    last = static_cast<uint8_t>(volume.lastFloorOrigin().z);
}

QPoint TWorldMapRenderProvider::toScreen(const Position& pos) const
{
    const int floorShift = m_camera.z - pos.z;
    return { (centerX() + pos.x - m_camera.x - floorShift) * FieldSize, (centerY() + pos.y - m_camera.y - floorShift) * FieldSize };
}

QPoint TWorldMapRenderProvider::mapPixelOf(const Position& field) const
{
    const QPoint pixel = toScreen(field);
    return { pixel.x(), pixel.y() };
}

Position TWorldMapRenderProvider::lightField(const int x, const int y, const int z) const
{
    return light::lightField(m_camera, centerX(), centerY(), x, y, z);
}

Position TWorldMapRenderProvider::fieldAt(const QPoint& mapPixel, const int z) const
{
    if (!m_camera.isValid())
        return {};
    // TWorldMapPicker : the inverse of toScreen, field = pixel >> 5.
    const int floorShift = m_camera.z - z;
    const int x = (mapPixel.x() >> 5) - centerX() + m_camera.x + floorShift;
    const int y = (mapPixel.y() >> 5) - centerY() + m_camera.y + floorShift;
    if (x < 0 || y < 0 || z < 0 || z > worldmap::TWorldMapCentralPosition::LastFloor)
        return {};
    return { static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z) };
}

SpriteRecord::Highlight TWorldMapRenderProvider::highlightOf(const uint32_t creatureId) const
{
    if (!m_targetHighlight)
        return SpriteRecord::NoHighlight;
    if (m_attackTarget && creatureId == m_attackTarget)
        return SpriteRecord::AttackTarget;
    if (m_followTarget && creatureId == m_followTarget)
        return SpriteRecord::FollowTarget;
    return SpriteRecord::NoHighlight;
}

SpriteRecord::Highlight TWorldMapRenderProvider::itemHighlightOf(const Position& field, const int stackIndex,
                                                                const uint16_t typeId) const
{
    if (!m_mouseHighlight || !m_mouseTargetTypeId || !typeId)
        return SpriteRecord::NoHighlight;
    if (stackIndex != m_mouseTargetStackPos || typeId != m_mouseTargetTypeId || field != m_mouseTargetField)
        return SpriteRecord::NoHighlight;
    return SpriteRecord::MouseTarget;
}

void TWorldMapRenderProvider::addHighlightBaseplate(std::vector<SpriteRecord>& records)
{
    // No field under the mouse: no square. Without the mouse highlight option
    // the field is kept but not shown.
    if (!m_mouseHighlight || !m_mouseHighlightField.isValid())
        return;
    // The animated square under the mouse sorts like a field effect (kind 2,
    // draw index 0) of its field; its frame follows the clock (& 7).
    const int floor = m_mouseHighlightField.z;
    const QPoint cell = gridCell(m_mouseHighlightField);
    const DrawKey key = keyOfKind(2, cell.x(), cell.y(), 0, floor);
    auto info = std::make_shared<appearances::TWorldmapHighlightBaseplateSpriteInfo>(
        appearances::TWorldmapHighlightBaseplateSpriteInfo::frameAtMillis(tibia::shared::TClockService::frameMillis()));
    const QPoint topLeft = mapPixelOf(m_mouseHighlightField);
    records.push_back({ .key = key.packed(),
                        .anchor = topLeft + QPoint(FieldSize, FieldSize),
                        .floor = floor,
                        .sprite = { std::move(info) } });
}

namespace {

int floorDiv(const int value, const int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

/// The draw group and layer of each sprite kind (read from the client's tables).
constexpr std::array<uint8_t, 10> KindGroups{ 5, 4, 3, 2, 2, 2, 2, 2, 1, 1 };
constexpr std::array<uint8_t, 10> KindLayers{ 8, 0, 7, 6, 5, 3, 3, 2, 1, 0 };

/// The kind of a thing's sprites, which picks their draw group and layer:
/// missiles 0, effects 2 (top effects 1), top objects 3, creatures 4, bottom
/// objects 7, clip 8, bank 9, lying objects 5, other objects 6 (kinds 5 and 6
/// draw alike).
int objectLayer(const TAppearanceType& type, const bool creature)
{
    if (type.isMissile())
        return 0;
    if (type.isEffect())
        return type.isTopEffect() ? 1 : 2;
    if (type.isOnTop())
        return 3;
    if (creature)
        return 4;
    if (type.isOnBottom())
        return 7;
    if (type.isGroundBorder())
        return 8;
    if (type.isGround())
        return 9;
    return type.isLyingCorpse() ? 5 : 6;
}

uint8_t drawGroupOfKind(const int kind)
{
    return static_cast<unsigned>(kind) < KindGroups.size() ? KindGroups[static_cast<size_t>(kind)] : 2;
}

/// An object with light shines unless it hangs from hooks and can be moved
/// but is not hung (a lamp lying on the floor).
bool objectShines(const TAppearanceType& type, const appearances::TAppearanceInstance& object)
{
    if (!type.hasLight())
        return false;
    if (!type.isHangable() || type.isNotMoveable())
        return true;
    const auto* hangable = dynamic_cast<const appearances::THangableObjectAppearanceInstance*>(&object);
    return hangable && hangable->hangSide() != 0;
}

uint8_t drawLayerOfKind(const int kind)
{
    return static_cast<unsigned>(kind) < KindLayers.size() ? KindLayers[static_cast<size_t>(kind)] : 0;
}

} // namespace

TWorldMapRenderProvider::DrawKey TWorldMapRenderProvider::keyOfKind(const int kind, const int x, const int y,
                                                                    const uint8_t index, const int floor)
{
    return DrawKey{ .group = drawGroupOfKind(kind), .x = x, .y = y, .z = floor, .layer = drawLayerOfKind(kind),
                    .index = index };
}

/// CIP sort key: the floor (deepest first), group, field x+y and field x
/// (offset into 12 bits), layer, draw index (inverted in group 4), the index
/// kept by a record moved to a lying object, then the record's flag.
uint64_t TWorldMapRenderProvider::DrawKey::packed() const
{
    const uint8_t drawn = group == 4 ? static_cast<uint8_t>(~index) : index;
    const uint64_t column = static_cast<uint64_t>(x + 0x380) & 0xfff;
    const uint64_t diagonal = (static_cast<uint64_t>(y + 0x700) & 0xfff) + column;
    const uint64_t floorAndGroup = (group & 7) + (static_cast<uint64_t>(~z) & 0xf) * 8;
    const uint64_t field = column | diagonal << 12 | floorAndGroup << 25;
    return ((field << 12 | static_cast<uint64_t>(layer & 0xf) << 8 | drawn) << 6) + (static_cast<uint64_t>(own & 0xf) << 2)
        | (flag & 3);
}

/// CIP TWorldMapField draw list: the stack with everything after its leading
/// bank, clip and bottom objects reversed (commons oldest first, creatures and
/// top objects bottom of the stack first).
uint8_t TWorldMapRenderProvider::drawIndex(const tibia::worldmap::TWorldMapField& tile, const int stackIndex)
{
    const auto& appearances = tile.appearances();
    const int count = static_cast<int>(appearances.size());
    int prefix = 0;
    while (prefix < count) {
        const TAppearanceType* type = appearances[prefix] ? appearances[prefix]->thingType() : nullptr;
        if (!type || (!type->isGround() && !type->isGroundBorder() && !type->isOnBottom()))
            break;
        ++prefix;
    }
    const int index = stackIndex < prefix ? stackIndex : prefix + (count - 1 - stackIndex);
    return static_cast<uint8_t>(std::clamp(index, 0, 0xff));
}

QPoint TWorldMapRenderProvider::gridCell(const Position& field) const
{
    const QPoint pixel = toScreen(field);
    return { floorDiv(pixel.x(), FieldSize), floorDiv(pixel.y(), FieldSize) };
}

std::vector<TWorldMapRenderProvider::FieldEntry> TWorldMapRenderProvider::fieldsInVolume(const uint8_t first,
                                                                                           const uint8_t last) const
{
    // The render volume's fields, deepest floor first, every floor in diagonals
    // from its top-left: the visible window plus a two-field skirt (the draw
    // size is visible + 3). Empty fields and fields covered from above are
    // left out.
    std::vector<FieldEntry> fields;
    const int DrawWidth = drawWidth();
    const int DrawHeight = drawHeight();
    const int CenterX = centerX();
    const int CenterY = centerY();
    const int left = CenterX - (m_visibleWidth - 1) / 2;
    const int top = CenterY - (m_visibleHeight - 1) / 2;
    const int skirtLeft = left - 2;
    const int skirtTop = top - 2;
    const int skirtRight = left + m_visibleWidth + 2;
    const int skirtBottom = top + m_visibleHeight + 2;
    const int diagLimit = DrawWidth + DrawHeight - 1;
    worldmap::TWorldMapStorage* storage = worldMapStorage();
    for (int z = last; z >= first; --z) {
        for (int diagonal = 0; diagonal < diagLimit; ++diagonal) {
            const int advance = diagonal >= DrawHeight ? diagonal - DrawHeight : 0;
            for (int iy = diagonal - advance, ix = advance; iy >= 0 && ix < DrawWidth; --iy, ++ix) {
                if (ix < skirtLeft || iy < skirtTop || ix >= skirtRight || iy >= skirtBottom)
                    continue;
                Position pos = m_camera.translated(ix - CenterX, iy - CenterY);
                pos.coveredUp(m_camera.z - z);
                tibia::worldmap::TWorldMapField* tile = storage->fieldAt(pos);
                // Cover cache keys off Map cover revision — do not pass coverDirty
                // (that reset every cell every opaque stream frame).
                if (!tile || !tile->isDrawable() || tile->isCompletelyCovered(first, false))
                    continue;
                fields.push_back({ worldmap::TWorldMapCoordinate(pos), tile, ix, iy });
            }
        }
    }
    return fields;
}

/// CIP v12/v13: every lying object of a drawn field lends its slot to the
/// fields up, left and up-left of it; a later lying object in field order
/// overwrites an earlier one.
void TWorldMapRenderProvider::mapLyingObjects(const std::vector<FieldEntry>& fields)
{
    m_lyingSlots.clear();
    for (const FieldEntry& field : fields) {
        const auto& appearances = field.field->appearances();
        for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
            const TAppearanceType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
            if (!type || type->getCategory() != ThingCategoryItem || !type->isLyingCorpse())
                continue;
            const LyingSlot slot{ .field = field.coordinate, .x = field.x, .y = field.y,
                                  .index = drawIndex(*field.field, i) };
            static constexpr std::array<std::array<int, 2>, 3> Lent{ { { 0, -1 }, { -1, 0 }, { -1, -1 } } };
            for (const auto& [dx, dy] : Lent) {
                const worldmap::TWorldMapCoordinate to(field.coordinate.x + dx, field.coordinate.y + dy, field.coordinate.z);
                const auto [lent, added] = m_lyingSlots.try_emplace(to, slot);
                // Draw-list order: the object with the higher draw index comes later.
                if (added || (lent->second.field == field.coordinate && lent->second.index > slot.index))
                    continue;
                lent->second = slot;
            }
        }
    }
}

/// CIP: creatures and top objects (kinds 3 and 4) of a field with a lent
/// lying-object slot sort at that object's field and draw index.
TWorldMapRenderProvider::DrawKey TWorldMapRenderProvider::slotKey(const worldmap::TWorldMapCoordinate& field, const int x,
                                                                  const int y, const uint8_t layer,
                                                                  const uint8_t index) const
{
    DrawKey key{ .group = 2, .x = x, .y = y, .z = m_floor, .layer = layer, .index = index };
    const auto slot = m_lyingSlots.find(field);
    if (slot == m_lyingSlots.end())
        return key;
    key.x = slot->second.x;
    key.y = slot->second.y;
    key.own = index;
    key.index = slot->second.index;
    return key;
}

/// Kinds that take a lying object's slot: top objects and creatures.
bool TWorldMapRenderProvider::takesLyingSlot(const int kind)
{
    return static_cast<unsigned>(kind - 3) < 2;
}

void TWorldMapRenderProvider::addFields(const std::vector<FieldEntry>& fields, const uint8_t first, const uint8_t last,
                                        TOcclusionGrid& grid)
{
    // CIP v8: the lying objects lend their slots, then every field draws.
    mapLyingObjects(fields);
    // The pack's lighting gathers floor by floor: it starts each floor, resets
    // the shade of the floor's fields, gathers the fields and then the floor's
    // missiles.
    size_t next = 0;
    for (int z = last; z >= first; --z) {
        m_floor = z;
        const size_t floorBegin = next;
        while (next < fields.size() && fields[next].coordinate.z == z)
            ++next;
        if (m_lighting) {
            QElapsedTimer rayFloor;
            rayFloor.start();
            m_lighting->beginFloor(z);
            m_raycastNs += rayFloor.nsecsElapsed();
            if (m_gatherRaycast) {
                for (size_t i = floorBegin; i < next; ++i)
                    m_lighting->resetShade(fields[i].x, fields[i].y, fields[i].field->canShade());
            }
        }
        for (size_t i = floorBegin; i < next; ++i)
            addField(fields[i], grid);
        gatherMissileLighting(z);
    }
}

void TWorldMapRenderProvider::addField(const FieldEntry& field, TOcclusionGrid& grid)
{
    // CIP v9: every object of the field. qtclient walks them bank, clip and
    // bottom objects, the other objects, the creatures, the effects, then the
    // top objects (the order the pack's lighting stamps them in); the key sort
    // puts them in the field's draw-list order.
    const QPoint dest{ field.x * FieldSize, field.y * FieldSize };
    m_key = DrawKey{ .group = 2, .x = field.x, .y = field.y, .z = m_floor };
    if (m_gatherRaycast)
        beginLightingField(field);
    uint8_t elevation = 0;
    addTileBelow(field, dest, elevation, grid);
    addTileCreatures(field, dest, elevation, grid);
    addTileEffects(field.field, dest);
    addTileTop(field, dest, grid);
    if (m_showLootHighlighting)
        addLootHighlight(field, dest);
}

void TWorldMapRenderProvider::beginLightingField(const FieldEntry& field)
{
    // Host (pack lighting): the driver sees the field's stack before its things.
    const int count = peelFieldStack(*field.field, nullptr, 0);
    if (m_lightingStack.size() < static_cast<size_t>(count))
        m_lightingStack.resize(static_cast<size_t>(count));
    peelFieldStack(*field.field, m_lightingStack.data(), count);
    const Position& pos = field.field->getPosition();
    m_lighting->beginField({ .ix = field.x,
                             .iy = field.y,
                             .pos = { pos.x, pos.y, pos.z },
                             .stack = m_lightingStack.data(),
                             .stackCount = count });
}

void TWorldMapRenderProvider::addLootHighlight(const FieldEntry& field, const QPoint& dest)
{
    // With loot highlighting on, the first container in draw order that holds
    // a field effect draws it after the field's objects: at the container's
    // place, as the effect type's kind with draw index 0, with its light.
    for (const appearances::TAppearanceInstancePtr& object : field.field->drawList()) {
        const auto* container = dynamic_cast<const appearances::TContainerObjectAppearanceInstance*>(object.get());
        if (!container || !container->fieldEffect())
            continue;
        const appearances::TAppearanceInstancePtr effect = container->fieldEffect();
        if (const TAppearanceType* type = effect->thingType()) {
            const TAppearanceType* containerType = object->thingType();
            const QPoint displacement = containerType ? containerType->getDisplacement() : QPoint();
            m_key = keyOfKind(objectLayer(*type, false), field.x, field.y, 0, m_floor);
            uint8_t elevation = object->elevation();
            addEffectFromAppearance(effect, dest - displacement, elevation);
        }
        break;
    }
    m_key = DrawKey{ .group = 2, .x = field.x, .y = field.y, .z = m_floor };
}

void TWorldMapRenderProvider::addTileBelow(const FieldEntry& field, const QPoint& dest, uint8_t& elevation,
                                           TOcclusionGrid& grid)
{
    tibia::worldmap::TWorldMapField* tile = field.field;
    const auto& appearances = tile->appearances();

    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const TAppearanceType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (!type || (!type->isGround() && !type->isGroundBorder()))
            break;
        addObject(field, i, dest, elevation, grid);
    }

    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const TAppearanceType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (!type || type->isGround() || type->isGroundBorder())
            continue;
        if (!type->isOnBottom())
            break;
        addObject(field, i, dest, elevation, grid);
    }

    if (tile->hasCommonItem()) {
        for (int i = static_cast<int>(appearances.size()) - 1; i >= 0; --i) {
            const TAppearanceType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
            if (!type || type->isGround() || type->isGroundBorder() || type->isOnBottom() || type->isOnTop()
                || type->getCategory() == ThingCategoryCreature || appearances[i]->isCreature())
                continue;
            addObject(field, i, dest, elevation, grid);
        }
    }

    // Host: effects an effect remap hook draws under creatures sort between the
    // commons and the creatures of their field.
    uint8_t effectElevation = 0;
    const auto& effects = tile->effectAppearances();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        const auto* effect = dynamic_cast<const appearances::TEffectInstance*>(effects[i].get());
        if (!effect || !effect->underCreatures())
            continue;
        m_key.group = 2;
        m_key.layer = 4;
        m_key.index = static_cast<uint8_t>(std::min(i, 0xff));
        m_key.own = 0;
        addEffectFromAppearance(effects[i], dest, effectElevation);
    }
}

void TWorldMapRenderProvider::addFieldStackCreature(tibia::worldmap::TWorldMapField* tile, const int stackIndex,
                                                    const QPoint& dest, const uint8_t elevation, TOcclusionGrid& grid)
{
    if (!tile || stackIndex < 0)
        return;
    const uint32_t creatureId = tile->stackCreatureId(stackIndex);
    if (creatureId == 0)
        return;
    tibia::creatures::TCreatureStorage* storage = creatureStorage();
    if (!storage)
        return;
    const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId);
    if (!row || row->isDisplaced() || row->position() != tile->getPosition())
        return;
    // The field lifted the creature's placeholder.
    const appearances::TAppearanceInstancePtr placeholder = tile->appearanceAt(stackIndex);
    const uint8_t lift = placeholder ? placeholder->elevation() : elevation;
    const QPoint at(dest.x() - lift, dest.y() - lift);
    addCreature(*row, tile->coordinate(), at, lift, stackIndex, grid);
}

/// A creature sorts with the field its step covers (CIP: the placeholder's
/// covered field, its offset kept in (-32, 0]) and keeps the draw index of its
/// placeholder on the field it stands on.
void TWorldMapRenderProvider::addTileCreatures(const FieldEntry& field, const QPoint& dest, const uint8_t elevation,
                                               TOcclusionGrid& grid)
{
    tibia::worldmap::TWorldMapField* tile = field.field;
    const Position& tilePos = tile->getPosition();
    tibia::creatures::TCreatureStorage* storage = creatureStorage();
    if (!storage)
        return;
    const int x = field.x;
    const int y = field.y;
    const uint8_t creatureLayer = drawLayerOfKind(4);

    for (const tibia::worldmap::TStackCreatureEntry& entry : tile->stackCreatureEntries()) {
        m_key = slotKey(field.coordinate, x, y, creatureLayer, drawIndex(*tile, entry.stackIndex));
        addFieldStackCreature(tile, entry.stackIndex, dest, elevation, grid);
    }

    // Host: a stepping creature is drawn from the field its step covers (the
    // field keeps the ids of the creatures stepping over it); its placeholder's
    // render infos are built on the field it stands on and land on this one.
    for (const uint32_t creatureId : tile->walkingCreatureIds()) {
        const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId);
        if (!row || !row->isDisplaced())
            continue;
        const Position& pos = row->position();
        const tibia::worldmap::TWorldMapField* own = tibia::worldmap::fieldAt(pos);
        const int stackPos = own && row->placeholderAppearance() ? own->stackPositionOf(row->placeholderAppearance()) : 0xff;
        if (stackPos == 0xff)
            continue;
        const int lift = row->drawElevation();
        m_key = slotKey(field.coordinate, x, y, creatureLayer, drawIndex(*own, stackPos));
        // Drawn on the field its step covers, which does not hold it (no stack index).
        const QPoint ownCorner(dest.x() + (pos.x - tilePos.x) * FieldSize - lift,
                               dest.y() + (pos.y - tilePos.y) * FieldSize - lift);
        addCreature(*row, own->coordinate(), ownCorner, static_cast<uint8_t>(std::clamp(lift, 0, 0xff)), -1, grid);
    }
    m_key = DrawKey{ .group = 2, .x = x, .y = y, .z = m_floor };
}

/// CIP: a field effect is kind 2 (group 3): drawn after every object, creature
/// and top object of the floor, in field order. With the topeffect flag it is
/// kind 1 (group 4): after all group-3 effects, newest first within a field.
/// Effects never take a lying object's slot.
void TWorldMapRenderProvider::addTileEffects(tibia::worldmap::TWorldMapField* tile, const QPoint& dest)
{
    const int x = m_key.x;
    const int y = m_key.y;
    uint8_t elevation = 0;
    const auto& effects = tile->effectAppearances();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        const auto* effect = dynamic_cast<const appearances::TEffectInstance*>(effects[i].get());
        if (!effect || effect->underCreatures())
            continue;
        const uint8_t index = static_cast<uint8_t>(std::min(i, 0xff));
        m_key = keyOfKind(effect->isTopEffectType() ? 1 : 2, x, y, index, m_floor);
        addEffectFromAppearance(effects[i], dest, elevation);
    }
    m_key = DrawKey{ .group = 2, .x = x, .y = y, .z = m_floor };
}

void TWorldMapRenderProvider::addTileTop(const FieldEntry& field, const QPoint& dest, TOcclusionGrid& grid)
{
    tibia::worldmap::TWorldMapField* tile = field.field;
    if (!tile->hasTopItem())
        return;
    uint8_t elevation = 0;
    const auto& appearances = tile->appearances();
    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const TAppearanceType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (type && type->isOnTop())
            addObject(field, i, dest, elevation, grid);
    }
}

void TWorldMapRenderProvider::addObject(const FieldEntry& field, const int stackIndex, const QPoint& dest,
                                        uint8_t& elevation, TOcclusionGrid& grid)
{
    tibia::worldmap::TWorldMapField* tile = field.field;
    if (!tile)
        return;
    const appearances::TAppearanceInstancePtr appearance = tile->appearanceAt(stackIndex);
    if (!appearance)
        return;
    TAppearanceType* type = appearance->thingType();
    // A creature's placeholder draws with the field's creatures.
    if (!type || type->getCategory() == ThingCategoryCreature || appearance->isCreature())
        return;

    const int x = m_key.x;
    const int y = m_key.y;
    const int kind = objectLayer(*type, false);
    const uint8_t index = drawIndex(*tile, stackIndex);
    if (takesLyingSlot(kind)) {
        m_key = slotKey(field.coordinate, x, y, drawLayerOfKind(kind), index);
    } else {
        m_key = keyOfKind(kind, x, y, index, m_floor);
        // Bank and clip objects larger than a field sort with the field's
        // other objects.
        if (m_key.group == 1 && (type->getWidth() > 1 || type->getHeight() > 1))
            m_key.group = 2;
    }

    // The field gave every object but a top object the elevation of the
    // objects drawn before it.
    const uint8_t lift = type->isOnTop() ? 0 : appearance->elevation();
    const QPoint at(dest.x() - lift, dest.y() - lift);
    const SpriteRecord::Highlight highlight = itemHighlightOf(tile->getPosition(), stackIndex, appearance->id());
    addRenderInfos(*appearance, field.coordinate, at, highlight, lift, stackIndex, grid);
    m_key = DrawKey{ .group = 2, .x = x, .y = y, .z = m_floor };
    elevation = tile->objectElevation();
}

void TWorldMapRenderProvider::pushRecord(SpriteRecord record)
{
    DrawKey key = m_key;
    key.z = m_floor;
    record.key = key.packed();
    record.floor = m_floor;
    m_records.push_back(std::move(record));
}

namespace {

/// The phase `outfit` draws on its idle or moving sequence, counted over the
/// idle phases and then the moving ones (host: the pack lighting's stamp
/// phases and TOutfitAppearanceInstance::spriteInfoAtPhase count that way).
int outfitPhase(const appearances::TOutfitAppearanceInstance& outfit, const bool moving)
{
    const std::shared_ptr<appearances::TAppearanceInstanceSpriteSequence>& sequence =
        moving ? outfit.movingSequence() : outfit.idleSequence();
    const int phase = sequence && sequence->animator() ? sequence->animator()->phase() : 0;
    if (!moving || outfit.movingSequence() == outfit.idleSequence())
        return phase;
    const auto& idle = outfit.idleSequence();
    return (idle ? static_cast<int>(idle->spriteInfos().size()) : 0) + phase;
}

} // namespace

void TWorldMapRenderProvider::addRenderInfos(const appearances::TAppearanceInstance& instance,
                                             const worldmap::TWorldMapCoordinate& coordinate, const QPoint& dest,
                                             const SpriteRecord::Highlight highlight, const uint8_t lift,
                                             const int stackIndex, TOcclusionGrid& grid)
{
    TAppearanceType* type = instance.thingType();
    if (!type)
        return;
    const auto* object = dynamic_cast<const appearances::TObjectAppearanceInstance*>(&instance);
    const auto* placeholder = dynamic_cast<const appearances::TCreaturePlaceholderAppearanceInstance*>(&instance);
    const std::shared_ptr<tibia::creatures::TCreature> creature = placeholder ? placeholder->creature() : nullptr;
    if (placeholder && !creature)
        return;

    // What the instance draws: an object's or a creature's render infos, else
    // the instance's own sprite.
    renderer::TRenderInfoList infos;
    if (object) {
        object->appendRenderInfos(coordinate, infos);
    } else if (placeholder) {
        placeholder->appendRenderInfos(coordinate, infos);
    } else {
        infos.push_back({ .type = type, .spriteInfo = instance.currentSpriteInfo(), .coordinate = coordinate,
                          .displacement = instance.displacement(), .movementOffset = instance.movementOffset(),
                          .kind = renderer::TRenderInfo::Self, .isCreature = instance.isCreature() });
    }

    // Host (pack lighting): the object's own sprite stamps the phase the
    // lighting asks for, with the object's pattern; a creature stamps its
    // outfit's and mount's phase with the look it is drawn with (the caller
    // holds the creature's lighting context).
    const appearances::TAppearanceInstanceSpriteSequence* sequence = instance.spriteSequence().get();
    const int phase = sequence && sequence->animator() ? sequence->animator()->phase()
        : object                                       ? object->calculateAnimationPhase()
                                                       : 0;
    const int xPattern = object ? object->patternX() : creature ? creature->drawnPatternX() : 0;
    const auto lightingScope = m_lighting && !placeholder
        ? std::optional(m_lighting->context(itemLightingContext(*type, lift, stackIndex)))
        : std::nullopt;
    const bool moving = placeholder && placeholder->showsMovingSprite();
    const std::shared_ptr<appearances::TOutfitAppearanceInstance> outfit =
        creature ? std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(creature->outfitAppearance()) : nullptr;
    const std::shared_ptr<appearances::TOutfitAppearanceInstance> mount =
        creature && creature->isMounted() ? creature->mountAppearance() : nullptr;
    bool packLightsAdded = false;

    using EKind = appearances::IAppearanceInstanceSpriteInfo::EKind;
    for (const renderer::TRenderInfo& info : infos) {
        if (!info.spriteInfo || !info.type)
            continue;
        m_objectCounter.addObject(coordinate, info.type->getId());
        // The mouse target is tinted unless the sprite is a creature mark or
        // the baseplate; otherwise a creature shows whether it is attacked or
        // followed.
        const EKind kind = info.spriteInfo->kind();
        SpriteRecord::Highlight shown = SpriteRecord::NoHighlight;
        if (kind != EKind::CreatureMark && kind != EKind::HighlightBaseplate && highlight != SpriteRecord::NoHighlight)
            shown = highlight;
        else if (creature)
            shown = highlightOf(creature->creatureId());

        // The info's sprite on the field it lands on (a stepping creature's
        // covered field), moved by its draw offset.
        const QPoint infoDest = dest
            + QPoint((info.coordinate.x - coordinate.x) * FieldSize, (info.coordinate.y - coordinate.y) * FieldSize);
        const size_t firstRecord = m_records.size();
        if (creature) {
            const worldmap::TWorldMapSubfieldExtent offset = info.drawPosition().offset;
            const QPoint at = infoDest + QPoint(offset.width + info.displacement.depth, offset.height + info.displacement.depth);
            const appearances::TOutfitAppearanceInstance* figure = info.kind == renderer::TRenderInfo::FigureMount ? mount.get()
                : info.kind == renderer::TRenderInfo::FigureOutfit                                                  ? outfit.get()
                                                                                                                    : nullptr;
            TAppearanceType* figureType = figure ? figure->thingType() : nullptr;
            if (figureType && !figureType->isNull()) {
                // Host (pack lighting): the figure stamps the phase the lighting
                // asks for.
                const int figurePhase = outfitPhase(*figure, moving);
                const int stampPhase =
                    m_lighting && m_lighting->frameActive() ? m_lighting->stampPhase(figurePhase) : figurePhase;
                addSpriteInfo(figureType, at + figureType->getDisplacement(), info.spriteInfo,
                              figure->spriteInfoAtPhase(stampPhase), xPattern, shown);
            } else if (kind != EKind::Dummy && (m_emit & EmitRecords)) {
                pushRecord({ .anchor = at + QPoint(FieldSize, FieldSize), .highlight = shown, .sprite = { info.spriteInfo } });
            }
        } else if (sequence && info.kind == renderer::TRenderInfo::Self && info.spriteInfo == sequence->currentSpriteInfo()) {
            addSequenceSprite(instance, dest, phase, xPattern, 1.f, 0, shown);
        } else {
            addRenderInfo(info, infoDest, shown);
        }

        // A shining type lights the map.
        const QPoint lightCenter =
            infoDest + QPoint(info.movementOffset.width + FieldSize / 2, info.movementOffset.height + FieldSize / 2);
        if (objectShines(*info.type, instance))
            addLight(lightCenter, info.type->getLight().intensity, info.type->getLight().color);

        if (creature) {
            // A creature lights the map with its own light (at least a dim
            // white one when it must stay visible), its outfit's and, while
            // riding, its mount's; the drawn outfit and mount types are kept.
            const auto addCreatureLight = [&](const Light& light) {
                addLight(lightCenter, light.intensity, light.color);
                // Host: the pack's lighting takes each light once per creature.
                if (m_lighting && !packLightsAdded)
                    m_lighting->addCenterLight(lightCenter.x(), lightCenter.y(), light.intensity, light.color);
            };
            addCreatureLight(creatureLight(*creature));
            TAppearanceType* outfitType = outfit ? outfit->thingType() : nullptr;
            if (outfitType) {
                m_frameTypes.insert(outfitType);
                if (outfitType->hasLight())
                    addCreatureLight(outfitType->getLight());
            }
            TAppearanceType* mountType = mount ? mount->thingType() : nullptr;
            if (mountType) {
                m_frameTypes.insert(mountType);
                if (mountType->hasLight())
                    addCreatureLight(mountType->getLight());
            }
            if (!outfitType && !mountType)
                m_frameTypes.insert(info.type);
            packLightsAdded = true;
        } else {
            m_frameTypes.insert(info.type);
        }

        // An opaque sprite drawn at its field's corner hides the lower floors
        // under the whole fields it covers.
        const worldmap::TWorldMapSubfieldExtent offset = info.drawPosition().offset;
        if (info.type->isOpaque() && offset.width == 0 && offset.height == 0 && offset.depth == 0) {
            for (size_t i = firstRecord; i < m_records.size(); ++i)
                occlude(grid, m_records[i], m_floor);
        }
    }
}

void TWorldMapRenderProvider::addCreature(const tibia::creatures::TCreature& stored,
                                          const worldmap::TWorldMapCoordinate& coordinate, const QPoint& dest,
                                          const uint8_t lift, const int stackIndex, TOcclusionGrid& grid)
{
    // Host: creatures an extension hides, and dead ones, are not drawn.
    if (stored.isDrawSuppressed())
        return;
    if (stored.isDead() && !stored.isHidden())
        return;
    const std::shared_ptr<appearances::TCreaturePlaceholderAppearanceInstance>& placeholder = stored.placeholderAppearance();
    if (!placeholder)
        return;

    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(
              creatureLightingContext(stored, static_cast<uint8_t>(stored.drawElevation()), stackIndex)))
        : std::nullopt;

    const QPoint at = dest + stored.walkOffset();

    // Host: the creature's HUD is drawn over its head.
    if (m_floor == m_camera.z && !stored.isDead() && !stored.isHidden()) {
        const uint32_t id = stored.creatureId();
        const bool listed = std::any_of(m_frame.creatures.begin(), m_frame.creatures.end(),
                                        [id](const CreatureHud& hud) { return hud.creatureId == id; });
        if (!listed) {
            m_frame.creatures.push_back({ id, at });
        }
    }

    // Host: an extension's decorations under and over the creature.
    addCreatureDecorations(stored, at, false);
    const appearances::TAppearanceInstancePtr outfitAppearance = stored.outfitAppearance();
    if (!outfitAppearance || dynamic_cast<const appearances::TOutfitAppearanceInstance*>(outfitAppearance.get())
        || !outfitAppearance->thingType()) {
        // The creature draws what its placeholder hands the map.
        // Host: the mouse target is an object of a field (field, stack position,
        // type), never a creature's placeholder.
        addRenderInfos(*placeholder, coordinate, dest, SpriteRecord::NoHighlight, lift, stackIndex, grid);
    } else {
        // Host: an effect worn as an outfit has no outfit sequences; it cycles
        // its phases past the first and last by the clock.
        TAppearanceType* type = outfitAppearance->thingType();
        const QPoint lightCenter = at + QPoint(FieldSize / 2, FieldSize / 2);
        const auto addCreatureLight = [&](const Light& light) {
            addLight(lightCenter, light.intensity, light.color);
            if (m_lighting)
                m_lighting->addCenterLight(lightCenter.x(), lightCenter.y(), light.intensity, light.color);
        };
        addCreatureLight(creatureLight(stored));
        if (type->hasLight())
            addCreatureLight(type->getLight());
        const int phases = std::max(1, type->getAnimationPhases() - 2);
        // The invisible creature's shimmer: 75 ms per phase.
        constexpr int ticks = 75;
        int phase = 0;
        if (phases > 1)
            phase = static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * phases)) / ticks);
        phase = std::min(phase + 1, phases);
        addSprites(type, at, 0, 0, 0, phase, false, 0, highlightOf(stored.creatureId()));
        m_objectCounter.addObject(coordinate, type->getId());
        m_frameTypes.insert(type);
    }
    addCreatureDecorations(stored, at, true);
}

void TWorldMapRenderProvider::addCreatureDecorations(const tibia::creatures::TCreature& stored, const QPoint& at,
                                                     const bool overOutfit)
{
    if (!tibia::creatures::hasCreatureDecorationHook())
        return;
    m_decorations.clear();
    tibia::creatures::creatureDecorations(stored.creatureId(), overOutfit,
                                          static_cast<qint64>(tibia::shared::TClockService::frameMillis()),
                                          m_decorations);
    for (const tibia::creatures::TCreatureDecorationSprite& decoration : m_decorations) {
        const QPoint point = at + decoration.offset;
        addSprites(decoration.type, point, decoration.patternX, 0, 0, decoration.phase, false, 0,
                   SpriteRecord::NoHighlight, decoration.opacity, decoration.tint);
        if (decoration.type->hasLight()) {
            const QPoint center = point + QPoint(FieldSize / 2, FieldSize / 2);
            addLight(center, decoration.type->getLight().intensity, decoration.type->getLight().color);
            if (m_lighting)
                m_lighting->addCenterLight(center.x(), center.y(), decoration.type->getLight().intensity,
                                           decoration.type->getLight().color);
        }
    }
}

void TWorldMapRenderProvider::addEffectFromAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                                      const QPoint& dest, uint8_t& elevation)
{
    auto* effect = dynamic_cast<appearances::TEffectInstance*>(appearance.get());
    if (!effect)
        return;
    const float opacity = effect->opacity();
    if (opacity < MinEffectOpacity)
        return;
    int phase = 0, xPattern = 0, yPattern = 0;
    if (!effect->getFieldDrawFrame(phase, xPattern, yPattern))
        return;
    TAppearanceType* type = appearance->thingType();
    const uint8_t tint = effect->tint();
    const QPoint at(dest.x() - elevation, dest.y() - elevation);
    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(transientLightingContext()))
        : std::nullopt;
    addSequenceSprite(*effect, at, phase, xPattern, opacity, tint);
    if (type)
        m_frameTypes.insert(type);
    if (type && type->hasLight()) {
        const QPoint center = at + QPoint(FieldSize / 2, FieldSize / 2);
        addLight(center, type->getLight().intensity, type->getLight().color);
        if (m_lighting)
            m_lighting->addCenterLight(center.x(), center.y(), type->getLight().intensity, type->getLight().color);
    }
}

void TWorldMapRenderProvider::addMissiles(const uint8_t first, const uint8_t last)
{
    // CIP v11: the missiles of the render volume after every field, numbered in
    // the order the effect storage holds them; a missile sorts with the field
    // under its flight position.
    m_emit = EmitRecords;
    int index = 0;
    for (int z = last; z >= first; --z) {
        m_floor = z;
        for (const appearances::TAppearanceInstancePtr& missile : tibia::worldmap::floorMissiles(z)) {
            m_key = keyOfKind(0, 0, 0, static_cast<uint8_t>(index++ & 0xff), z);
            addMissile(missile);
        }
    }
    m_emit = EmitAll;
}

void TWorldMapRenderProvider::gatherMissileLighting(const int z)
{
    // The pack's lighting takes a floor's missiles right after its fields.
    if (!m_lighting)
        return;
    m_emit = EmitStamps;
    for (const appearances::TAppearanceInstancePtr& missile : tibia::worldmap::floorMissiles(z))
        addMissile(missile);
    m_emit = EmitAll;
}

void TWorldMapRenderProvider::addMissile(const appearances::TAppearanceInstancePtr& appearance)
{
    auto* missile = dynamic_cast<appearances::TMissileEffectInstance*>(appearance.get());
    if (!missile)
        return;
    const float opacity = missile->opacity();
    if (opacity < MinEffectOpacity)
        return;
    TAppearanceType* type = appearance->thingType();
    const QPoint dest = toScreen(missile->pathFrom()) + missile->flightOffset();
    m_key.x = floorDiv(dest.x(), FieldSize);
    m_key.y = floorDiv(dest.y(), FieldSize);
    const auto lightingScope = m_lighting && (m_emit & EmitStamps)
        ? std::optional(m_lighting->context(transientLightingContext()))
        : std::nullopt;
    addSequenceSprite(*missile, dest, missile->phase(), missile->patternX(), opacity, 0);
    if (type && (m_emit & EmitRecords))
        m_frameTypes.insert(type);
    // Missiles light the map from their current flight position.
    if (type && type->hasLight()) {
        const QPoint center = dest + QPoint(FieldSize / 2, FieldSize / 2);
        if (m_emit & EmitRecords)
            addLight(center, type->getLight().intensity, type->getLight().color);
        if (m_lighting && (m_emit & EmitStamps))
            m_lighting->addCenterLight(center.x(), center.y(), type->getLight().intensity, type->getLight().color);
    }
}

namespace {

/// The sprite infos `info` draws, bottom first: a layered info's layers, else itself.
std::vector<const appearances::IAppearanceInstanceSpriteInfo*> drawnLayers(const appearances::IAppearanceInstanceSpriteInfo* info)
{
    if (!info)
        return {};
    if (const auto* layered = dynamic_cast<const appearances::TLayeredAppearanceInstanceSpriteInfo*>(info)) {
        std::vector<const appearances::IAppearanceInstanceSpriteInfo*> layers;
        for (const auto& layer : layered->layers())
            layers.push_back(layer.get());
        return layers;
    }
    return { info };
}

/// The spritesheet sprite id a (masked) sprite info draws; 0 for other kinds.
uint32_t drawnSpriteId(const appearances::IAppearanceInstanceSpriteInfo* info)
{
    using EKind = appearances::IAppearanceInstanceSpriteInfo::EKind;
    if (!info)
        return 0;
    if (info->kind() == EKind::Spritesheet)
        return static_cast<const appearances::TSpritesheetAppearanceInstanceSpriteInfo*>(info)->spriteId();
    if (info->kind() == EKind::Masked) {
        const auto& sprite = static_cast<const appearances::TMaskedAppearanceInstanceSpriteInfo*>(info)->sprite();
        return sprite ? sprite->spriteId : 0;
    }
    return 0;
}

} // namespace

void TWorldMapRenderProvider::addSpriteInfo(TAppearanceType* type, const QPoint& dest,
                                            const appearances::TAppearanceInstanceSpriteInfoPtr& info,
                                            const appearances::TAppearanceInstanceSpriteInfoPtr& stamp, const int xPattern,
                                            const SpriteRecord::Highlight highlight, const float opacity,
                                            const uint8_t tint)
{
    if (!type || type->isNull() || !info)
        return;
    // A thing's frame ends at the bottom-right of its field, less its displacement.
    const QPoint anchor = dest - type->getDisplacement() + QPoint(FieldSize, FieldSize);
    if (info->kind() != appearances::IAppearanceInstanceSpriteInfo::EKind::Dummy && (m_emit & EmitRecords))
        pushRecord({ .anchor = anchor, .highlight = highlight, .sprite = { info, tint }, .opacity = opacity });
    if ((m_emit & EmitStamps) && m_lighting && m_lighting->frameActive()) {
        for (const appearances::IAppearanceInstanceSpriteInfo* layer : drawnLayers(stamp.get())) {
            if (const uint32_t stampId = drawnSpriteId(layer))
                m_lighting->addFrame(peelLightingFrame(*type, anchor, stampId, xPattern));
        }
    }
}

void TWorldMapRenderProvider::addRenderInfo(const renderer::TRenderInfo& info, const QPoint& dest,
                                            const SpriteRecord::Highlight highlight)
{
    if (!info.type)
        return;
    // The sprite's frame ends at the bottom-right of its field moved by the
    // info's draw offset. `dest` is already lifted by the field's elevation,
    // which is the info's depth.
    const worldmap::TWorldMapSubfieldExtent offset = info.drawPosition().offset;
    const QPoint at = dest + QPoint(offset.width + info.displacement.depth, offset.height + info.displacement.depth);
    addSpriteInfo(info.type, at + info.type->getDisplacement(), info.spriteInfo, info.spriteInfo, 0, highlight);
}

void TWorldMapRenderProvider::addSequenceSprite(const appearances::TAppearanceInstance& instance, const QPoint& dest,
                                                const int phase, const int xPattern, const float opacity, const uint8_t tint,
                                                const SpriteRecord::Highlight highlight)
{
    const appearances::TAppearanceInstanceSpriteSequence* sequence = instance.spriteSequence().get();
    if (!sequence)
        return;
    appearances::TAppearanceInstanceSpriteInfoPtr stamp = sequence->currentSpriteInfo();
    if (m_lighting && m_lighting->frameActive()) {
        const int stampPhase = m_lighting->stampPhase(phase);
        const auto infos = sequence->spriteInfos();
        if (stampPhase != phase && stampPhase >= 0 && static_cast<size_t>(stampPhase) < infos.size())
            stamp = infos[static_cast<size_t>(stampPhase)];
    }
    addSpriteInfo(instance.thingType(), dest, sequence->currentSpriteInfo(), stamp, xPattern, highlight, opacity, tint);
}

void TWorldMapRenderProvider::addSprites(TAppearanceType* type, const QPoint& dest, const int xPattern, const int yPattern,
                                         const int zPattern, const int animationPhase, const bool colorize, const uint32_t colors,
                                         const SpriteRecord::Highlight highlight, const float opacity, const uint8_t tint)
{
    if (!type || type->isNull() || type->getAnimationPhases() == 0)
        return;

    // A thing's frame ends at the bottom-right of its field, less its displacement.
    const QPoint anchor = dest - type->getDisplacement() + QPoint(FieldSize, FieldSize);
    const std::optional<uint32_t> sprite = type->spriteIdAt(0, xPattern, yPattern, zPattern, animationPhase);
    if (sprite) {
        appearances::TAppearanceInstanceSpriteInfoPtr info;
        const std::optional<uint32_t> mask = colorize ? type->spriteIdAt(1, xPattern, yPattern, zPattern, animationPhase)
                                                      : std::nullopt;
        if (mask) {
            info = std::make_shared<appearances::TMaskedAppearanceInstanceSpriteInfo>(
                *sprite, *mask, appearances::TOutfitColors::fromPacked(colors));
        } else {
            info = std::make_shared<appearances::TSpritesheetAppearanceInstanceSpriteInfo>(*sprite);
        }
        if (m_emit & EmitRecords)
            pushRecord({ .anchor = anchor, .highlight = highlight, .sprite = { std::move(info), tint }, .opacity = opacity });
    }
    if ((m_emit & EmitStamps) && m_lighting && m_lighting->frameActive()) {
        const int stampPhase = m_lighting->stampPhase(animationPhase);
        const std::optional<uint32_t> stamp = stampPhase == animationPhase
            ? sprite
            : type->spriteIdAt(0, xPattern, yPattern, zPattern, stampPhase);
        if (stamp)
            m_lighting->addFrame(peelLightingFrame(*type, anchor, *stamp, xPattern));
    }
}


}
