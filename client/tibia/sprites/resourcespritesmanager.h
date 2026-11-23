#pragma once

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>

#include <map>
#include <memory>

namespace tibia {
namespace sprites {

/// The client's built-in resource images the Cyclopedia map draws (CIP
/// sprites::TResourceSpritesManager), keyed by resource id: 2 the minimap
/// marker symbol strip, 3 the NPC icon, 4/5 the passage icon (idle /
/// selected), 6 the point-of-interest icon, 7 the raid icon, 8 the player icon,
/// 9 the house-status icon.
class TResourceSpritesManager : public QObject, public std::enable_shared_from_this<TResourceSpritesManager>
{
    Q_OBJECT
public:
    /// One resource image. `size` is the size of one cell: the images are strips
    /// of square cells, so it is the image's height in both directions.
    struct TResourceSprite
    {
        QImage image;
        QSize size{-1, -1};
        QString path;
    };

    /// Loads the eight resource images.
    TResourceSpritesManager();

    /// Loads the image at `path` and stores it as resource `id` (replacing an
    /// earlier one). A file that does not load gives a null image and an empty cell.
    void addResourceSprite(const QString& path, int id);

    /// The pixel rectangle one cell of resource `id` covers when drawn at
    /// `position` (its top-left, or its centre when `centered`). Throws
    /// std::out_of_range for an id that was never added.
    QRect resourceRectangle(int id, const QPointF& position, bool centered) const;

    const std::map<int, TResourceSprite>& resourceSprites() const { return m_resourceSprites; }

private:
    std::map<int, TResourceSprite> m_resourceSprites;
};

}
}
