#pragma once

#include "minimap/minimapmarker.h"

#include <cip_gen.h>

namespace tibia {
namespace minimap {

/// One minimap marker as QML draws it: its text as the tooltip, its icon as
/// a symbol image and when it was last changed (CIP TMinimapMarkerQmlRenderInfo).
class TMinimapMarkerQmlRenderInfo final : public cip::tibia::minimap::TMinimapMarkerQmlRenderInfo
{
    Q_OBJECT
public:
    explicit TMinimapMarkerQmlRenderInfo(QObject* parent = nullptr);

    QString tooltiptext() const override;
    /// The marker icon of the minimap-markers image provider.
    QString symbol() const override;
    QDateTime lastModifiedTimestamp() const override;

    const TMinimapMarker& marker() const { return m_marker; }
    /// Shows `marker`; each property that changed notifies.
    void setMarker(const TMinimapMarker& marker);

private:
    TMinimapMarker m_marker{ {}, 0, {}, QDateTime::fromSecsSinceEpoch(0) };
};

}
}
