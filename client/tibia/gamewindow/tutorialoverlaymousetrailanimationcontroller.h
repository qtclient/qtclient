#pragma once

#include <cip_gen.h>

#include <memory>
#include <utility>

class QQuickItem;

namespace tibia {
namespace gamewindow {

class TTutorialOverlayItemController;

/// The tutorial overlay's mouse trail (CIP: TTutorialOverlayMouseTrailAnimationController):
/// tutorial/TutorialOverlayMouseTrailAnimation.qml animates a mouse cursor from the
/// centre of one overlay item to the centre of another, as a drag and drop or a
/// multi-use. It owns its item.
class TTutorialOverlayMouseTrailAnimationController final
    : public cip::tibia::gamewindow::TTutorialOverlayMouseTrailAnimationController
{
    Q_OBJECT
public:
    /// The animation types of an ADD_MOUSE_TRAIL_HINT_AT_TUTORIAL_MARKER action.
    enum EAnimationType
    {
        NONE = 0,
        DRAG_AND_DROP = 1,
        MULTI_USE = 2,
    };

    explicit TTutorialOverlayMouseTrailAnimationController(QQuickItem* item);
    /// The item loses its parent and is deleted later.
    ~TTutorialOverlayMouseTrailAnimationController() override;

    void setSource(std::weak_ptr<TTutorialOverlayItemController> source) { m_source = std::move(source); }
    void setDestination(std::weak_ptr<TTutorialOverlayItemController> destination) { m_destination = std::move(destination); }
    void setAnimationType(const int animationType) { m_animationType = animationType; }

    /// The item's sourcePoint and destinationPoint become the centres of the source
    /// and destination overlay items (each only while it exists), and its
    /// animationType "draganddrop", "multiuse" or "none".
    void updateAnimation();

private:
    QQuickItem* m_item = nullptr;
    std::weak_ptr<TTutorialOverlayItemController> m_source;
    std::weak_ptr<TTutorialOverlayItemController> m_destination;
    int m_animationType = NONE;
};

}
}
