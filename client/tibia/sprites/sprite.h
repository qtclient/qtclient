#pragma once

#include <QImage>
#include <QSize>

namespace tibia {
namespace sprites {

/// One drawable sprite image of known size (CIP sprites::TSprite).
class TSprite
{
public:
    TSprite(const QSize& size, double scale)
        : m_size(size)
        , m_scale(scale)
    {
    }
    virtual ~TSprite() = default;

    virtual QImage getImage() const = 0;
    virtual const QSize& getSize() const { return m_size; }
    virtual double getScale() const { return m_scale; }

    /// The resolver clears this for whole asset spritesheets.
    void setCacheable(bool cacheable) { m_cacheable = cacheable; }
    bool isCacheable() const { return m_cacheable; }
    /// The resolver sets this for sprites of appearances and image resources.
    void setFromResource(bool fromResource) { m_fromResource = fromResource; }
    bool isFromResource() const { return m_fromResource; }
    void setHighlighted(bool highlighted) { m_highlighted = highlighted; }
    bool isHighlighted() const { return m_highlighted; }

private:
    bool m_cacheable = true;
    bool m_fromResource = false;
    bool m_highlighted = false;
    QSize m_size;
    double m_scale = 0.0;
};

}
}
