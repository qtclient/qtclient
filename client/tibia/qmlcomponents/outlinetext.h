#pragma once


#include <cip_gen.h>

class QQuickText;

namespace tibia::qmlcomponents {

/// The Qt Quick texts behind an outline text item (CIP: TOutlineTextQuickItem's
/// children): an outlined text and, in high quality, a plain one over it.
class TOutlineTextChildren
{
public:
    explicit TOutlineTextChildren(cip::tibia::qmlcomponents::TOutlineTextQuickItem* owner);

    /// Pushes the owner's properties into the texts.
    void sync();
    void resize();
    bool truncated() const;

private:
    cip::tibia::qmlcomponents::TOutlineTextQuickItem* m_owner;
    QQuickText* m_outline;
    QQuickText* m_fill = nullptr;
};

/// QML OutlineText: text drawn with an outline (Qt Quick Text, style Outline).
class TOutlineTextQuickItem : public cip::tibia::qmlcomponents::TOutlineTextQuickItem
{
    Q_OBJECT
public:
    explicit TOutlineTextQuickItem(QQuickItem* parent = nullptr);

    bool truncated() const override { return m_children.truncated(); }
    void syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other) override;

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    TOutlineTextChildren m_children;
};

/// QML CachedOutlineText. CIP renders these into a shared texture cache
/// (TCachedOutlineTextTextureManager); here they draw like OutlineText.
class TCachedOutlineTextQuickItem : public cip::tibia::qmlcomponents::TCachedOutlineTextQuickItem
{
    Q_OBJECT
public:
    explicit TCachedOutlineTextQuickItem(QQuickItem* parent = nullptr);

    bool truncated() const override { return m_children.truncated(); }
    void setHighQuality(bool highQuality) override;
    void syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other) override;

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    TOutlineTextChildren m_children;
};

}
