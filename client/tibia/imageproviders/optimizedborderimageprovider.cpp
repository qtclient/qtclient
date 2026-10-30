#include "optimizedborderimageprovider.h"

#include <QPainter>

namespace tibia::qmlcomponents {

QImage loadResourceImage(const QString& id)
{
    QString path = id;
    while (path.startsWith(u'/'))
        path.remove(0, 1);
    return QImage(QStringLiteral(":/") + path);
}

QImage OptimizedBorderImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    // CIP 0x101027270: at least 3x3; a missing image shows as a red block.
    QSize target = requestedSize;
    if (target.width() < 3 || target.height() < 3)
        target = QSize(3, 3);
    QImage src = loadResourceImage(id);
    if (src.isNull()) {
        src = QImage(target, QImage::Format_ARGB32_Premultiplied);
        src.fill(Qt::red);
    }
    if (requestedSize.width() < 1 || requestedSize.height() < 1 || target == src.size() || src.width() < 3 || src.height() < 3) {
        if (size)
            *size = src.size();
        return src;
    }

    const int w = target.width();
    const int h = target.height();
    const int sw = src.width();
    const int sh = src.height();
    QImage out(target, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    // Corners, edges, center (1 pixel border).
    p.drawImage(QRect(0, 0, 1, 1), src, QRect(0, 0, 1, 1));
    p.drawImage(QRect(w - 1, 0, 1, 1), src, QRect(sw - 1, 0, 1, 1));
    p.drawImage(QRect(0, h - 1, 1, 1), src, QRect(0, sh - 1, 1, 1));
    p.drawImage(QRect(w - 1, h - 1, 1, 1), src, QRect(sw - 1, sh - 1, 1, 1));
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

}
