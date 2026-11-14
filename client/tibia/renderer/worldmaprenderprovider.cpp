#include "worldmaprenderprovider.h"
#include "tibia/runtime/host.h"

#include "appearances/appearanceinstance.h"
#include "appearances/spriteinfo.h"
#include "appearances/worldmaphighlightbaseplate.h"
#include "app/clientoptions.h"
#include "mods/registry.h"
#include "mods/weather_driver.h"
#include "mods/worldview.h"
#include "sprites/sprite.h"
#include "renderer/lightmodel.h"
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldgeometry.h"
#include "worldmap/lightmap.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QLoggingCategory>

#include <tibia/creatures/attachedeffect.h>
#include <tibia/creatures/movementtweens.h>
#include <tibia/config/runtimegameconfig.h>
#include <client/item.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include "creatures/creaturestorage.h"
#include "creatures/walkdebug.h"
#include "worldmap/worldmapfield.h"
#include <framework/core/clock.h>

#include <QColor>

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <vector>
#include <tibia/game/gamesession.h>
#include "worldmap/worldmapstorage.h"
#include <tibia/game/thingsession_forward.h>

namespace tibia::qmlcomponents {

Q_LOGGING_CATEGORY(lcWorldMapBuild, "tibia.worldmap.build", QtWarningMsg)
Q_LOGGING_CATEGORY(lcWeatherFrame, "tibia.weather.frame", QtWarningMsg)
Q_LOGGING_CATEGORY(lcWeatherPerf, "tibia.weather.perf", QtWarningMsg)

namespace {

constexpr int FieldSize = TWorldMapRenderProvider::FieldSize;

uint32_t outfitColors(const Outfit& outfit)
{
    return outfit.getHead() | outfit.getBody() << 8 | outfit.getLegs() << 16 | static_cast<uint32_t>(outfit.getFeet()) << 24;
}

/// Thing subclasses hide getThingType; Thing exposes it.
ThingType* typeOf(const Thing& thing)
{
    return thing.getThingType();
}

tibia::mods::LightingMapPos lightingCamera(const Position& camera)
{
    return { camera.x, camera.y, camera.z };
}

tibia::game::MapPos geometryCamera(const Position& camera)
{
    return { camera.x, camera.y, camera.z };
}

tibia::mods::LightingThingContext itemLightingContext(ThingType& type, const uint8_t lift,
                                                      tibia::worldmap::TWorldMapField* tile)
{
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.loose = !type.isNotMoveable();
    ctx.caster = true;
    // Thing::isCommon || isOnTop: walkable commons/onTop on a wall field hang (no cast).
    const bool common = !type.isGround() && !type.isGroundBorder() && !type.isOnTop() && !type.isOnBottom();
    if ((common || type.isOnTop()) && !type.isNotWalkable() && tile && tile->hasBlockingBottomItem())
        ctx.caster = false;
    return ctx;
}

tibia::mods::LightingThingContext creatureLightingContext(const tibia::creatures::TCreature& stored,
                                                          const uint8_t lift)
{
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.creature = true;
    ctx.walking = stored.isWalking();
    ctx.localPlayer = stored.isLocalPlayer();
    ctx.caster = true;
    ctx.creatureId = stored.creatureId();
    return ctx;
}

tibia::mods::LightingThingContext transientLightingContext(const uint8_t lift = 0)
{
    // Effects / missiles: never stamp casters; lights must not take soft-occlusion slots.
    tibia::mods::LightingThingContext ctx;
    ctx.lift = lift;
    ctx.transient = true;
    ctx.caster = false;
    return ctx;
}

tibia::mods::LightingFrameDesc peelLightingFrame(ThingType& type, const Point& anchor, const uint32_t spriteId,
                                                 const int xPattern)
{
    tibia::mods::LightingFrameDesc desc;
    desc.spriteId = spriteId;
    desc.anchorX = anchor.x;
    desc.anchorY = anchor.y;
    desc.displacementX = type.getDisplacement().x;
    desc.displacementY = type.getDisplacement().y;
    desc.shape = static_cast<uint8_t>(type.getThingShape());
    desc.elevation = static_cast<uint8_t>(std::clamp(type.getElevation(), 0, 255));
    desc.hook = static_cast<int8_t>(type.isHangable() ? (type.getNumPatternX() >= 2 ? xPattern : 0) : -1);
    desc.onTop = type.isOnTop();
    desc.castsShadow = type.castsShadow();
    desc.lyingCorpse = type.isLyingCorpse();
    desc.hasLight = type.hasLight();
    if (desc.hasLight) {
        desc.lightIntensity = type.getLight().intensity;
        desc.lightColor = type.getLight().color;
    }
    return desc;
}

}

TWorldMapRenderProvider::TWorldMapRenderProvider(QObject* parent)
    : QObject(parent)
{
    loadLightOptions();
    loadMarkOptions();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name.startsWith(QLatin1String("light")))
            loadLightOptions();
        else if (name.startsWith(QLatin1String("combatShow")) || name == QLatin1String("gameWindowShowTragetFrame"))
            loadMarkOptions();
    });
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
    m_mouseTargetTypeId = typeId;
    m_mouseTargetField = typeId ? field : Position{};
    m_mouseTargetStackPos = typeId ? stackPos : -1;
    m_mouseTargetCreatureId = 0;
}

void TWorldMapRenderProvider::setMouseTarget(const ThingPtr& thing)
{
    if (thing && thing->isItem()) {
        setMouseTargetObject(static_cast<uint16_t>(thing->getId()), thing->getPosition(), thing->getStackPos());
        return;
    }
    setMouseTargetObject(0, {}, -1);
}

void TWorldMapRenderProvider::setMouseTargetCreatureId(const uint32_t id)
{
    m_mouseTargetCreatureId = id;
    m_mouseTargetTypeId = 0;
    m_mouseTargetField = {};
    m_mouseTargetStackPos = -1;
}

void TWorldMapRenderProvider::loadMarkOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    m_markOptions.target = options.value(QLatin1String("gameWindowShowTragetFrame")).toBool(true);
    m_markOptions.pvp = options.value(QLatin1String("combatShowPvpFrames")).toBool(true);
    m_markOptions.timed = options.value(QLatin1String("combatShowFrames")).toBool(true);
}

/// The target frame is red for the attack target and green for the follow
/// target, lighter under the mouse, white for any other creature under the
/// mouse (42270/42420, colors from). Then the
/// server's permanent mark (vt 0x230) and timed mark (vt 0x240).
std::array<uint32_t, 3> TWorldMapRenderProvider::creatureMarks(const tibia::creatures::TCreature& creature) const
{
    std::array<uint32_t, 3> marks{};
    size_t count = 0;
    if (m_markOptions.target) {
        const uint32_t id = creature.creatureId();
        // Creature hover is id-only (setMouseTargetCreatureId); items use setMouseTargetObject.
        const bool hovered = m_mouseTargetCreatureId == id;
        uint32_t target = 0;
        if (id == m_attackTarget)
            target = hovered ? 0xFFFF8080 : 0xFFFF0000;
        else if (id == m_followTarget)
            target = hovered ? 0xFF80FF80 : 0xFF00FF00;
        else if (hovered && !m_mouseHighlight)
            target = 0xFFFFFFFF;
        if (target)
            marks[count++] = target;
    }
    const auto argb = [](const Color& color) { return 0xFF000000u | color.r() << 16 | color.g() << 8 | color.b(); };
    if (m_markOptions.pvp && creature.showStaticSquare())
        marks[count++] = argb(creature.staticSquareColor());
    if (m_markOptions.timed && creature.showTimedSquare())
        marks[count++] = argb(creature.timedSquareColor());
    return marks;
}

void TWorldMapRenderProvider::loadLightOptions()
{
    m_lightOptions = light::loadOptions();
}

void TWorldMapRenderProvider::advance()
{
    m_lighting = tibia::mods::worldLightingDriver();

    QElapsedTimer cost;
    cost.start();
    m_frame.valid = build();
    m_buildNs = cost.nsecsElapsed();
    if (m_frame.valid)
        updateHighlightBaseplate();
    if (!m_frame.valid) {
        m_frame.sprites.clear();
        m_frame.creatures.clear();
        if (tibia::creatures::lcWalk().isDebugEnabled()) {
            static QElapsedTimer s_invalidFrameLog;
            if (tibia::creatures::walkDebugShouldLog(s_invalidFrameLog, 500)) {
                qCDebug(tibia::creatures::lcWalk)
                    << "frame-invalid" << "buildUs" << (m_buildNs / 1000)
                    << "camera" << tibia::creatures::walkPos(m_camera)
                    << "floors" << int(m_firstFloor) << ".." << int(m_lastFloor);
            }
        }
    }
    if (m_tilesNs > 5'000'000) {
        // Always visible under QTCLIENT_PERF (category default is Warning).
        // tilesUs excludes gather; sum tiles+gather ≈ tile-walk wall time.
        qCWarning(lcWorldMapBuild) << "slow tiles us" << m_tilesNs / 1000
                                   << "floors" << int(m_firstFloor) << ".." << int(m_lastFloor)
                                   << "sprites" << m_frame.sprites.size()
                                   << "gather us" << m_raycastGatherNs / 1000 << "light us" << m_lightNs / 1000
                                   << "roofs us" << m_raycastRoofsNs / 1000 << "raycast us" << m_raycastNs / 1000
                                   << "cameraMoved" << m_cameraTileMoved << "coverDirty" << m_coverDirty;
    } else if (lcWorldMapBuild().isDebugEnabled()) {
        static QElapsedTimer logged;
        if (!logged.isValid() || logged.elapsed() > 1000) {
            logged.start();
            qCDebug(lcWorldMapBuild) << "build us" << m_buildNs / 1000 << "tiles us" << m_tilesNs / 1000
                                     << "gather us" << m_raycastGatherNs / 1000 << "light us" << m_lightNs / 1000
                                     << "roofs us" << m_raycastRoofsNs / 1000 << "raycast us" << m_raycastNs / 1000
                                     << "sprites" << m_frame.sprites.size()
                                     << "cameraMoved" << m_cameraTileMoved << "coverDirty" << m_coverDirty;
        }
    }
    // CIP rebuilds the sprite list every frame; QSG reconciles in place.
    if (m_frame.valid) {
        Q_EMIT frameAdvanced();
        Q_EMIT overlaysAdvanced();
    }
}

bool TWorldMapRenderProvider::build()
{
    m_lighting = tibia::mods::worldLightingDriver();

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
    if (!tibia::game::localPlayer() || !tibia::game::isOnline() || !tibia::game::localPlayerPosition().isValid()) {
        frame.sprites.clear();
        frame.creatures.clear();
        m_lights.clear();
        return false;
    }

    // Cover caches invalidate per opaque cell (Map::invalidateCoverCachesFrom).
    const uint32_t coverRevision = tibia::game::coverRevision();
    const Position camera = tibia::game::localPlayerPosition();
    const bool cameraTileMoved = m_camera != camera;
    const bool coverDirty = coverRevision != m_coverRevision;
    m_cameraTileMoved = cameraTileMoved;
    m_coverDirty = coverDirty;
    m_coverRevision = coverRevision;
    m_camera = camera;
    const Point walk = tibia::creatures::walkOffsetFor(tibia::game::localPlayerId());
    frame.camera = QPoint((CenterX - (m_visibleWidth - 1) / 2) * FieldSize + walk.x,
                          (CenterY - (m_visibleHeight - 1) / 2) * FieldSize + walk.y);

    // firstVisibleFloor on camera step or opaque cover change .
    // No hysteresis — delayed hide left upper floors stuck on.
    uint8_t first;
    uint8_t last;
    if (!m_floorsCached || cameraTileMoved || coverDirty) {
        first = firstVisibleFloor();
        last = std::max(first, lastVisibleFloor());
        m_cachedFirstFloor = first;
        m_cachedLastFloor = last;
        m_floorsCached = true;
    } else {
        first = m_cachedFirstFloor;
        last = m_cachedLastFloor;
    }
    m_firstFloor = first;
    m_lastFloor = last;

    frame.highlightStrength = static_cast<float>((std::sin(g_clock.millis() / 100.0) * 0.15 + 0.65) * 0.75);
    m_attackTarget = tibia::game::attackingCreatureId();
    m_followTarget = tibia::game::getFollowingCreatureId();

    QElapsedTimer phase;
    phase.start();
    // CIP every-frame full emit (classic and raycast). Drain map dirty for cover-flood
    // coalescing; positions are unused because the whole FOV is remitted.
    tibia::game::clearStaticDirtyTiles();

    frame.sprites.clear();
    frame.creatures.clear();
    frame.sprites.reserve(std::max(previousSprites, static_cast<size_t>(DrawWidth * DrawHeight)));
    if (previousCreatures)
        frame.creatures.reserve(previousCreatures);
    m_lights.clear();

    const bool gatherRaycast = (m_lighting && m_lighting->enabled())
        && m_lighting->beginFrame(lightingCamera(m_camera), DrawWidth, DrawHeight, first, last);

    // Share opaque/top-ground probes across FOV cover walks (cold-cache cliffs).
    tibia::game::beginCoverProbeBatch();
    for (int z = last; z >= first; --z) {
        m_floor = z;
        {
            QElapsedTimer rayFloor;
            rayFloor.start();
            if (m_lighting) m_lighting->beginFloor(z);
            m_raycastNs += rayFloor.nsecsElapsed();
        }
        m_floorSprites.clear();
        m_floorKeys.clear();
        m_visibleFields.clear();

        // CIP volume masks keep the visible window plus a two-field skirt
        // (draw size is visible + 3). Hoisted out of the per-cell loop.
        const int left = CenterX - (m_visibleWidth - 1) / 2;
        const int top = CenterY - (m_visibleHeight - 1) / 2;
        const int skirtLeft = left - 2;
        const int skirtTop = top - 2;
        const int skirtRight = left + m_visibleWidth + 2;
        const int skirtBottom = top + m_visibleHeight + 2;
        const int diagLimit = DrawWidth + DrawHeight - 1;

        for (int diagonal = 0; diagonal < diagLimit; ++diagonal) {
            const int advance = diagonal >= DrawHeight ? diagonal - DrawHeight : 0;
            for (int iy = diagonal - advance, ix = advance; iy >= 0 && ix < DrawWidth; --iy, ++ix) {
                if (ix < skirtLeft || iy < skirtTop || ix >= skirtRight || iy >= skirtBottom)
                    continue;
                Position pos = m_camera.translated(ix - CenterX, iy - CenterY);
                pos.coveredUp(m_camera.z - z);
                tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(pos);
                // Cover cache keys off Map cover revision — do not pass coverDirty
                // (that reset every cell every opaque stream frame).
                if (!tile || !tile->isDrawable() || tile->isCompletelyCovered(first, false))
                    continue;
                // Shade grid only for tiles that will draw (skip covered / empty).
                if (gatherRaycast)
                    if (m_lighting) m_lighting->resetShade(ix, iy, tile->canShade());
                m_visibleFields.push_back({ tile, ix, iy });
            }
        }

        // CIP: every sprite of the floor gets a sort key; the floor is drawn in
        // key order, so emission order only breaks ties within one appearance.
        mapLyingObjects();
        for (const VisibleField& field : m_visibleFields)
            addTile(field.tile, field.x, field.y);

        int missileIndex = 0;
        for (const appearances::TAppearanceInstancePtr& missile : tibia::worldmap::floorMissiles(z)) {
            m_key = DrawKey{ .group = 5, .layer = 8, .index = static_cast<uint8_t>(std::min(missileIndex++, 0xff)) };
            addMissile(missile);
        }

        // CIP v21: the animated square under the mouse is layer 2 like a field
        // effect (group 3); its frame is refreshed by updateHighlightBaseplate.
        if (m_mouseHighlight && m_mouseHighlightField.isValid() && m_mouseHighlightField.z == z) {
            const QPoint cell = gridCell(m_mouseHighlightField);
            m_key = DrawKey{ .group = 3, .x = cell.x(), .y = cell.y(), .layer = 7 };
            const appearances::TWorldmapHighlightBaseplateSpriteInfo info(
                appearances::TWorldmapHighlightBaseplateSpriteInfo::frameAtMillis(g_clock.millis()));
            const QPoint topLeft = mapPixelOf(m_mouseHighlightField);
            m_floorSprites.push_back({ .anchor = topLeft + QPoint(FieldSize, FieldSize), .sprite = info.key() });
            m_floorKeys.push_back(m_key.packed());
        }

        m_floorOrder.resize(m_floorSprites.size());
        std::iota(m_floorOrder.begin(), m_floorOrder.end(), 0u);
        std::stable_sort(m_floorOrder.begin(), m_floorOrder.end(),
                         [this](const uint32_t a, const uint32_t b) { return m_floorKeys[a] < m_floorKeys[b]; });
        frame.sprites.reserve(frame.sprites.size() + m_floorOrder.size());
        for (const uint32_t i : m_floorOrder)
            frame.sprites.push_back(std::move(m_floorSprites[i]));
    }
    tibia::game::endCoverProbeBatch();
    const qint64 walkNs = phase.nsecsElapsed();
    m_raycastGatherNs = m_lighting ? m_lighting->gatherNs() : 0;
    m_tilesNs = std::max<qint64>(0, walkNs - m_raycastGatherNs);

    phase.restart();
    buildLight();
    m_lightNs = phase.nsecsElapsed();

    phase.restart();
    if (m_lighting) m_lighting->endFrame(m_frame.geometry.valid() ? &m_frame.geometry : nullptr);
    m_raycastRoofsNs = m_lighting ? m_lighting->roofsNs() : 0;
    m_raycastNs += phase.nsecsElapsed();
    return true;
}

WeatherFrame TWorldMapRenderProvider::weatherFrame() const
{
    QElapsedTimer cost;
    cost.start();
    WeatherFrame out;
    auto* weather = tibia::mods::weatherDriver();
    if (!weather || !weather->enabled() || !m_frame.valid)
        return out;
    const auto state = weather->display();
    if (!state.hasAtmosphere && weather->lightningFlash() < 0.001f)
        return out;
    if (!m_frame.geometry.valid())
        return out;
    if (m_camera.z > tibia::config::mapSeaFloor())
        return out;
    out.active = true;
    out.kind = state.kind;
    out.intensity = state.intensity;
    out.fog = state.fog;
    out.clouds = state.clouds;
    out.windX = state.windX;
    out.windY = state.windY;
    out.time = static_cast<float>(g_clock.millis()) * 0.001f;
    out.lightning = weather->lightningFlash();
    // Midday isometric SE fall (map px). Kept in stock so weather overlays work
    // without linking a raycast pack.
    out.fallX = 0.70710678f;
    out.fallY = 0.70710678f;
    out.outdoorFraction = m_frame.geometry.outdoorFraction();
    out.outdoorMask = m_frame.geometry.outdoorMask;
    out.ceilingMap = m_frame.geometry.ceilingMap;
    out.shapeMap = m_frame.geometry.shapeMap;
    out.mapSize = QSize(drawWidth() * FieldSize, drawHeight() * FieldSize);
    const Position originField = m_camera.translated(-centerX(), -centerY());
    out.worldOrigin = QPointF(double(originField.x) * FieldSize, double(originField.y) * FieldSize);
    const qint64 frameUs = cost.nsecsElapsed();

    // Throttled geometry + cost feedback (~1 Hz).
    if (lcWeatherFrame().isDebugEnabled() || lcWeatherPerf().isDebugEnabled()) {
        static qint64 s_lastLogMs = 0;
        static qint64 s_frameUsSum = 0;
        static int s_frameSamples = 0;
        s_frameUsSum += frameUs;
        ++s_frameSamples;
        const qint64 now = g_clock.millis();
        if (now - s_lastLogMs >= 1000) {
            s_lastLogMs = now;
            const qint64 avgFrameUs = s_frameSamples > 0 ? s_frameUsSum / s_frameSamples : 0;
            s_frameUsSum = 0;
            s_frameSamples = 0;
            if (lcWeatherPerf().isDebugEnabled()) {
                qCDebug(lcWeatherPerf).nospace()
                    << "weatherFrame avg us=" << avgFrameUs / 1000
                    << " mask=" << out.outdoorMask.width() << "x" << out.outdoorMask.height()
                    << " mapPx=" << out.mapSize.width() << "x" << out.mapSize.height()
                    << " precip=" << out.kind << "/" << out.intensity
                    << " fog=" << out.fog << " clouds=" << out.clouds;
            }
            if (lcWeatherFrame().isDebugEnabled()) {
                int outdoor = 0;
                int impact = 0;
                int maxH = 0;
                qint64 sumH = 0;
                // ThingShape histogram on impact cells (0=none … 6=Ridge).
                int shapeCount[7] = {};
                int shapeImpact = 0;
                const QImage& mask = out.outdoorMask;
                const QImage& ceil = out.ceilingMap;
                const QImage& shapes = out.shapeMap;
                const bool haveShapes = !shapes.isNull() && shapes.size() == mask.size();
                if (!mask.isNull() && !ceil.isNull() && mask.size() == ceil.size()) {
                    for (int y = 0; y < mask.height(); ++y) {
                        const uchar* o = mask.constScanLine(y);
                        const uchar* c = ceil.constScanLine(y);
                        const uchar* s = haveShapes ? shapes.constScanLine(y) : nullptr;
                        for (int x = 0; x < mask.width(); ++x) {
                            if (o[x] < 128)
                                continue;
                            ++outdoor;
                            const int h = int(c[x]);
                            if (h > 3) {
                                ++impact;
                                sumH += h;
                                maxH = std::max(maxH, h);
                                if (s) {
                                    const int sid = int(s[x]);
                                    if (sid >= 0 && sid <= 6) {
                                        ++shapeCount[sid];
                                        ++shapeImpact;
                                    }
                                }
                            }
                        }
                    }
                }
                const float avgH = impact > 0 ? float(sumH) / float(impact) : 0.f;
                qCDebug(lcWeatherFrame).nospace()
                    << "frame precip=" << out.kind << "/" << out.intensity
                    << " fog=" << out.fog << " clouds=" << out.clouds
                    << " wind=(" << out.windX << "," << out.windY << ")"
                    << " fall=(" << out.fallX << "," << out.fallY << ")"
                    << " outdoorFrac=" << out.outdoorFraction
                    << " outdoorCells=" << outdoor
                    << " impactCells=" << impact << " (h>3, splash/collide)"
                    << " maxH=" << maxH << " avgH=" << avgH
                    << " mapPx=" << out.mapSize.width() << "x" << out.mapSize.height()
                    << " origin=(" << out.worldOrigin.x() << "," << out.worldOrigin.y() << ")"
                    << " lightning=" << out.lightning
                    << " t=" << out.time;
                qCDebug(lcWeatherFrame).nospace()
                    << "shapes impact=" << shapeImpact
                    << " map=" << (haveShapes ? "yes" : "NO")
                    << " none=" << shapeCount[0]
                    << " flat=" << shapeCount[1]
                    << " wall=" << shapeCount[2]
                    << " box=" << shapeCount[3]
                    << " round=" << shapeCount[4]
                    << " window=" << shapeCount[5]
                    << " ridge=" << shapeCount[6]
                    << " (splash: wall=drip box/ridge=ring round=ripple+beads flat/none=puddle)";
            }
        }
    }
    return out;
}

void TWorldMapRenderProvider::updateWorldGeometry()
{
    bool needGeo = (m_lighting && m_lighting->enabled())
        || tibia::mods::worldFrameHooks().requestOutdoorGather;
    if (auto* weather = tibia::mods::weatherDriver()) {
        weather->tick();
        const auto geoState = weather->display();
        needGeo = needGeo
            || (weather->enabled() && (geoState.hasAtmosphere || weather->lightningFlash() > 0.001f));
    }
    if (!needGeo) {
        m_frame.geometry = {};
    } else {
        const uint32_t revision = tibia::game::mapItemRevision();
        const int sea = tibia::config::mapSeaFloor();
        const int origin = m_camera.z > sea ? m_camera.z : static_cast<int>(m_lastFloor);
        const bool geoFresh = m_frame.geometry.valid() && m_geometryCamera == m_camera
            && m_geometryRevision == revision && m_geometryWidth == drawWidth()
            && m_geometryHeight == drawHeight() && m_geometryFirst == m_firstFloor
            && m_geometryLast == m_lastFloor && m_geometryOrigin == origin;
        if (!geoFresh) {
            QElapsedTimer geoCost;
            geoCost.start();
            m_frame.geometry = buildWorldGeometry({
                .camera = geometryCamera(m_camera),
                .drawWidth = drawWidth(),
                .drawHeight = drawHeight(),
                .centerX = centerX(),
                .centerY = centerY(),
                .firstFloor = m_firstFloor,
                .lastFloor = m_lastFloor,
                .heightOriginZ = origin,
            });
            const qint64 geoNs = geoCost.nsecsElapsed();
            m_geometryCamera = m_camera;
            m_geometryRevision = revision;
            m_geometryWidth = drawWidth();
            m_geometryHeight = drawHeight();
            m_geometryFirst = m_firstFloor;
            m_geometryLast = m_lastFloor;
            m_geometryOrigin = origin;
            if (geoNs > 3'000'000) {
                qCWarning(lcWeatherPerf) << "slow worldGeometry us" << geoNs / 1000
                                         << "size" << m_frame.geometry.width << "x" << m_frame.geometry.height
                                         << "floors" << m_firstFloor << ".." << m_lastFloor;
            } else if (lcWeatherPerf().isDebugEnabled()) {
                static QElapsedTimer s_geoLog;
                static qint64 s_geoSum = 0;
                static int s_geoCount = 0;
                s_geoSum += geoNs;
                ++s_geoCount;
                if (!s_geoLog.isValid() || s_geoLog.elapsed() > 1000) {
                    s_geoLog.start();
                    qCDebug(lcWeatherPerf) << "worldGeometry rebuilds" << s_geoCount
                                           << "avg us" << (s_geoCount > 0 ? s_geoSum / s_geoCount / 1000 : 0)
                                           << "last us" << geoNs / 1000
                                           << "size" << m_frame.geometry.width << "x" << m_frame.geometry.height;
                    s_geoSum = 0;
                    s_geoCount = 0;
                }
            }
        }
    }

    if (auto* weather = tibia::mods::weatherDriver()) {
        const auto state = weather->display();
        const bool rainingOnPlayer =
            !m_frame.geometry.valid() || m_frame.geometry.outdoorAt(centerX(), centerY());
        const bool aboveSea = m_camera.z <= tibia::config::mapSeaFloor();
        weather->updateSound(state, weather->lightningFlash(), weather->enabled(), rainingOnPlayer, aboveSea);
    }
}

void TWorldMapRenderProvider::applyWeatherAmbientTint()
{
    if (m_lightUntinted.isNull()) {
        if (m_frame.light.isNull())
            return;
        m_lightUntinted = m_frame.light;
    }
    m_frame.light = m_lightUntinted;
    auto* weather = tibia::mods::weatherDriver();
    if (!weather || !weather->enabled() || !m_frame.geometry.valid())
        return;
    const auto state = weather->display();
    // Pack sets ambientTint (mood policy). Tip only applies a neutral cool curve outdoors.
    const float amount = std::clamp(state.ambientTint, 0.f, 1.f);
    if (amount < 0.01f)
        return;
    const float coolR = 1.f - amount * 0.35f;
    const float coolG = 1.f - amount * 0.15f;
    const float coolB = 1.f + amount * 0.08f;
    for (int y = 0; y < m_frame.light.height() && y < m_frame.geometry.height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(m_frame.light.scanLine(y));
        const uchar* outdoor = m_frame.geometry.outdoorMask.constScanLine(y);
        for (int x = 0; x < m_frame.light.width() && x < m_frame.geometry.width; ++x) {
            if (outdoor[x] == 0)
                continue;
            const QRgb c = line[x];
            line[x] = qPremultiply(qRgba(std::clamp(int(qRed(c) * coolR), 0, 255),
                                         std::clamp(int(qGreen(c) * coolG), 0, 255),
                                         std::clamp(int(qBlue(c) * coolB), 0, 255), qAlpha(c)));
        }
    }
}

/// TWorldMapRenderProvider::v7 filling TLightMap .
/// CIP allocates a fresh TLightMap every frame — no idle light-image cache.
void TWorldMapRenderProvider::buildLight()
{
    updateWorldGeometry();

    const light::BuildInput in{
        .camera = m_camera,
        .drawWidth = drawWidth(),
        .drawHeight = drawHeight(),
        .centerX = centerX(),
        .centerY = centerY(),
        .firstFloor = m_firstFloor,
        .lastFloor = m_lastFloor,
        .options = m_lightOptions,
    };
    // One CIP floor-walk: raycast ambient + classic TLightMap (shared fieldFlags / roofs).
    const light::ClassicLightBuild built = light::buildClassicLightPair(in, m_lights, (m_lighting && m_lighting->enabled()));
    m_lightUntinted = light::toLightImage(built.classic, in.drawWidth, in.drawHeight);
    m_frame.light = m_lightUntinted;
    applyWeatherAmbientTint();
    if (m_lighting && m_lighting->enabled()) {
        m_lighting->setFieldAmbient(light::toLightImage(built.ambient, in.drawWidth, in.drawHeight));
        // Same grid the LightMap multiplies (post weather tint) — raycast open-floor SoT.
        m_lighting->setFieldClassic(m_frame.light);
    }
}

float TWorldMapRenderProvider::lightnessAt(const QPoint& mapPixel) const
{
    // CIP dims creature name/HUD from the light map (TLightMap::v7). With Light
    // Effects off the LightMap is not drawn — keep HUD at full brightness.
    if (!m_lightOptions.effectsEnabled)
        return 1.f;
    return worldmap::TLightMap::lightnessAt(m_frame.light, mapPixel);
}

int TWorldMapRenderProvider::fieldLightness(const Position& field) const
{
    // CIP v14: no light map → 0xff; else TLightMap sample.
    if (!m_lightOptions.effectsEnabled || m_frame.light.isNull())
        return 255;
    const Point pixel = toScreen(field);
    if (pixel.x < 0 || pixel.y < 0)
        return 0;
    return worldmap::TLightMap::fieldLightness(m_frame.light, pixel.x / FieldSize, pixel.y / FieldSize);
}

void TWorldMapRenderProvider::addLight(const Point& center, const uint8_t intensity, const uint8_t color)
{
    if (intensity > 0)
        m_lights.push_back({ QPoint(center.x, center.y), intensity, color, static_cast<uint8_t>(m_floor) });
}

/// TRenderVolumeProvider::v10 : floors above are hidden from the
/// lowest one that covers the camera's field or a field next to it (the plus
/// shape), unless that field blocks the view itself. A floor covers a field
/// with a bank or bottom object drawn over it, or a bank right above it;
/// dont_hide objects never cover. CIP hides them at once, without a fade.
uint8_t TWorldMapRenderProvider::firstVisibleFloor() const
{
    return renderer::TRenderVolumeProvider::firstVisibleFloor(m_camera);
}

uint8_t TWorldMapRenderProvider::lastVisibleFloor() const
{
    return renderer::TRenderVolumeProvider::lastVisibleFloor(m_camera);
}

Point TWorldMapRenderProvider::toScreen(const Position& pos) const
{
    const int floorShift = m_camera.z - pos.z;
    return { (centerX() + pos.x - m_camera.x - floorShift) * FieldSize, (centerY() + pos.y - m_camera.y - floorShift) * FieldSize };
}

QPoint TWorldMapRenderProvider::mapPixelOf(const Position& field) const
{
    const Point pixel = toScreen(field);
    return { pixel.x, pixel.y };
}

Position TWorldMapRenderProvider::lightField(const int x, const int y, const int z) const
{
    return light::lightField(m_camera, centerX(), centerY(), x, y, z);
}

Position TWorldMapRenderProvider::firstField(const int z) const
{
    Position field = m_camera.translated(-centerX(), -centerY());
    field.coveredUp(m_camera.z - z);
    return field;
}

Position TWorldMapRenderProvider::fieldAt(const QPoint& mapPixel, const int z) const
{
    if (!m_camera.isValid())
        return {};
    // TWorldMapPicker : the inverse of toScreen, field = pixel >> 5.
    const int floorShift = m_camera.z - z;
    const int x = (mapPixel.x() >> 5) - centerX() + m_camera.x + floorShift;
    const int y = (mapPixel.y() >> 5) - centerY() + m_camera.y + floorShift;
    if (x < 0 || y < 0 || z < 0 || z > tibia::config::mapMaxZ())
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

void TWorldMapRenderProvider::updateHighlightBaseplate()
{
    // CIP v21: the square's frame follows the wall clock (& 7). build() keys it
    // into its field's slot; this refreshes the frame in place.
    const appearances::TWorldmapHighlightBaseplateSpriteInfo info(
        appearances::TWorldmapHighlightBaseplateSpriteInfo::frameAtMillis(g_clock.millis()));
    for (SpriteRecord& sprite : m_frame.sprites) {
        if (sprite.sprite.isHighlightBaseplate())
            sprite.sprite = info.key();
    }
}

namespace {

int floorDiv(const int value, const int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

/// CIP layer of an object: top, then bottom, clip and bank; common otherwise.
uint8_t objectLayer(const ThingType& type)
{
    if (type.isOnTop())
        return 6;
    if (type.isOnBottom())
        return 2;
    if (type.isGroundBorder())
        return 1;
    if (type.isGround())
        return 0;
    return 3;
}

} // namespace

/// CIP sort key: group, field x+y and field x (offset into 12 bits), layer,
/// draw index, then the index kept by a record moved to a lying object.
uint64_t TWorldMapRenderProvider::DrawKey::packed() const
{
    const uint64_t column = static_cast<uint64_t>(x + 0x380) & 0xfff;
    const uint64_t diagonal = (static_cast<uint64_t>(y + 0x700) & 0xfff) + column;
    const uint64_t field = column | diagonal << 12 | static_cast<uint64_t>(group & 7) << 25;
    return (field << 12 | static_cast<uint64_t>(layer & 0xf) << 8 | index) << 6 | static_cast<uint64_t>(own & 0xf) << 2;
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
        const ThingType* type = appearances[prefix] ? appearances[prefix]->thingType() : nullptr;
        if (!type || (!type->isGround() && !type->isGroundBorder() && !type->isOnBottom()))
            break;
        ++prefix;
    }
    const int index = stackIndex < prefix ? stackIndex : prefix + (count - 1 - stackIndex);
    return static_cast<uint8_t>(std::clamp(index, 0, 0xff));
}

QPoint TWorldMapRenderProvider::gridCell(const Position& field) const
{
    const Point pixel = toScreen(field);
    return { floorDiv(pixel.x, FieldSize), floorDiv(pixel.y, FieldSize) };
}

/// CIP v12/v13: every lying object of a visible field lends its slot to the
/// fields up, left and up-left of it; a later lying object in field order
/// overwrites an earlier one.
void TWorldMapRenderProvider::mapLyingObjects()
{
    const int width = drawWidth();
    const int height = drawHeight();
    m_lyingSlots.assign(static_cast<size_t>(width * height), LyingSlot{});
    for (const VisibleField& field : m_visibleFields) {
        const auto& appearances = field.tile->appearances();
        for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
            const ThingType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
            if (!type || type->getCategory() != ThingCategoryItem || !type->isLyingCorpse())
                continue;
            const LyingSlot slot{ .set = true, .x = field.x, .y = field.y, .index = drawIndex(*field.tile, i) };
            static constexpr std::array<std::array<int, 2>, 3> Lent{ { { 0, -1 }, { -1, 0 }, { -1, -1 } } };
            for (const auto& [dx, dy] : Lent) {
                const int x = field.x + dx;
                const int y = field.y + dy;
                if (x < 0 || y < 0 || x >= width || y >= height)
                    continue;
                LyingSlot& lent = m_lyingSlots[static_cast<size_t>(y * width + x)];
                // Draw-list order: the object with the higher draw index comes later.
                if (lent.set && lent.x == field.x && lent.y == field.y && lent.index > slot.index)
                    continue;
                lent = slot;
            }
        }
    }
}

/// CIP: creatures and top objects (layers 5 and 6) of a field with a lent
/// lying-object slot sort at that object's field and draw index.
TWorldMapRenderProvider::DrawKey TWorldMapRenderProvider::slotKey(const int x, const int y, const uint8_t layer,
                                                                  const uint8_t index) const
{
    DrawKey key{ .group = 2, .x = x, .y = y, .layer = layer, .index = index };
    const int width = drawWidth();
    if (x < 0 || y < 0 || x >= width || y >= drawHeight())
        return key;
    const LyingSlot& slot = m_lyingSlots[static_cast<size_t>(y * width + x)];
    if (!slot.set)
        return key;
    key.x = slot.x;
    key.y = slot.y;
    key.own = index;
    key.index = slot.index;
    return key;
}

void TWorldMapRenderProvider::addTile(tibia::worldmap::TWorldMapField* tile, const int x, const int y)
{
    const Point dest{ x * FieldSize, y * FieldSize };
    m_key = DrawKey{ .group = 2, .x = x, .y = y };
    uint8_t elevation = 0;
    addTileBelow(tile, dest, elevation);
    addTileCreatures(tile, dest, elevation);
    addTileEffects(tile, dest);
    addTileTop(tile, dest);
}

void TWorldMapRenderProvider::addTileBelow(tibia::worldmap::TWorldMapField* tile, const Point& dest, uint8_t& elevation)
{
    const auto& appearances = tile->appearances();

    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const ThingType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (!type || (!type->isGround() && !type->isGroundBorder()))
            break;
        addFieldStackItem(tile, i, dest, elevation);
    }

    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const ThingType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (!type || type->isGround() || type->isGroundBorder())
            continue;
        if (!type->isOnBottom())
            break;
        addFieldStackItem(tile, i, dest, elevation);
    }

    if (tile->hasCommonItem()) {
        for (int i = static_cast<int>(appearances.size()) - 1; i >= 0; --i) {
            const ThingType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
            if (!type || type->isGround() || type->isGroundBorder() || type->isOnBottom() || type->isOnTop()
                || type->getCategory() == ThingCategoryCreature)
                continue;
            addFieldStackItem(tile, i, dest, elevation);
        }
    }

    // Product effect variants drawn under creatures sort between the commons
    // and the creatures of their field.
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
                                                    const Point& dest, const uint8_t elevation)
{
    if (!tile || stackIndex < 0)
        return;
    const uint32_t creatureId = tile->stackCreatureId(stackIndex);
    if (creatureId == 0)
        return;
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return;
    const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId);
    if (!row || row->isDisplaced() || row->position() != tile->getPosition())
        return;
    const Point at(dest.x - elevation, dest.y - elevation);
    appearances::TAppearanceInstancePtr appearance = tile->appearanceAt(stackIndex);
    if (!appearance)
        appearance = row->outfitAppearance();
    addCreatureFromOutfitAppearance(appearance, *row, at);
}

/// A creature sorts with the field its step covers (CIP: the placeholder's
/// covered field, its offset kept in (-32, 0]) and keeps the draw index of its
/// placeholder on the field it stands on.
void TWorldMapRenderProvider::addTileCreatures(tibia::worldmap::TWorldMapField* tile, const Point& dest, uint8_t elevation)
{
    const Position& tilePos = tile->getPosition();
    tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
    if (!storage)
        return;
    const int x = m_key.x;
    const int y = m_key.y;

    for (const tibia::worldmap::TStackCreatureEntry& entry : tile->stackCreatureEntries()) {
        m_key = slotKey(x, y, 5, drawIndex(*tile, entry.stackIndex));
        addFieldStackCreature(tile, entry.stackIndex, dest, elevation);
    }

    for (const uint32_t creatureId : tile->walkingCreatureIds()) {
        const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(creatureId);
        if (!row || !row->isDisplaced())
            continue;
        const Position& pos = row->position();
        const tibia::worldmap::TWorldMapField* own = tibia::worldmap::fieldAt(pos);
        const int stackPos = own ? own->creatureStackPos(creatureId) : -1;
        if (stackPos < 0)
            continue;
        const int lift = row->drawElevation();
        appearances::TAppearanceInstancePtr appearance = row->outfitAppearance();
        if (!appearance)
            continue;
        m_key = slotKey(x, y, 5, drawIndex(*own, stackPos));
        addCreatureFromOutfitAppearance(
            appearance, *row,
            Point(dest.x + (pos.x - tilePos.x) * FieldSize - lift, dest.y + (pos.y - tilePos.y) * FieldSize - lift));
    }
    m_key = DrawKey{ .group = 2, .x = x, .y = y };
}

/// CIP: a field effect is layer 2, group 3: drawn after every object, creature
/// and top object of the floor, in field order. With the topeffect flag it is
/// layer 1, group 4: after all group-3 effects, newest first within a field.
/// Effects never take a lying object's slot.
void TWorldMapRenderProvider::addTileEffects(tibia::worldmap::TWorldMapField* tile, const Point& dest)
{
    uint8_t elevation = 0;
    const auto& effects = tile->effectAppearances();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        const auto* effect = dynamic_cast<const appearances::TEffectInstance*>(effects[i].get());
        if (!effect || effect->underCreatures())
            continue;
        const uint8_t index = static_cast<uint8_t>(std::min(i, 0xff));
        if (effect->isTopEffectType()) {
            m_key.group = 4;
            m_key.layer = 0;
            m_key.index = static_cast<uint8_t>(~index);
        } else {
            m_key.group = 3;
            m_key.layer = 7;
            m_key.index = index;
        }
        m_key.own = 0;
        addEffectFromAppearance(effects[i], dest, elevation);
    }
    m_key.group = 2;
}

void TWorldMapRenderProvider::addTileTop(tibia::worldmap::TWorldMapField* tile, const Point& dest)
{
    if (!tile->hasTopItem())
        return;
    uint8_t elevation = 0;
    const auto& appearances = tile->appearances();
    for (int i = 0; i < static_cast<int>(appearances.size()); ++i) {
        const ThingType* type = appearances[i] ? appearances[i]->thingType() : nullptr;
        if (type && type->isOnTop())
            addFieldStackItem(tile, i, dest, elevation);
    }
}

void TWorldMapRenderProvider::addFieldStackItem(tibia::worldmap::TWorldMapField* tile, const int stackIndex, const Point& dest,
                                                uint8_t& elevation)
{
    if (!tile)
        return;
    const appearances::TAppearanceInstancePtr appearance = tile->appearanceAt(stackIndex);
    if (!appearance)
        return;
    const ThingType* type = appearance->thingType();
    if (!type || type->getCategory() == ThingCategoryCreature)
        return;

    // Bank and clip objects are group 1 (under every object of the floor)
    // unless their sprite is larger than a field.
    const int x = m_key.x;
    const int y = m_key.y;
    const uint8_t layer = objectLayer(*type);
    const uint8_t index = drawIndex(*tile, stackIndex);
    if (layer == 6) {
        m_key = slotKey(x, y, layer, index);
    } else {
        const bool large = type->getWidth() > 1 || type->getHeight() > 1;
        m_key = DrawKey{ .group = static_cast<uint8_t>(layer <= 1 && !large ? 1 : 2), .x = x, .y = y, .layer = layer,
                         .index = index };
    }

    const Point at(dest.x - elevation, dest.y - elevation);
    const SpriteRecord::Highlight highlight = itemHighlightOf(tile->getPosition(), stackIndex, appearance->id());
    addItemFromAppearance(appearance, at, highlight, elevation, tile);
    m_key = DrawKey{ .group = 2, .x = x, .y = y };

    if (type->hasElevation())
        elevation = static_cast<uint8_t>(std::min<int>(elevation + type->getElevation(), tibia::config::tileMaxElevation()));
}

void TWorldMapRenderProvider::addItemFromAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                                      const Point& dest, const SpriteRecord::Highlight highlight)
{
    addItemFromAppearance(appearance, dest, highlight, 0, nullptr);
}

void TWorldMapRenderProvider::addItemFromAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                                      const Point& dest, const SpriteRecord::Highlight highlight,
                                                      const uint8_t lift, tibia::worldmap::TWorldMapField* tile)
{
    if (!appearance)
        return;

    ThingType* type = appearance->thingType();
    if (!type)
        return;

    const auto* object = dynamic_cast<const appearances::TObjectAppearanceInstance*>(appearance.get());
    if (object && !object->drawable())
        return;

    int xPattern = 0;
    int yPattern = 0;
    int zPattern = 0;
    if (object) {
        xPattern = object->patternX();
        yPattern = object->patternY();
        zPattern = object->patternZ();
    }
    if (const auto* hang = dynamic_cast<const appearances::THangableObjectAppearanceInstance*>(appearance.get())) {
        const int patterns = type->getNumPatternX();
        const uint8_t side = hang->hangSide();
        if (side == 1 && patterns >= 2)
            xPattern = 1;
        else if (side >= 2 && patterns >= 3)
            xPattern = 2;
    }

    const int phase = object ? object->calculateAnimationPhase() : 0;
    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(itemLightingContext(*type, lift, tile)))
        : std::nullopt;

    if (const auto* showOff = dynamic_cast<const appearances::TShowOffSocketAppearanceInstance*>(appearance.get())) {
        if (showOff->drawsSocket()) {
            addSprites(type, dest, xPattern, yPattern, zPattern, phase, false, 0, highlight);
            if (type->hasLight())
                addLight(dest + Point(FieldSize / 2), type->getLight().intensity, type->getLight().color);
        }
        if (showOff->hasFigure()) {
            for (const auto& layer : showOff->figureLayers()) {
                if (!layer.type)
                    continue;
                addSprites(layer.type, dest + Point(layer.dx, layer.dy), layer.xPattern, layer.yPattern,
                           layer.zPattern, layer.phase, layer.colorize, layer.colors, highlight);
            }
        }
        return;
    }

    addSprites(type, dest, xPattern, yPattern, zPattern, phase, false, 0, highlight);
    if (type->hasLight())
        addLight(dest + Point(FieldSize / 2), type->getLight().intensity, type->getLight().color);
}

void TWorldMapRenderProvider::addPodiumOutfit(Item& item, const Point& dest,
                                             const SpriteRecord::Highlight highlight)
{
    const auto showOff = appearances::TShowOffSocketAppearanceInstance::fromItem(item);
    if (!showOff || !showOff->hasFigure())
        return;

    for (const auto& layer : showOff->figureLayers()) {
        if (!layer.type)
            continue;
        addSprites(layer.type, dest + Point(layer.dx, layer.dy), layer.xPattern, layer.yPattern, layer.zPattern,
                   layer.phase, layer.colorize, layer.colors, highlight);
    }
}

void TWorldMapRenderProvider::addCreatureFromOutfitAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                                              const tibia::creatures::TCreature& stored, const Point& dest)
{
    if (stored.isLocalPlayer() && stored.isSoftDead())
        return;
    if (!stored.canBeSeen() || !stored.canDraw() || (stored.isDead() && !stored.isHidden()) || stored.isHided())
        return;

    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(
              creatureLightingContext(stored, static_cast<uint8_t>(stored.drawElevation()))))
        : std::nullopt;

    Point at = dest + stored.walkOffset();
    if (const PointF jump = stored.jumpOffset(); !jump.isNull())
        at -= Point(static_cast<int>(std::round(jump.x)), static_cast<int>(std::round(jump.y)));

    Light light = stored.light();
    if (stored.isLocalPlayer() && (tibia::game::worldLightIntensity() < 64 || m_camera.z > tibia::config::mapSeaFloor())) {
        if (light.intensity == 0)
            light.intensity = 2;
        else if (light.color == 0 || light.color > 215)
            light.color = 215;
    }
    addLight(at + Point(FieldSize / 2), light.intensity, light.color);
    if (m_lighting) {
        const Point center = at + Point(FieldSize / 2);
        m_lighting->addCenterLight(center.x, center.y, light.intensity, light.color);
    }

    if (m_floor == m_camera.z && !stored.isDead() && !stored.isHidden()) {
        const uint32_t id = stored.creatureId();
        const bool listed = std::any_of(m_frame.creatures.begin(), m_frame.creatures.end(),
                                        [id](const CreatureHud& hud) { return hud.creatureId == id; });
        if (!listed) {
            const Point head = tibia::game::cipFeature(tibia::client::GameNegativeOffset) ? at : at - stored.displacement();
            m_frame.creatures.push_back({ id, QPoint(head.x, head.y) });
        }
    }

    if (const std::array<uint32_t, 3> marks = creatureMarks(stored); marks[0]) {
        const appearances::TCreatureMarkAppearanceInstanceSpriteInfo markInfo(marks);
        const Point anchor = at - stored.displacement() + Point(FieldSize);
        m_floorSprites.push_back({ .anchor = QPoint(anchor.x, anchor.y), .sprite = markInfo.key() });
        m_floorKeys.push_back(m_key.packed());
    }

    appearances::TAppearanceInstancePtr outfitAppearance = appearance;
    if (!outfitAppearance)
        outfitAppearance = stored.outfitAppearance();
    if (!outfitAppearance)
        return;

    const auto* outfitInst = dynamic_cast<const appearances::TOutfitAppearanceInstance*>(outfitAppearance.get());
    const Outfit& outfit = outfitInst ? outfitInst->outfit() : stored.outfit();
    ThingType* type = outfitAppearance->thingType();
    if (!type)
        type = tibia::game::rawThingType(outfit.getId(), outfit.getCategory());
    if (!type)
        return;

    const SpriteRecord::Highlight highlight = highlightOf(stored.creatureId());
    const Point walked = dest + stored.walkOffset();
    const Point jumped = at;
    const bool mounted = outfit.hasMount();
    addAttachedEffects(stored, walked, jumped, false);

    if (outfit.isCreature()) {
        ThingType* mount = outfit.hasMount() ? tibia::game::rawThingType(outfit.getMount(), ThingCategoryCreature) : nullptr;
        if (mount) {
            at -= mount->getDisplacement();
            addSprites(mount, at, stored.drawnPatternX(), 0, 0, stored.mountAnimationPhase(), false, 0, highlight);
            at += stored.displacement();
        }
        const int phase = stored.animationPhase();
        const bool colorize = stored.drawOutfitColor() && type->getLayers() > 1;
        const uint32_t colors = colorize ? outfitColors(outfit) : 0;
        for (int addon = 0; addon < type->getNumPatternY(); ++addon) {
            if (addon > 0 && !(outfit.getAddons() & (1 << (addon - 1))))
                continue;
            addSprites(type, at, stored.drawnPatternX(), addon, stored.patternZ(), phase, colorize, colors, highlight);
        }
        addAttachedEffects(stored, walked, jumped, true);
        return;
    }

    int phases = type->getAnimationPhases();
    int ticks = tibia::config::itemTicksPerFrame();
    if (outfit.isEffect()) {
        phases = std::max(1, phases - 2);
        ticks = tibia::config::invisibleTicksPerFrame();
    }
    int phase = 0;
    if (outfit.isItem() && type->idleAnimation())
        phase = stored.idleAnimationPhase();
    else if (phases > 1)
        phase = static_cast<int>((g_clock.millis() % (static_cast<long long>(ticks) * phases)) / ticks);
    if (outfit.isEffect())
        phase = std::min(phase + 1, phases);
    addSprites(type, at - stored.displacement(), 0, 0, 0, phase, false, 0, highlight);
    addAttachedEffects(stored, walked, jumped, true);
}

void TWorldMapRenderProvider::addAttachedEffects(const tibia::creatures::TCreature& stored,
                                                 const Point& walked, const Point& jumped, const bool onTop)
{
    if (!stored.hasAttachedEffects())
        return;
    for (const AttachedEffectPtr& effect : stored.attachedEffects()) {
        int phase = 0;
        Point offset;
        float opacity = 1.f;
        if (!effect->getDrawFrame(onTop, phase, offset, opacity))
            continue;
        ThingType* type = effect->getThingType();
        const Point point = (effect->isFollowingOwner() ? jumped : walked) + offset;
        const int xPattern = static_cast<int>(effect->getDirection()) % std::max<int>(1, type->getNumPatternX());
        uint8_t tint = 0;
        if (const auto hook = tibia::mods::effectTintHook())
            tint = hook(effect->getId());
        addSprites(type, point, xPattern, 0, 0, phase, false, 0, SpriteRecord::NoHighlight, opacity, tint);
        if (const Light& light = effect->getLight(); light.intensity > 0) {
            addLight(walked + Point(FieldSize / 2), light.intensity, light.color);
            if (m_lighting) {
                const Point center = walked + Point(FieldSize / 2);
                m_lighting->addCenterLight(center.x, center.y, light.intensity, light.color);
            }
        }
        if (type->hasLight()) {
            addLight(point + Point(FieldSize / 2), type->getLight().intensity, type->getLight().color);
            if (m_lighting) {
                const Point center = point + Point(FieldSize / 2);
                m_lighting->addCenterLight(center.x, center.y, type->getLight().intensity, type->getLight().color);
            }
        }
    }
}

void TWorldMapRenderProvider::addEffectFromAppearance(const appearances::TAppearanceInstancePtr& appearance,
                                                      const Point& dest, uint8_t& elevation)
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
    ThingType* type = appearance->thingType();
    if (type && !type->getSprites().empty())
        (void)sprites::TEffectWrapperSpritesheetIdentifier(type->getSprites().front());
    uint8_t tint = 0;
    if (const auto hook = tibia::mods::effectVariantTintHook())
        tint = hook(effect->variantId());
    const Point at(dest.x - elevation, dest.y - elevation);
    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(transientLightingContext()))
        : std::nullopt;
    addSprites(type, at, xPattern, yPattern, 0, phase, false, 0, SpriteRecord::NoHighlight, opacity, tint);
    if (type && type->hasLight()) {
        const Point center = at + Point(FieldSize / 2);
        addLight(center, type->getLight().intensity, type->getLight().color);
        if (m_lighting)
            m_lighting->addCenterLight(center.x, center.y, type->getLight().intensity, type->getLight().color);
    }
}

void TWorldMapRenderProvider::addMissile(const appearances::TAppearanceInstancePtr& appearance)
{
    auto* missile = dynamic_cast<appearances::TMissileEffectInstance*>(appearance.get());
    if (!missile)
        return;
    const float opacity = missile->opacity();
    if (opacity < MinEffectOpacity)
        return;
    // CIP appearances::TMissileEffectInstance; missiles share effects::TEffectStorage.
    ThingType* type = appearance->thingType();
    const Point dest = toScreen(missile->pathFrom()) + missile->flightOffset();
    // CIP v11: a missile sorts with the field under its flight position.
    m_key.x = floorDiv(dest.x, FieldSize);
    m_key.y = floorDiv(dest.y, FieldSize);
    const auto lightingScope = m_lighting
        ? std::optional(m_lighting->context(transientLightingContext()))
        : std::nullopt;
    addSprites(type, dest, missile->patternX(), missile->patternY(), 0, missile->phase(), false, 0, SpriteRecord::NoHighlight,
               opacity);
    // TWorldMapRenderProvider::v11 : missiles share TEffectStorage with field effects
    // and light the map from their current flight position.
    if (type && type->hasLight()) {
        const Point center = dest + Point(FieldSize / 2);
        addLight(center, type->getLight().intensity, type->getLight().color);
        if (m_lighting)
            m_lighting->addCenterLight(center.x, center.y, type->getLight().intensity, type->getLight().color);
    }
}

void TWorldMapRenderProvider::addSprites(ThingType* type, const Point& dest, const int xPattern, const int yPattern,
                                         const int zPattern, const int animationPhase, const bool colorize, const uint32_t colors,
                                         const SpriteRecord::Highlight highlight, const float opacity, const uint8_t tint)
{
    if (!type || type->isNull() || type->getAnimationPhases() == 0)
        return;

    const auto& sprites = type->getSprites();
    // A thing's frame ends at the bottom-right of its field, less its displacement.
    const Point anchor = dest - type->getDisplacement() + Point(FieldSize);
    const uint64_t drawKey = m_key.packed();
    const auto push = [&](SpriteRecord&& record) {
        m_floorSprites.push_back(std::move(record));
        m_floorKeys.push_back(drawKey);
    };
    const bool protobuf = tibia::game::isUsingProtobuf();
    const bool raycast = (m_lighting && m_lighting->frameActive());

    // .spr files use sprite 0 for blank fields. SpriteKey matches CIP SpriteInfo::key.
    const auto exists = [&](const uint32_t index) { return index < sprites.size() && (protobuf || sprites[index] != 0); };
    const auto add = [&](const int w, const int h, const Point& at) {
        const uint32_t index = type->getSpriteIndex(w, h, 0, xPattern, yPattern, zPattern, animationPhase);
        if (!exists(index))
            return;
        SpriteKey key{ .spriteId = sprites[index] };
        if (colorize) {
            const uint32_t maskIndex = type->getSpriteIndex(w, h, 1, xPattern, yPattern, zPattern, animationPhase);
            if (exists(maskIndex)) {
                key.maskId = sprites[maskIndex];
                key.colors = colors;
            }
        }
        key.tint = tint;
        push({ .anchor = QPoint(at.x, at.y),
                        .sprite = key,
                        .opacity = opacity,
                        .highlight = highlight });
    };

    // Protobuf sprites are the whole frame; .spr ones are 32x32 fields from the
    // bottom-right one up.
    if (protobuf) {
        const uint32_t index = type->getSpriteIndex(-1, -1, 0, xPattern, yPattern, zPattern, animationPhase);
        if (exists(index)) {
            SpriteKey key{ .spriteId = sprites[index], .tint = tint };
            if (colorize) {
                const uint32_t maskIndex = type->getSpriteIndex(-1, -1, 1, xPattern, yPattern, zPattern, animationPhase);
                if (exists(maskIndex)) {
                    key.maskId = sprites[maskIndex];
                    key.colors = colors;
                }
            }
            push({ .anchor = QPoint(anchor.x, anchor.y),
                            .sprite = key,
                            .opacity = opacity,
                            .highlight = highlight });
        }
        if (raycast) {
            const int stampPh = (m_lighting ? m_lighting->stampPhase(animationPhase) : 0);
            const uint32_t stampIndex = stampPh == animationPhase
                ? index
                : type->getSpriteIndex(-1, -1, 0, xPattern, yPattern, zPattern, stampPh);
            if (stampIndex < sprites.size())
                if (m_lighting)
                    m_lighting->addFrame(peelLightingFrame(*type, anchor, sprites[stampIndex], xPattern));
        }
        return;
    }
    const Size size = type->getSize();
    for (int h = 0; h < size.height(); ++h) {
        for (int w = 0; w < size.width(); ++w)
            add(w, h, anchor - Point(w * FieldSize, h * FieldSize));
    }
}

}
