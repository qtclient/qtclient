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

public Q_SLOTS:
    /// The drop area the mouse was last over went away.
    void onLastFoundDropAreaDestroyed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Stops following the drop area the mouse was last over.
    void forgetLastFoundDropArea();
    /// Sends the last found drop area a drag-leave and forgets it.
    void leaveLastFoundDropArea();
    /// The topmost visible, enabled drop area at `scenePosition` below `root`
    /// that accepts `keys`.
    static QQuickItem* findDropArea(const QPointF& scenePosition, QQuickItem* root, const QStringList& keys);

    QQuickItem* m_root;
    QMimeData* m_mimeData;
    QQuickItem* m_lastFoundDropArea = nullptr;
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
public Q_SLOTS:
    /// The drag ended (dropped or cancelled).
    void onDragAndDropEventFilterFinished();
    /// The drop happened with these modifiers held.
    void onDragAndDropConfirmed(Qt::KeyboardModifiers KeyboardModifiers);
    /// The icon follows the mouse.
    void onDragAndDropMousePositionChanged();

private:
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
