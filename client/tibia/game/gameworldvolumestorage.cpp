#include "game/gameworldvolumestorage.h"

#include "appearances/appearanceinstance.h"
#include "config/clientoptions.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
#include "effects/effectstorage.h"
#include "iclockservice.h"
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldmapcameraviewport.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

namespace tibia::game {

TGameWorldVolumeStorage::TGameWorldVolumeStorage(
    std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
    std::shared_ptr<config::TClientOptions> clientOptions, std::shared_ptr<shared::IClockService> clockService)
    : m_appearancesManager(std::move(appearancesManager))
    , m_appearanceInstanceFactory(std::move(appearanceInstanceFactory))
    , m_clientOptions(std::move(clientOptions))
    , m_clockService(std::move(clockService))
    , m_worldMapViewport(std::make_shared<worldmap::TWorldMapViewport>())
    , m_worldMapCameraViewport(std::make_shared<renderer::TWorldMapCameraViewport>())
{
    // The camera's volume reaches one column / row further left and up and
    // two further right and down than it shows.
    m_worldMapCameraViewport->setAdditionalColumnsAndRows(1, 2, 1, 2);
    createStorages();
}

TGameWorldVolumeStorage::~TGameWorldVolumeStorage() = default;

void TGameWorldVolumeStorage::createStorages()
{
    m_creatureMovementSpeedCalculator = std::make_shared<creatures::TCreatureMovementSpeedCalculator>();
    m_worldMapStorage = std::make_shared<worldmap::TWorldMapStorage>(m_creatureMovementSpeedCalculator, m_clockService);
    // The stored volume is exactly the volume set, without margins.
    m_worldMapStorage->setHorizontalStoredVolumeMargins(0, 0);
    m_worldMapStorage->setVerticalStoredVolumeMargins(0, 0);
    m_ambientLightStorage = std::make_shared<worldmap::TAmbientLightStorage>();
    m_creatureStorage = std::make_shared<creatures::TCreatureStorage>();
    m_renderVolumeProvider = std::make_shared<renderer::TRenderVolumeProvider>(m_worldMapViewport, m_worldMapStorage);
    // The volume shows no effects: its effect storage holds none.
    const auto effectStorage = std::make_shared<effects::TEffectStorage>(std::size_t{ 0 });
    // Host: the factory's instance tracker is process-wide.
    m_worldMapRenderProvider = std::make_shared<qmlcomponents::TWorldMapRenderProvider>(
        m_renderVolumeProvider, m_worldMapStorage, m_creatureStorage, m_ambientLightStorage, effectStorage,
        m_clientOptions, m_appearanceInstanceFactory->instanceTracker());
}

std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> TGameWorldVolumeStorage::worldMapRenderProvider() const
{
    return m_worldMapRenderProvider;
}

std::shared_ptr<renderer::TWorldMapCameraViewport> TGameWorldVolumeStorage::worldMapCameraViewport() const
{
    return m_worldMapCameraViewport;
}

std::shared_ptr<worldmap::TWorldMapViewport> TGameWorldVolumeStorage::worldMapViewport() const
{
    return m_worldMapViewport;
}

std::shared_ptr<worldmap::TAmbientLightStorage> TGameWorldVolumeStorage::ambientLightStorage() const
{
    return m_ambientLightStorage;
}

std::shared_ptr<worldmap::TWorldMapStorage> TGameWorldVolumeStorage::worldMapStorage() const
{
    return m_worldMapStorage;
}

std::shared_ptr<creatures::TCreatureStorage> TGameWorldVolumeStorage::creatureStorage() const
{
    return m_creatureStorage;
}

std::shared_ptr<appearances::TAppearanceInstanceFactory> TGameWorldVolumeStorage::appearanceInstanceFactory() const
{
    return m_appearanceInstanceFactory;
}

std::shared_ptr<renderer::TRenderVolumeProvider> TGameWorldVolumeStorage::renderVolumeProvider() const
{
    return m_renderVolumeProvider;
}

void TGameWorldVolumeStorage::updateViewportFromCamera()
{
    const worldmap::TWorldMapExtent extent = m_worldMapCameraViewport->volumeExtent();
    if (extent.width == 0 && extent.height == 0)
        return;
    m_worldMapViewport->setSize(extent.width * 32, extent.height * 32);
    m_worldMapViewport->setCenter(m_worldMapCameraViewport->centerSubfieldCoordinate());
    m_renderVolumeProvider->updateRenderVolume();
    // Host: advance() is the provider's public build of the next frame.
    m_worldMapRenderProvider->advance();
}

}
