#include "minimap/minimaprendererscenegraphdrawcommandprovider.h"

#include "minimap/minimaprenderinfostorage.h"
#include "renderer/spritetextureprovider.h"
#include "renderer/worldmapcamera.h"
#include "sprites/minimaptilespritesheetidentifier.h"
#include "sprites/resourcespritesmanager.h"
#include "sprites/spriteidentifier.h"

#include <QColor>
#include <QRgb>

#include <array>
#include <stdexcept>
#include <variant>

namespace tibia::minimap {

namespace {

constexpr int GroundFloor = 7;
constexpr int FloorCount = 16;
constexpr int SubfieldsPerField = 32;
/// The fields the level separator reaches past the visible area.
constexpr int LevelSeparatorMargin = 8;
/// The opacity of markers on other floors.
constexpr float OtherFloorMarkerOpacity = 0.4f;
/// The zone colour of a shown subarea.
constexpr QRgb SubAreaOverlayColor = 0xffffe137;

/// The rectangle's pixels as a QRectF (right and bottom included).
QRectF toRectF(const QRect& rectangle)
{
    return QRectF(rectangle.left(), rectangle.top(), static_cast<double>(rectangle.right() - rectangle.left()) + 1,
                  static_cast<double>(rectangle.bottom() - rectangle.top()) + 1);
}

renderer::TScenegraphDrawCommand spriteCommand(std::unique_ptr<sprites::ISpriteIdentifier> identifier,
                                               const QRectF& rectangle, const bool smooth)
{
    renderer::TScenegraphDrawCommandDataSprite sprite;
    sprite.spriteIdentifier = std::move(identifier);
    sprite.rectangle = rectangle;
    sprite.firstFlag = false;
    sprite.secondFlag = smooth;
    sprite.factor = 1.0;
    return renderer::TScenegraphDrawCommand(std::move(sprite));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult markerResult(renderer::TScenegraphDrawCommand command)
{
    TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult result;
    result.command.emplace(std::move(command));
    result.belowFloors = false;
    return result;
}

}

std::optional<renderer::TScenegraphDrawCommand>
TCreateMapTileScenegraphDrawCommandVisitor::operator()(const worldmap::TMapTileMapAreaData& data) const
{
    return spriteCommand(std::make_unique<sprites::TAssetSpritesheetIdentifier>(data.fileName, data.scale),
                         toRectF(camera.projectedSubfieldRectangle(area)), smooth);
}

std::optional<renderer::TScenegraphDrawCommand>
TCreateMapTileScenegraphDrawCommandVisitor::operator()(const worldmap::TMinimapTileMapAreaData& data) const
{
    return spriteCommand(std::make_unique<sprites::TMinimapTileSpritesheetIdentifier>(data.scale, data.origin.toPosition(),
                                                                                      data.imageKind),
                         toRectF(camera.projectedSubfieldRectangle(area)), smooth);
}

std::optional<renderer::TScenegraphDrawCommand>
TSubAreaOverlaysMapDataVisitor::operator()(const worldmap::TSubAreaWorldMapAreaData& data) const
{
    const worldmap::TWorldMapCoordinate& origin = area.origin;
    const int x = (origin.z + origin.x) * SubfieldsPerField;
    const int y = (origin.z + origin.y) * SubfieldsPerField;
    const worldmap::TWorldMapExtent extent = area.getExtent();
    auto mask = std::make_shared<sprites::TAssetSpritesheetIdentifier>(data.fileName);
    auto overlay = std::make_unique<sprites::TEffectWrapperSpritesheetIdentifier>(
        std::move(mask), sprites::TEffectWrapperSpritesheetIdentifier::EEffect::Highlight, QColor(SubAreaOverlayColor));
    const QRect rectangle(QPoint(x, y), QPoint(x + extent.width * SubfieldsPerField - 1,
                                               y + extent.height * SubfieldsPerField - 1));
    return spriteCommand(std::move(overlay), toRectF(rectangle), true);
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TNPCMarkerWorldMapAreaData&) const
{
    return markerResult(renderer::createNPCSpriteDrawCommand(manager, position, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TMinimapMarkerWorldMapAreaData& data) const
{
    return markerResult(renderer::createMinimapMarkerSpriteDrawCommand(manager, data.icon, position, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TPassageMapAreaData& data) const
{
    return markerResult(renderer::createPassageSpriteDrawCommand(manager, position, data.selected ? 1 : 0, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TViewpointMapAreaData&) const
{
    return markerResult(renderer::createPointOfInterestSpriteDrawCommand(manager, position, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TActiveRaidMapAreaData&) const
{
    return markerResult(renderer::createRaidSpriteDrawCommand(manager, position, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::TPlayerPositionMapAreaData&) const
{
    return markerResult(renderer::createPlayerSpriteDrawCommand(manager, position, true));
}

TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult
TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::operator()(const worldmap::THouseMarkerWorldMapAreaData& data) const
{
    return markerResult(renderer::createHouseStatusSpriteDrawCommand(manager, data.listing, position, true));
}

TMinimapRendererScenegraphDrawCommandProvider::TMinimapRendererScenegraphDrawCommandProvider(
    std::shared_ptr<TMinimapRenderInfoStorage> renderInfoStorage)
    : m_renderInfoStorage(std::move(renderInfoStorage))
    , m_camera(m_renderInfoStorage ? m_renderInfoStorage->camera() : nullptr)
{
}

TMinimapRendererScenegraphDrawCommandProvider::~TMinimapRendererScenegraphDrawCommandProvider() = default;

void TMinimapRendererScenegraphDrawCommandProvider::setViewExtent(const worldmap::TWorldMapExtent& extent)
{
    if (!(m_viewExtent == extent))
        m_viewExtent = extent;
}

void TMinimapRendererScenegraphDrawCommandProvider::updateSpriteDrawCommands()
{
    if (!m_renderInfoStorage || !m_camera)
        return;
    TMinimapRenderInfoStorage& renderInfos = *m_renderInfoStorage;
    using ETileType = TMinimapRenderInfoStorage::ETileType;

    // The floors drawn: above ground the ones down to the ground floor for the
    // map images, else only the camera's.
    const int floor = m_camera->topLeftSubfieldCoordinate().z;
    int lastFloor = floor;
    if (floor <= GroundFloor) {
        const ETileType tileType = renderInfos.tileType();
        if (tileType == ETileType::Satellite || tileType == ETileType::Minimap)
            lastFloor = GroundFloor;
    }
    const QMatrix4x4 matrix = m_camera->matrix();
    const worldmap::TWorldMapCuboid visibleArea = m_camera->visibleArea();

    m_rootDrawCommand.emplace();

    QColor background(Qt::black);
    switch (renderInfos.tileType()) {
    case ETileType::Satellite:
        background = QColor(0x27, 0x4c, 0xa5);
        break;
    case ETileType::Minimap:
        background = QColor(0x33, 0x66, 0x99);
        break;
    case ETileType::LocalMinimap:
        background = QColor(Qt::black);
        break;
    case ETileType::AfterLast:
        throw std::runtime_error("Access to  EMinimapTileType::_AfterLast in "
                                 "TMinimapRendererScenegraphDrawCommandProvider::updateSpriteDrawCommands");
    }
    const QSize viewportSize = m_camera->viewportSize();
    m_rootDrawCommand->addChild(renderer::TScenegraphDrawCommandDataColoredRectangle{
        background, QRectF(0.0, 0.0, viewportSize.width(), viewportSize.height()) });

    renderer::TScenegraphDrawCommand cameraCommand(renderer::TScenegraphDrawCommandDataCamera{ matrix });
    renderer::TScenegraphDrawCommand opacityCommand(
        renderer::TScenegraphDrawCommandDataOpacity{ static_cast<double>(renderer::highlightStrength()) });

    std::vector<std::optional<renderer::TScenegraphDrawCommand>> floors(FloorCount);
    for (int index = FloorCount - 1; index >= 0; --index) {
        if (index > lastFloor || index < floor)
            floors[index].reset();
        else
            floors[index].emplace();
    }

    const int floorsToQuery = floor <= GroundFloor ? GroundFloor - floor + 1 : 2;
    for (const worldmap::TWorldMapAreaElement& element : renderInfos.mapTileAreaData(floorsToQuery)) {
        std::optional<renderer::TScenegraphDrawCommand> command =
            std::visit(TCreateMapTileScenegraphDrawCommandVisitor{ *m_camera, element.area, false }, element.data);
        const int tileFloor = element.area.origin.z;
        if (command && tileFloor >= 0 && tileFloor < FloorCount && floors[tileFloor])
            floors[tileFloor]->addChild(std::move(*command));
    }

    // Every floor below the camera's is darkened once more.
    if (floor <= GroundFloor && m_levelSeparatorAlpha > 0.0f) {
        QColor separator(0, 0, 0);
        separator.setAlphaF(m_levelSeparatorAlpha);
        const worldmap::TWorldMapExtent extent = visibleArea.getExtent();
        const worldmap::TWorldMapCuboid separated(visibleArea.origin, extent.width + LevelSeparatorMargin,
                                                  extent.height + LevelSeparatorMargin);
        for (int index = lastFloor; index > floor; --index) {
            if (index >= 0 && index < FloorCount && floors[index]) {
                floors[index]->addChild(renderer::TScenegraphDrawCommandDataColoredRectangle{
                    separator, toRectF(m_camera->projectedSubfieldRectangle(separated)) });
            }
        }
    }

    if (m_drawSubAreaOverlays) {
        renderer::TScenegraphDrawCommand overlays;
        const std::vector<worldmap::TWorldMapAreaElement> subAreas = renderInfos.subAreaAreaData();
        opacityCommand.setAnimated(subAreas.empty() ? opacityCommand.isAnimated() : true);
        for (const worldmap::TWorldMapAreaElement& element : subAreas) {
            std::optional<renderer::TScenegraphDrawCommand> command =
                std::visit(TSubAreaOverlaysMapDataVisitor{ element.area }, element.data);
            overlays.addChild(std::move(command).value());
        }
        opacityCommand.addChild(std::move(overlays));
    }

    renderer::TScenegraphDrawCommand belowFloorsMarkers;
    renderer::TScenegraphDrawCommand markers;
    if (sprites::TResourceSpritesManager* manager = renderInfos.resourceSpritesManager()) {
        for (const worldmap::TWorldMapAreaElement& element : renderInfos.markerAreaData()) {
            const worldmap::TWorldMapCoordinate& coordinate = element.area.origin;
            const QPoint position =
                m_camera->convertSubfieldCoordinateToPoint(worldmap::TWorldMapSubfieldCoordinate::fromField(coordinate));
            TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor::TResult result =
                std::visit(TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor{ *manager, position }, element.data);
            if (!result.command)
                continue;
            renderer::TScenegraphDrawCommand& list = result.belowFloors ? belowFloorsMarkers : markers;
            if (coordinate.z == floor) {
                list.addChild(std::move(*result.command));
            } else if (floor <= GroundFloor && coordinate.z <= GroundFloor) {
                // Markers on the other floors above ground show faded.
                renderer::TScenegraphDrawCommand faded(
                    renderer::TScenegraphDrawCommandDataOpacity{ static_cast<double>(OtherFloorMarkerOpacity) });
                faded.addChild(std::move(*result.command));
                list.addChild(std::move(faded));
            }
        }
    }

    renderer::TScenegraphDrawCommand content;
    content.addChild(std::move(belowFloorsMarkers));
    for (int index = FloorCount - 1; index >= 0; --index) {
        if (floors[index] && floors[index]->hasChilds())
            cameraCommand.addChild(std::move(*floors[index]));
    }
    cameraCommand.addChild(std::move(opacityCommand));
    content.addChild(std::move(cameraCommand));
    content.addChild(std::move(markers));
    m_rootDrawCommand->addChild(std::move(content));
}

}
