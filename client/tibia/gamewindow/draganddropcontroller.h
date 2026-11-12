#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>

class QMimeData;
class QQuickItem;

namespace tibia {
namespace gamewindow {

/// While an object is dragged, turns the application's mouse events into drag
/// events for the TibiaObjectDropArea under the mouse (CIP:
/// TDragAndDropControllerEventFilter, ctor , eventFilter
/// ). Escape, another press or the application going inactive
/// cancel the drag; releasing the left button drops.
class TDragAndDropControllerEventFilter final : public cip::tibia::gamewindow::TDragAndDropControllerEventFilter
{
    Q_OBJECT
public:
    explicit TDragAndDropControllerEventFilter(QQuickItem* root, QObject* parent = nullptr);
    ~TDragAndDropControllerEventFilter() override;

    /// Leaves the current drop area (a drag-leave event).
    void leaveTarget();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void moveTo(const QPointF& scenePosition, Qt::KeyboardModifiers modifiers, bool drop);

    QPointer<QQuickItem> m_root;
    QMimeData* m_mimeData;
    QPointer<QQuickItem> m_target;
};

/// Moving objects with the mouse (CIP: TDragAndDropController, ctor
/// , the root context's tibiaDragAndDropController). A panel starts
/// the drag with its MOVE_OBJECT action; the drop area's owner takes it back
/// on the drop, with the keyboard modifiers held at the release.
class TDragAndDropController final : public cip::tibia::gamewindow::TDragAndDropController
{
    Q_OBJECT
public:
    /// `root` is searched for drop areas; `icon` is gamewindow.qml's
    /// dragApperanceInstanceIcon, shown at the mouse while dragging.
    TDragAndDropController(QQuickItem* root, QQuickItem* icon, QObject* parent = nullptr);
    ~TDragAndDropController() override;

    static TDragAndDropController* instance();

    /// CIP . `source` is the item the drag started from.
    void startDrag(const input::TGameActionPtr& action, QObject* source);
    /// CIP : the dragged action, which the drag no longer holds.
    input::TGameActionPtr takeAction();
    /// CIP : the modifiers at the drop.
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    bool isDragging() const { return m_filterInstalled; }

private:
    /// CIP : the drag ended (dropped or cancelled).
    void finish();
    /// CIP : the icon follows the mouse.
    void moveIcon();
    /// CIP : the icon shows the dragged object (Thing or CIP typeId).
    /// Creatures leave the icon empty — SingleObject cannot draw outfits.
    void updateIcon(uint16_t objectClientId, const Position& position, int stackPosition,
                    uint32_t creatureId = 0);

    QPointer<QQuickItem> m_root;
    QPointer<QQuickItem> m_icon;
    TDragAndDropControllerEventFilter* m_filter;
    input::TGameActionPtr m_action;
    QPointer<QQuickItem> m_source;
    Qt::KeyboardModifiers m_modifiers;
    bool m_filterInstalled = false;
};

}
}
