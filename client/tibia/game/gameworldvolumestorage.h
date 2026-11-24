#pragma once

#include <memory>

namespace tibia::appearances {
class TAppearancesManager;
class TAppearanceInstanceFactory;
}
namespace tibia::config {
class TClientOptions;
}
namespace tibia::shared {
class IClockService;
}
namespace tibia::creatures {
class TCreatureMovementSpeedCalculator;
class TCreatureStorage;
}
namespace tibia::renderer {
class TRenderVolumeProvider;
class TWorldMapCameraViewport;
}
namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}
namespace tibia::worldmap {
class TAmbientLightStorage;
class TWorldMapCuboid;
class TWorldMapStorage;
class TWorldMapViewport;
}
namespace tibia::protobuf::protocol {
class MapFields;
}

namespace tibia::game {

/// A world of its own outside the game session (CIP: TGameWorldVolumeStorage):
/// a map storage with its creatures and ambient light, the viewport and camera
/// looking at it and the render provider drawing it. The houses dialog shows a
/// house's area through one.
class TGameWorldVolumeStorage
{
public:
    TGameWorldVolumeStorage(std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
                            std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
                            std::shared_ptr<config::TClientOptions> clientOptions,
                            std::shared_ptr<shared::IClockService> clockService);
    virtual ~TGameWorldVolumeStorage();

    virtual std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider() const;
    virtual std::shared_ptr<renderer::TWorldMapCameraViewport> worldMapCameraViewport() const;
    virtual std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport() const;
    virtual std::shared_ptr<worldmap::TAmbientLightStorage> ambientLightStorage() const;
    virtual std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage() const;
    virtual std::shared_ptr<creatures::TCreatureStorage> creatureStorage() const;
    virtual std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory() const;
    virtual std::shared_ptr<renderer::TRenderVolumeProvider> renderVolumeProvider() const;

    /// The volume holds the fields of `area` (a house's area of the static map
    /// data): the storage keeps that box (with its margins), the viewport takes
    /// the box's sheared envelope (in pixels) centred on the envelope's first
    /// field, and every field of `fields` (column first, the floors from the
    /// box's first floor down; each field's skip leaves out that many fields)
    /// that the storage holds gets its objects; creatures are left out. Throws
    /// std::runtime_error when the fields do not cover the box.
    void fillFromArea(const worldmap::TWorldMapCuboid& area, const protobuf::protocol::MapFields& fields);

    /// Fits the viewport to the camera: once the camera shows any field, the
    /// viewport takes the camera's volume (in pixels) and centre, the render
    /// volume follows and the render provider builds its frame.
    void updateViewportFromCamera();

private:
    void createStorages();

    std::shared_ptr<appearances::TAppearancesManager> m_appearancesManager;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_appearanceInstanceFactory;
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<shared::IClockService> m_clockService;
    std::shared_ptr<worldmap::TWorldMapViewport> m_worldMapViewport;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<worldmap::TAmbientLightStorage> m_ambientLightStorage;
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::shared_ptr<renderer::TRenderVolumeProvider> m_renderVolumeProvider;
    std::shared_ptr<renderer::TWorldMapCameraViewport> m_worldMapCameraViewport;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    /// Host: qtclient's map storage borrows its movement speed calculator; the
    /// volume keeps it alive.
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_creatureMovementSpeedCalculator;
};

}
