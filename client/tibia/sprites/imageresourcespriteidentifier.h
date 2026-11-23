#pragma once

#include "sprites/resourcespriteidentifier.h"

#include <QRect>
#include <QString>

namespace tibia {
namespace sprites {

/// A rectangle of an image file (CIP sprites::TImageResourceSpriteIdentifier):
/// the resolver loads the image and cuts the rectangle out of it.
class TImageResourceSpriteIdentifier final : public IResourceSpriteIdentifier
{
public:
    TImageResourceSpriteIdentifier(QString path, const QRect& rect)
        : m_path(std::move(path))
        , m_rect(rect)
    {
    }

    std::shared_ptr<ISpriteIdentifier> clone() const override;
    /// Same path and rectangle.
    bool equals(const ISpriteIdentifier& other) const override;
    /// The path's hash, then left, top, width and height, combined in order.
    size_t hash() const override;
    int resourceKind() const override { return ImageResource; }

    QString path() const { return m_path; }
    QRect rect() const { return m_rect; }

private:
    QString m_path;
    QRect m_rect;
};

}
}
