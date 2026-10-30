#pragma once

#include <cip_gen.h>

#include <QCursor>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QTimer>

#include <array>
#include <vector>

namespace tibia::gamewindow {

/// The application cursor (CIP: TMouseCursorController): the shape of the
/// topmost region under the mouse unless a flag overrides it, drawn from
/// graphics_resources' :/cursors images (options mouseAnimatedCursor,
/// mouseBigCursor, mouseSystemCursor).
class TMouseCursorController : public cip::tibia::gamewindow::TMouseCursorController
{
    Q_OBJECT
public:
    enum Region
    {
        MapWindow = 1,
        BattleList = 2,
        Container = 3,
        ContainerNoDrop = 4,
        Minimap = 5,
        TutorialOverlay = 6,
        MouseShield = 7,
        CyclopediaMap = 8,
    };

    enum Shape
    {
        Attack,
        Crosshair,
        CrosshairReject,
        Default,
        DefaultReject,
        Look,
        Open,
        Reject,
        ResizeHorizontal,
        ResizeVertical,
        ResizeFDiagonal,
        ResizeBDiagonal,
        Talk,
        Use,
        Walk,
        Lenshelp,
        PointingHand,
        QuickLoot,
        ShapeCount
    };

    explicit TMouseCursorController(const QJsonObject& options, QObject* parent = nullptr);
    ~TMouseCursorController() override;

    /// The controller exposed to QML as tibiaMouseCursorController, if any.
    static TMouseCursorController* instance();

    /// The region the mouse entered last and has not left, 0 for none.
    int topRegion() const { return m_regions.isEmpty() ? 0 : m_regions.last(); }
    /// The shape shown while `region` is on top.
    void setLayerShape(int region, int shape);
    void setTargetSelection(bool enabled);
    void setLenshelp(bool enabled);
    /// While an object is dragged the shape stays as it was (CIP +0x61).
    void setDragging(bool dragging);

    void setResizeHorizontal(bool enabled) override;
    void setResizeVertical(bool enabled) override;
    void setResizeFDiagonal(bool enabled) override;
    void setResizeBDiagonal(bool enabled) override;
    void setPointingHand(bool enabled) override;
    void setOpenContainer(bool enabled) override;
    void setDefaultCursor() override;
    void onMouseShieldEntered() override { enter(MouseShield); }
    void onMouseShieldExited() override { exit(MouseShield); }
    void onMapWindowEntered() override { enter(MapWindow); }
    void onMapWindowExited() override { exit(MapWindow); }
    void onMinimapEntered() override { enter(Minimap); }
    void onMinimapExited() override { exit(Minimap); }
    void onContainerEntered() override { enter(Container); }
    void onContainerExited() override { exit(Container); }
    void onContainerNoDropEntered() override { enter(ContainerNoDrop); }
    void onContainerNoDropExited() override { exit(ContainerNoDrop); }
    void onBattleListEntered() override { enter(BattleList); }
    void onBattleListExited() override { exit(BattleList); }
    void onTutorialOverlayEntered() override { enter(TutorialOverlay); }
    void onTutorialOverlayExited() override { exit(TutorialOverlay); }
    void onCyclopediaMapEntered() override { enter(CyclopediaMap); }
    void onCyclopediaMapExited() override { exit(CyclopediaMap); }

private:
    struct Cursors
    {
        std::vector<QCursor> frames;
        int fps = 0;
    };

    void loadImages(bool animated, bool big);
    void loadSystemCursors();
    void loadImage(Shape shape, const QString& path, int hotX, int hotY, int fps, bool big);
    void loadSystemCursor(Shape shape, Qt::CursorShape cursor);
    void copyShape(Shape from, Shape to);
    void setFlag(bool& flag, bool enabled);
    void enter(int region);
    void exit(int region);
    int currentShape() const;
    void update();

    std::array<Cursors, ShapeCount> m_cursors;
    QList<int> m_regions;
    QMap<int, int> m_layerShapes;
    QTimer m_timer;
    const QCursor* m_applied = nullptr;
    bool m_targetSelection = false;
    bool m_lenshelp = false;
    bool m_resizeHorizontal = false;
    bool m_resizeVertical = false;
    bool m_resizeFDiagonal = false;
    bool m_resizeBDiagonal = false;
    bool m_pointingHand = false;
    bool m_openContainer = false;
    bool m_dragging = false;
};

}
