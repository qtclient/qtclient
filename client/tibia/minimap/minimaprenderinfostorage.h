#pragma once

#include <cip_gen.h>

#include <array>

namespace tibia {
namespace minimap {

/// What the Cyclopedia map's renderer draws besides the tiles (CIP:
/// TMinimapRenderInfoStorage): each change of a source marks the render infos
/// outdated and emits renderInfosChanged. Only a camera change marks the
/// second and fourth flags.
class TMinimapRenderInfoStorage final : public cip::tibia::minimap::TMinimapRenderInfoStorage
{
    Q_OBJECT
public:
    TMinimapRenderInfoStorage();
    ~TMinimapRenderInfoStorage() override;

    /// The four outdated flags, in CipSoft's order.
    const std::array<bool, 4>& outdatedFlags() const { return m_outdated; }

public Q_SLOTS:
    void onCameraViewportChanged();
    void onPassagesChanged();
    void onViewpointsChanged();
    void onActiveRaidsChanged();
    void onMinimapMarkersChanged();
    void onPlayerPositionChanged();
    void onClickedAreaChanged();

private:
    /// A source other than the camera changed: flags one and three.
    void markContentOutdated();

    std::array<bool, 4> m_outdated{};
};

}
}
