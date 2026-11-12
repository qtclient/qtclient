#pragma once

#include "input/clickareaeventfilter.h"

#include <cip_gen.h>

#include <QMetaObject>

class QQuickItem;

namespace tibia {
namespace gamewindow {

/// CIP's TUiFocusManager (ctor , vtable): the keyboard
/// focus of a part of the UI with its own text fields (the NPC trade pane,
/// TNpcDialogController +0x160; the sidebar widgets of
/// TSidebarWidgetControllerWithFocusHandling +0xa0). Taking it blocks the
/// hotkeys and takes the keyboard from the chat input
/// (TEventFilterManager::block,) and watches the clicks: one
/// outside the click area gives it back . Losing the keyboard to
/// the chat input (TEventFilterManager's bind signal) drops the focus.
class TUiFocusManager final : public cip::tibia::gamewindow::TUiFocusManager
{
    Q_OBJECT
public:
    explicit TUiFocusManager(QObject* parent = nullptr);
    ~TUiFocusManager() override;

    /// CIP : the item whose clicks keep the focus.
    void setClickArea(QQuickItem* item);
    /// CIP : an inactive manager doesn't take the focus; going
    /// inactive gives it back.
    void setActive(bool active);

public Q_SLOTS:
    /// CIP .
    void takeFocus() override;
    /// CIP .
    void releaseFocus() override;
    /// CIP : the chat input has the keyboard again.
    void onFocusTakenByEventFilterManager();

private:
    input::TClickAreaEventFilter m_clickArea;
    QMetaObject::Connection m_keyboardConnection;
};

}
}
