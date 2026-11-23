#include "minimap/minimaprenderinfostorage.h"

namespace tibia::minimap {

TMinimapRenderInfoStorage::TMinimapRenderInfoStorage()
    : cip::tibia::minimap::TMinimapRenderInfoStorage(nullptr)
{
}

TMinimapRenderInfoStorage::~TMinimapRenderInfoStorage() = default;

void TMinimapRenderInfoStorage::onCameraViewportChanged()
{
    m_outdated = { true, true, true, true };
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::markContentOutdated()
{
    m_outdated[0] = true;
    m_outdated[2] = true;
    Q_EMIT renderInfosChanged();
}

void TMinimapRenderInfoStorage::onPassagesChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onViewpointsChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onActiveRaidsChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onMinimapMarkersChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onPlayerPositionChanged()
{
    markContentOutdated();
}

void TMinimapRenderInfoStorage::onClickedAreaChanged()
{
    markContentOutdated();
}

}
