#include "mapsession.h"

#include <client/position.h>
#include <framework/util/point.h>
#include <tibia/game/gamenotify.h>
#include <tibia/worldmap/pathfindtypes.h>
#include <tibia/worldmap/worldmapsessionstate.h>
#include <tibia/worldmap/worldmapstorage.h>
#include "worldmap/worldmapfield.h"

namespace tibia::game {
namespace {

Position toPosition(const MapPos& p)
{
    return Position(p.x, p.y, static_cast<uint8_t>(p.z));
}

MapUseHit fromFieldHit(const tibia::worldmap::TWorldMapField::TFieldItemHit& hit)
{
    if (!hit.ok())
        return {};
    return { true, hit.typeId, hit.stackPos };
}

TWorldMapSessionState& sessionOverlay()
{
    return tibia::worldmap::TWorldMapStorage::ensureStorage()->sessionState();
}

} // namespace

bool canTravelPath(const MapPos from, const MapPos to, const int maxComplexity)
{
    return sessionOverlay().canTravelPath(toPosition(from), toPosition(to), maxComplexity);
}

void canTravelPathAsync(const MapPos from, const MapPos to, const std::function<void(bool ok)>& callback)
{
    if (!callback) {
        return;
    }
    if (from == to) {
        callback(true);
        return;
    }
    if (from.z != to.z) {
        callback(false);
        return;
    }
    sessionOverlay().findPathAsync(toPosition(from), toPosition(to),
                        [callback](const PathFindResult_ptr& ret) {
                            const bool ok = ret
                                && (ret->status == tibia::client::PathFindResultOk
                                    || ret->status == tibia::client::PathFindResultSamePosition);
                            callback(ok);
                        },
                        tibia::client::AutoWalkPathTravel);
}

bool isWalkable(const MapPos pos, const bool ignoreCreatures)
{
    if (const tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos)))
        return tile->isWalkable(ignoreCreatures);
    return false;
}

bool blocksWalk(const MapPos pos, const bool ignoreCreatures)
{
    if (const tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos)))
        return !tile->isWalkable(ignoreCreatures);
    return false;
}

std::vector<uint32_t> walkingCreatureIds(const MapPos pos)
{
    if (const tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos)))
        return tile->walkingCreatureIds();
    return {};
}

MapAwareRange awareRange()
{
    const AwareRange range = sessionOverlay().getAwareRange();
    return { range.left, range.top, range.right, range.bottom };
}

void setAwareRange(const MapAwareRange range)
{
    sessionOverlay().setAwareRange({ range.left, range.top, range.right, range.bottom });
}

void resetAwareRange()
{
    sessionOverlay().resetAwareRange();
}

MapPos centralPosition()
{
    const Position center = sessionOverlay().getCentralPosition();
    return { center.x, center.y, center.z };
}

void setCentralPosition(const MapPos pos)
{
    sessionOverlay().setCentralPosition(toPosition(pos));
}

MapWorldLight worldLight()
{
    const Light light = sessionOverlay().getLight();
    return { light.intensity, light.color };
}

void applyWorldLight(const uint8_t intensity, const uint8_t color)
{
    const Light oldLight = sessionOverlay().getLight();
    const Light newLight{ intensity, color };
    sessionOverlay().setLight(newLight);
    if (oldLight.color != color || oldLight.intensity != intensity)
        tibia::game::notify("onWorldLightChange", newLight, oldLight);
}

bool isAwareOf(const MapPos pos)
{
    return sessionOverlay().isAwareOfPosition(toPosition(pos));
}

std::vector<MapUseHit> itemHits(const MapPos pos)
{
    const tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    std::vector<MapUseHit> out;
    for (const auto& hit : tile->itemHits()) {
        if (const MapUseHit peeled = fromFieldHit(hit); peeled.ok)
            out.push_back(peeled);
    }
    return out;
}

MapUseHit topUseHit(const MapPos pos)
{
    const tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(toPosition(pos));
    if (!tile)
        return {};
    return fromFieldHit(tile->topUseHit());
}

std::vector<MapItemHit> findItemsInRange(const MapPos center, const int rangeX, const int rangeY,
                                         const std::vector<uint16_t>& typeIds)
{
    std::vector<MapItemHit> out;
    for (const TWorldMapSessionState::TItemHit& hit :
         sessionOverlay().findItemsInRange(toPosition(center), rangeX, rangeY, typeIds)) {
        out.push_back({
            { hit.pos.x, hit.pos.y, hit.pos.z },
            hit.typeId,
            hit.stackPos,
        });
    }
    return out;
}

bool isSightClear(const MapPos from, const MapPos to)
{
    return sessionOverlay().isSightClear(toPosition(from), toPosition(to));
}

uint32_t coverRevision()
{
    return sessionOverlay().getCoverRevision();
}

void clearStaticDirtyTiles()
{
    sessionOverlay().clearStaticDirtyTiles();
}

void beginCoverProbeBatch()
{
    sessionOverlay().beginCoverProbeBatch();
}

void endCoverProbeBatch()
{
    sessionOverlay().endCoverProbeBatch();
}

void setPathAvoid(const MapPos pos, const bool avoid)
{
    sessionOverlay().setPathAvoid(toPosition(pos), avoid);
}

void clearPathAvoids()
{
    sessionOverlay().clearPathAvoids();
}

void findPathAsync(const MapPos from, const MapPos to, const std::function<void(PathFindResult_ptr)>& callback,
                   const tibia::client::AutoWalkPathMode pathMode)
{
    sessionOverlay().findPathAsync(toPosition(from), toPosition(to), callback, pathMode);
}

void addMapObserver(::MapObserver* const observer)
{
    sessionOverlay().addObserver(observer);
}

void removeMapObserver(::MapObserver* const observer)
{
    sessionOverlay().removeObserver(observer);
}

void notifyCameraMove(const MapPoint offset)
{
    sessionOverlay().notificateCameraMove(Point(offset.x, offset.y));
}

void cleanMap()
{
    sessionOverlay().clean();
}

void cleanMapDynamicThings()
{
    sessionOverlay().cleanDynamicThings();
}

uint8_t firstAwareFloor()
{
    return sessionOverlay().getFirstAwareFloor();
}

uint8_t lastAwareFloor()
{
    return sessionOverlay().getLastAwareFloor();
}

uint32_t itemRevision()
{
    return sessionOverlay().getItemRevision();
}

uint32_t dynamicRevision()
{
    return sessionOverlay().getDynamicRevision();
}

bool isCompletelyCovered(const MapPos pos, bool& isLoading, const uint8_t firstFloor)
{
    return sessionOverlay().isCompletelyCovered(toPosition(pos), isLoading, firstFloor);
}

bool isCovered(const MapPos pos, bool& isLoading, const uint8_t firstFloor)
{
    return sessionOverlay().isCovered(toPosition(pos), isLoading, firstFloor);
}

void setWaypointCostFactory(WaypointCostFactory factory)
{
    sessionOverlay().setWaypointCostFactory(std::move(factory));
}

std::shared_ptr<tibia::minimap::TWaypointCosts> waypointCosts()
{
    return sessionOverlay().waypointCosts();
}

void clearWaypointCostFactory()
{
    sessionOverlay().clearWaypointCostFactory();
}

void initMap()
{
    sessionOverlay().init();
}

void terminateMap()
{
    sessionOverlay().terminate();
}

} // namespace tibia::game
