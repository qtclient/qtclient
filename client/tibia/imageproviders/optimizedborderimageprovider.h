#pragma once

#include <QQuickImageProvider>

namespace tibia::qmlcomponents {

/// image://optimized1pixelborderimage/<image path>: a resource image with a
/// 1 pixel border, pre-stretched to the requested size so Optimized1PixelBorderImage
/// draws one textured quad instead of a nine-patch.
class OptimizedBorderImageProvider final : public QQuickImageProvider
{
public:
    static constexpr auto Id = "optimized1pixelborderimage";

    OptimizedBorderImageProvider() : QQuickImageProvider(Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

/// The resource image an image provider id names ("/images/x.png" or "images/x.png").
QImage loadResourceImage(const QString& id);

}
