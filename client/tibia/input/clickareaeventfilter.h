#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia {
namespace input {

/// CIP input::TClickAreaEventFilter (ctor , eventFilter):
/// installed on the application, it reports every mouse press, release or
/// double click as inside or outside its item's scene rectangle. Never eats
/// the event.
class TClickAreaEventFilter final : public cip::tibia::input::TClickAreaEventFilter
{
    Q_OBJECT
public:
    explicit TClickAreaEventFilter(QObject* parent = nullptr);

    /// CIP .
    void setClickArea(QQuickItem* item);
    QQuickItem* clickArea() const { return m_area; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPointer<QQuickItem> m_area;
};

}
}
