#include "optimizedborderimageprovider.h"

#include <QPainter>

namespace tibia::shared {

TOptimized1PixelBorderImageProvider::TOptimized1PixelBorderImageProvider()
    : QQuickImageProvider(Image)
{
}

QImage loadOptimized1PixelBorderResourceImage(const QString& id)
{
    // CipSoft : QString::fromUtf8(":%1").arg(id) — id keeps a leading '/'.
    return QImage(QStringLiteral(":%1").arg(id));
}

QImage TOptimized1PixelBorderImageProvider::requestImage(const QString& id, QSize* size,
                                                         const QSize& requestedSize)
{
    // CipSoft : clamp requested size to at least 3×3; missing → red block.
    QSize target = requestedSize;
    if (target.width() < 3)
        target.setWidth(3);
    if (target.height() < 3)
        target.setHeight(3);

    QImage src = loadOptimized1PixelBorderResourceImage(id);
    if (src.isNull()) {
        src = QImage(target, QImage::Format_ARGB32_Premultiplied);
        src.fill(Qt::red);
    }

    // CipSoft: invalid requestedSize → return source; equal size → return source;
    // otherwise nine-patch stretch (corners via setPixel, edges/center via tiled draw).
    if (requestedSize.width() < 1 || requestedSize.height() < 1
        || (target.width() == src.width() && target.height() == src.height())) {
        if (size)
            *size = src.size();
        return src;
    }

    const int w = target.width();
    const int h = target.height();
    const int sw = src.width();
    const int sh = src.height();
    QImage out(target, src.format() == QImage::Format_Invalid
                           ? QImage::Format_ARGB32_Premultiplied
                           : src.format());
    out.fill(Qt::transparent);

    if (sw < 3 || sh < 3) {
        // Degenerate source: copy what we can; CIP still enters stretch helpers.
        QPainter p(&out);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(QRect(0, 0, w, h), src);
        p.end();
        if (size)
            *size = out.size();
        return out;
    }

    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    // Corners (CipSoft uses setPixel for the four corners).
    p.drawImage(QRect(0, 0, 1, 1), src, QRect(0, 0, 1, 1));
    p.drawImage(QRect(w - 1, 0, 1, 1), src, QRect(sw - 1, 0, 1, 1));
    p.drawImage(QRect(0, h - 1, 1, 1), src, QRect(0, sh - 1, 1, 1));
    p.drawImage(QRect(w - 1, h - 1, 1, 1), src, QRect(sw - 1, sh - 1, 1, 1));
    // Edges + center (CipSoft tiled drawImage).
    p.drawImage(QRect(1, 0, w - 2, 1), src, QRect(1, 0, sw - 2, 1));
    p.drawImage(QRect(1, h - 1, w - 2, 1), src, QRect(1, sh - 1, sw - 2, 1));
    p.drawImage(QRect(0, 1, 1, h - 2), src, QRect(0, 1, 1, sh - 2));
    p.drawImage(QRect(w - 1, 1, 1, h - 2), src, QRect(sw - 1, 1, 1, sh - 2));
    p.drawImage(QRect(1, 1, w - 2, h - 2), src, QRect(1, 1, sw - 2, sh - 2));
    p.end();
    if (size)
        *size = out.size();
    return out;
}

} // namespace tibia::shared
