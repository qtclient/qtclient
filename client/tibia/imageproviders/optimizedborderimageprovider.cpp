#include "optimizedborderimageprovider.h"

#include <QPainter>

namespace tibia::shared {

namespace {

/// Tiles `sourceRect` of `source` over `targetRect` of `target`, unscaled; the
/// last tile in each direction is cut at the target's edge.
void tileImage(const QImage& source, const QRect& sourceRect, QImage& target, const QRect& targetRect)
{
    // Host: a source narrower than 3 pixels has an empty edge to tile (a
    // division by zero in CipSoft's code).
    if (sourceRect.width() <= 0 || sourceRect.height() <= 0)
        return;
    const int columns = targetRect.width() / sourceRect.width();
    const int rows = targetRect.height() / sourceRect.height();
    QPainter painter(&target);
    if (rows < 0 || columns < 0)
        return;
    for (int row = 0; row <= rows; ++row) {
        for (int column = 0; column <= columns; ++column) {
            const QRect tile = targetRect
                & QRect(targetRect.x() + column * sourceRect.width(), targetRect.y() + row * sourceRect.height(),
                        sourceRect.width(), sourceRect.height());
            painter.drawImage(QRectF(tile), source,
                              QRectF(sourceRect.x(), sourceRect.y(), tile.width(), tile.height()));
        }
    }
}

}

TOptimized1PixelBorderImageProvider::TOptimized1PixelBorderImageProvider()
    : QQuickImageProvider(Image)
{
}

QImage loadOptimized1PixelBorderResourceImage(const QString& id)
{
    return QImage(QStringLiteral(":%1").arg(id));
}

QImage TOptimized1PixelBorderImageProvider::requestImage(const QString& id, QSize* size,
                                                         const QSize& requestedSize)
{
    QSize target = requestedSize;
    if (target.height() < 3 || target.width() < 3)
        target = QSize(3, 3);

    QImage source = loadOptimized1PixelBorderResourceImage(id);
    if (source == QImage()) {
        source = QImage(target, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::red);
    }

    const QSize sourceSize = source.size();
    if (requestedSize.width() < 1 || requestedSize.height() < 1)
        target = sourceSize;
    if (target == sourceSize) {
        *size = QSize(source.width(), source.height());
        return source;
    }

    QImage image(target, source.format());
    // Host: CipSoft leaves the new image uninitialised before painting over it.
    image.fill(Qt::transparent);
    const int width = target.width();
    const int height = target.height();
    const int sourceWidth = sourceSize.width();
    const int sourceHeight = sourceSize.height();
    image.setPixel(0, 0, source.pixel(0, 0));
    image.setPixel(0, height - 1, source.pixel(0, sourceHeight - 1));
    image.setPixel(width - 1, 0, source.pixel(sourceWidth - 1, 0));
    image.setPixel(width - 1, height - 1, source.pixel(sourceWidth - 1, sourceHeight - 1));
    // Left, right, top and bottom edges, then the centre.
    tileImage(source, QRect(QPoint(0, 1), QPoint(0, sourceHeight - 2)), image,
              QRect(QPoint(0, 1), QPoint(0, height - 2)));
    tileImage(source, QRect(QPoint(sourceWidth - 1, 1), QPoint(sourceWidth - 1, sourceHeight - 2)), image,
              QRect(QPoint(width - 1, 1), QPoint(width - 1, height - 2)));
    tileImage(source, QRect(QPoint(1, 0), QPoint(sourceWidth - 2, 0)), image,
              QRect(QPoint(1, 0), QPoint(width - 2, 0)));
    tileImage(source, QRect(QPoint(1, sourceHeight - 1), QPoint(sourceWidth - 2, sourceHeight - 1)), image,
              QRect(QPoint(1, height - 1), QPoint(width - 2, height - 1)));
    tileImage(source, QRect(QPoint(1, 1), QPoint(sourceWidth - 2, sourceHeight - 2)), image,
              QRect(QPoint(1, 1), QPoint(width - 2, height - 2)));
    *size = QSize(image.width(), image.height());
    return image;
}

} // namespace tibia::shared
