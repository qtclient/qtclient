#pragma once

#include "renderer/scenegraphdrawcommand.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapareadata.h"

#include <QPoint>

#include <memory>
#include <optional>

namespace tibia {
namespace renderer {
class TWorldMapCamera;
}
namespace sprites {
class TResourceSpritesManager;
}
namespace minimap {

class TMinimapRenderInfoStorage;

/// A map tile piece's image drawn over the fields it covers, in projected
/// subfields (CIP: TCreateMapTileScenegraphDrawCommandVisitor). Other pieces
/// draw nothing.
struct TCreateMapTileScenegraphDrawCommandVisitor
{
    const renderer::TWorldMapCamera& camera;
    worldmap::TWorldMapCuboid area;
    /// Smooth filtering of the image.
    bool smooth = false;

    std::optional<renderer::TScenegraphDrawCommand> operator()(const worldmap::TMapTileMapAreaData& data) const;
    std::optional<renderer::TScenegraphDrawCommand> operator()(const worldmap::TMinimapTileMapAreaData& data) const;
    template<typename T>
    std::optional<renderer::TScenegraphDrawCommand> operator()(const T&) const
    {
        return std::nullopt;
    }
};

/// A shown subarea's mask file highlighted in the zone colour over the fields
/// it covers (CIP: TSubAreaOverlaysMapDataVisitor). Other pieces draw nothing.
struct TSubAreaOverlaysMapDataVisitor
{
    worldmap::TWorldMapCuboid area;

    std::optional<renderer::TScenegraphDrawCommand> operator()(const worldmap::TSubAreaWorldMapAreaData& data) const;
    template<typename T>
    std::optional<renderer::TScenegraphDrawCommand> operator()(const T&) const
    {
        return std::nullopt;
    }
};

/// A marker piece's icon centred on a pixel of the view, not scaled with the
/// map (CIP: TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor).
struct TCreateUnscaledMapMarkerSceneGraphDrawCommandVisitor
{
    /// The command and whether it goes to the list drawn under the map's
    /// floors (every marker builder leaves it false).
    struct TResult
    {
        std::optional<renderer::TScenegraphDrawCommand> command;
        bool belowFloors = false;
    };

    sprites::TResourceSpritesManager& manager;
    QPoint position;

    TResult operator()(const worldmap::TNPCMarkerWorldMapAreaData& data) const;
    TResult operator()(const worldmap::TMinimapMarkerWorldMapAreaData& data) const;
    TResult operator()(const worldmap::TPassageMapAreaData& data) const;
    TResult operator()(const worldmap::TViewpointMapAreaData& data) const;
    TResult operator()(const worldmap::TActiveRaidMapAreaData& data) const;
    TResult operator()(const worldmap::TPlayerPositionMapAreaData& data) const;
    TResult operator()(const worldmap::THouseMarkerWorldMapAreaData& data) const;
    template<typename T>
    TResult operator()(const T&) const
    {
        return {};
    }
};

/// Builds the Cyclopedia map's tree of draw commands from the render infos
/// (CIP: TMinimapRendererScenegraphDrawCommandProvider): the background in the
/// tile type's colour; through the camera the floors from the deepest drawn up
/// to the camera's, each with its map tiles and, below the camera's floor, a
/// rectangle darkening it, then the pulsing subarea overlays; then the markers
/// in view pixels.
class TMinimapRendererScenegraphDrawCommandProvider
{
public:
    explicit TMinimapRendererScenegraphDrawCommandProvider(std::shared_ptr<TMinimapRenderInfoStorage> renderInfoStorage);
    virtual ~TMinimapRendererScenegraphDrawCommandProvider();

    /// The tree built last; empty before the first build.
    virtual const std::optional<renderer::TScenegraphDrawCommand>& rootDrawCommand() const { return m_rootDrawCommand; }

    void setResourceSpritesManager(sprites::TResourceSpritesManager* manager) { m_resourceSpritesManager = manager; }
    /// The renderer's showAreaCaptions setting (the tree does not depend on it).
    bool showAreaCaptions() const { return m_showAreaCaptions; }
    void setShowAreaCaptions(const bool& show)
    {
        if (m_showAreaCaptions != show)
            m_showAreaCaptions = show;
    }
    /// The shown subareas' overlays are drawn.
    bool drawSubAreaOverlays() const { return m_drawSubAreaOverlays; }
    void setDrawSubAreaOverlays(const bool& draw)
    {
        if (m_drawSubAreaOverlays != draw)
            m_drawSubAreaOverlays = draw;
    }
    /// The alpha of the rectangle darkening each floor below the camera's; 0 draws none.
    float levelSeparatorAlpha() const { return m_levelSeparatorAlpha; }
    void setLevelSeparatorAlpha(const float alpha)
    {
        if (m_levelSeparatorAlpha != alpha)
            m_levelSeparatorAlpha = alpha;
    }
    /// The fields the view spans (its size over the scale factor, in fields).
    void setViewExtent(const worldmap::TWorldMapExtent& extent);
    const worldmap::TWorldMapExtent& viewExtent() const { return m_viewExtent; }

    /// Rebuilds the tree from the render infos and the camera.
    void updateSpriteDrawCommands();

private:
    std::shared_ptr<TMinimapRenderInfoStorage> m_renderInfoStorage;
    renderer::TWorldMapCamera* m_camera = nullptr;
    sprites::TResourceSpritesManager* m_resourceSpritesManager = nullptr;
    std::optional<renderer::TScenegraphDrawCommand> m_rootDrawCommand;
    worldmap::TWorldMapSubfieldCoordinate m_subfieldCoordinate;
    worldmap::TWorldMapExtent m_viewExtent{ 0, 0, 0 };
    bool m_showAreaCaptions = false;
    bool m_drawSubAreaOverlays = false;
    float m_levelSeparatorAlpha = 0.0f;
};

}
}
