#pragma once

#include <cip_gen.h>

#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSizeF>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// CIP gamewindow::TLenshelpController: the lenshelp popup (LenshelpTemplate.qml)
/// under `parentItem`, with the caption and content of the object the mouse is
/// on, placed next to that object's scene rectangle inside the parent.
class TLenshelpController final : public cip::tibia::gamewindow::TLenshelpController
{
    Q_OBJECT
public:
    TLenshelpController(QQmlEngine* engine, QQuickItem* parentItem, QObject* parent = nullptr);
    ~TLenshelpController() override;

    /// The popup item is made once; it follows its own size changes.
    void createPopup();
    void destroyPopup();
    /// Shows `caption` / `content` next to `triggerRect` (scene coordinates).
    void show(const QString& caption, const QString& content, const QRectF& triggerRect);
    void hide();
    QRectF triggerRect() const { return m_triggerRect; }

    /// `rect` moved and cut to lie inside `bounds` (unchanged when it does).
    static QRectF clampedInto(const QRectF& bounds, const QRectF& rect);
    /// Where a popup of `size` goes for `triggerRect` inside `bounds`: right of
    /// it, left of it, above, below (each aligned to the trigger's near edge
    /// first), else along the bounds' edges.
    static QPointF popupPosition(const QSizeF& size, const QRectF& bounds, const QRectF& triggerRect);

private:
    void updatePosition();

    QQmlEngine* m_engine = nullptr;
    QPointer<QQuickItem> m_popup;
    QPointer<QQuickItem> m_parentItem;
    QRectF m_triggerRect;
};

}
}
