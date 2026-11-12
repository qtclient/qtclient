#pragma once

#include <QQuickImageProvider>

namespace tibia {
namespace shared {

/// image://optimized1pixelborderimage/<image path>: a resource image with a
/// 1 pixel border, pre-stretched to the requested size so Optimized1PixelBorderImage
/// draws one textured quad instead of a nine-patch.
///
/// CipSoft: shared::TOptimized1PixelBorderImageProvider (ctor ,
/// requestImage).
class TOptimized1PixelBorderImageProvider final : public QQuickImageProvider
{
public:
    static constexpr auto Id = "optimized1pixelborderimage";

    TOptimized1PixelBorderImageProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

/// CipSoft loads via QString::arg(":%1", id); id is typically "/images/…".
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
