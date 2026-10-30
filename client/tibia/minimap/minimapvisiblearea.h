#pragma once

#include <vector>

#include <client/position.h>

#include <cip_gen.h>

#include <QMatrix4x4>
#include <QPoint>
#include <QPointer>
#include <QSize>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::minimap {

/// The fields the minimap shows (CIP: TMinimapVisibleArea, vtable 0x10263d308):
/// a floor rectangle sized by the item's pixels and the zoom (2^zoom pixels
/// per field), and the world-to-item transform.
class TMinimapVisibleArea final : public cip::tibia::minimap::TMinimapVisibleArea
{
    Q_OBJECT
public:
    static constexpr float MinZoom = -1;
    static constexpr float MaxZoom = 2;

    explicit TMinimapVisibleArea(QObject* parent = nullptr);

    /// The character's options the zoom is read from and written to.
    void setOptionsSource(config::TSidebarOptions* options);

    const Position& topLeft() const { return m_topLeft; }
    QSize size() const { return m_size; }
    Position center() const;
    void setCenter(const Position& center);
    void setViewportSizePx(int width, int height);

    /// Whether the 256x256 tile at `id` overlaps the area.
    bool intersectsTile(const Position& id) const;

    void moveX(int dx);
    void moveY(int dy);
    void moveZ(int dz);
    int z() const { return m_topLeft.z; }
    void setZ(int z);

    const QMatrix4x4& matrix() const { return m_matrix; }
    void zoomIn();
    void zoomOut();
    float scale() const;

    /// The field under an item pixel.
    Position itemToWorld(const QPoint& itemPixel) const;
    /// The item pixel of a field's centre.
    QPoint worldToItem(int x, int y) const;

public Q_SLOTS:
    void restoreZoomLevelFromOptions() override;

private:
    void updateMatrix();
    void resizeKeepingCenter();
    void storeZoom();

    QPointer<config::TSidebarOptions> m_options;
    QMetaObject::Connection m_optionsConnection;
    Position m_topLeft{ 0, 0, 0 };
    QSize m_size{ 1, 1 };
    float m_zoom = 0;
    QSize m_viewportPx{ 1, 1 };
    QMatrix4x4 m_matrix;
};

}
