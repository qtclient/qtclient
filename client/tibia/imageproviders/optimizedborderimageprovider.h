#pragma once

#include <QQuickImageProvider>

namespace tibia {
namespace shared {

/// CIP `shared::TOptimized1PixelBorderImageProvider`
/// (image://optimized1pixelborderimage/<image path>): a resource image with a
/// 1 pixel border, grown to the requested size (at least 3x3) by keeping the
/// corners and tiling the edges and the centre, so Optimized1PixelBorderImage
/// draws one textured quad instead of a nine-patch. A missing image is red.
class TOptimized1PixelBorderImageProvider final : public QQuickImageProvider
{
public:
    static constexpr auto Id = "optimized1pixelborderimage";

    TOptimized1PixelBorderImageProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

/// The resource ":<id>" (ids keep their leading '/').
QImage loadOptimized1PixelBorderResourceImage(const QString& id);

} // namespace shared

namespace qmlcomponents {

using OptimizedBorderImageProvider = shared::TOptimized1PixelBorderImageProvider;

inline QImage loadResourceImage(const QString& id)
{
    return shared::loadOptimized1PixelBorderResourceImage(id);
}

} // namespace qmlcomponents
}
