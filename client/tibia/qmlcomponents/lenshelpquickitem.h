#pragma once

#include <cip_gen.h>

#include <QQuickItem>

namespace tibia {
namespace qmlcomponents {

/// CIP qmlcomponents::TLenshelpQuickItem (QML Lenshelp): an overlay over a part
/// of the UI with its lenshelp caption and content. It exists for the manager
/// (shown only in lenshelp mode) and shows its text while the mouse is on it,
/// at `triggerRect` when that has an area, else at the whole item.
class TLenshelpQuickItem : public cip::tibia::qmlcomponents::TLenshelpQuickItem
{
    Q_OBJECT
public:
    explicit TLenshelpQuickItem(QQuickItem* parent = nullptr);
    ~TLenshelpQuickItem() override;

protected:
    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
};

}
}
