#pragma once

#include "minimap/minimapmarker.h"

#include <QPointer>

#include <vector>

namespace tibia {
namespace minimap {

class TMinimapMarkerStorage;
class TMinimapVisibleArea;

/// The markers a minimap shows (CIP: TMinimapMarkerRenderProvider): those of
/// the marker storage on the visible area's floor within five fields of it.
class TMinimapMarkerRenderProvider
{
public:
    TMinimapMarkerRenderProvider(const TMinimapVisibleArea* area, TMinimapMarkerStorage* storage);
    virtual ~TMinimapMarkerRenderProvider() = default;

    virtual std::vector<TMinimapMarkerPtr> renderInfos() const;

private:
    const TMinimapVisibleArea* m_area;
    QPointer<TMinimapMarkerStorage> m_storage;
};

}
}
