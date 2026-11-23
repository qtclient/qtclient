#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QRectF>

#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia {
namespace input {
class TEventFilterManager;
}
namespace gamewindow {

class TTutorialOverlayItemController;
class TTutorialOverlayMouseFilterController;
class TTutorialOverlayMouseTrailAnimationController;

/// The tutorial overlay (CIP: TTutorialOverlayController). While the tutorial mode
/// is on, every tutorial marker item gets a tutorial/TutorialOverlayTemplate.qml
/// item over it (a blinking border and the marker's text hint), the rest of the
/// game window is covered with mouse filter areas that swallow the mouse, and a
/// mouse trail can animate a drag and drop or a multi-use between two markers.
/// The keyboard and the mouse wheel are discarded meanwhile (TEventFilterManager).
/// The game client calls update() every frame.
class TTutorialOverlayController final : public cip::tibia::gamewindow::TTutorialOverlayController
{
    Q_OBJECT
public:
    using THint = input::TGameActionAddHintAtTutorialMarker::THint;

    /// `dialogPlaceholder`: where the overlay items go (in `engine`);
    /// `gameWindowItem`: the area the mouse filters cover.
    TTutorialOverlayController(QQmlEngine* engine, QQuickItem* dialogPlaceholder, QQuickItem* gameWindowItem,
                               input::TEventFilterManager* eventFilterManager, QObject* parent = nullptr);
    ~TTutorialOverlayController() override;

    /// Removes `rect` from every area of `areas`: an area it does not overlap stays,
    /// one it overlaps becomes the parts left, right, above and below the overlap.
    static void subtractRect(const QRectF& rect, std::vector<QRectF>& areas);

public Q_SLOTS:
    /// REMOVE_ALL_TUTORIAL_MARKERS drops the hints and the mouse trail,
    /// ADD_HINT_AT_TUTORIAL_MARKER stores a marker's hint,
    /// ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER sets the mouse trail (each then
    /// updates the overlay); CHANGE_TUTORIAL_MODE_ENABLED shows or hides it.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void onMouseFilterAreaEntered() override;
    void onMouseFilterAreaExited() override;
    /// Places an overlay item over every tutorial marker (and removes those of
    /// markers gone), the mouse filters over the rest of the game window, gives the
    /// overlay items their hints and the mouse trail its two items. Only while shown.
    void update();

private:
    /// Makes the mouse trail's item.
    void createMouseTrail();
    void showOverlay();
    /// Drops the mouse trail, the mouse filters and the overlay items.
    void hideOverlay();
    /// One mouse filter per area (made as needed, the extra ones dropped).
    void updateMouseFilters(const std::vector<QRectF>& areas);
    QQuickItem* createOverlayItem(const QString& qmlFile);

    QQmlEngine* m_engine = nullptr;
    QQuickItem* m_dialogPlaceholder = nullptr;
    /// The tutorial marker items seen, by marker id.
    std::map<int, QQuickItem*> m_markerItems;
    QQuickItem* m_gameWindowItem = nullptr;
    input::TEventFilterManager* m_eventFilterManager = nullptr;
    /// The overlay item of each marker id.
    std::map<int, std::shared_ptr<TTutorialOverlayItemController>> m_overlayItems;
    /// The text hint of each marker id.
    std::map<int, THint> m_hints;
    int m_mouseTrailSourceMarkerID = 0;
    int m_mouseTrailDestinationMarkerID = 0;
    int m_mouseTrailAnimationType = 0;
    std::shared_ptr<TTutorialOverlayMouseTrailAnimationController> m_mouseTrail;
    std::vector<std::shared_ptr<TTutorialOverlayMouseFilterController>> m_mouseFilters;
    bool m_isShown = false;
};

}
}
