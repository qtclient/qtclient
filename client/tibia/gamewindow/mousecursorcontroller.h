#pragma once

#include <cip_gen.h>

#include <QCursor>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QTimer>

#include <array>
#include <vector>

class QQmlEngine;

namespace tibia {
namespace gamewindow {

/// The application cursor (CIP: TMouseCursorController): the shape of the
/// topmost region under the mouse unless a flag overrides it, drawn from
/// graphics_resources' :/cursors images (options mouseAnimatedCursor,
/// mouseBigCursor, mouseSystemCursor). Reloads when the options change.
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

    /// CIP: makes the process-wide controller (a new one replaces the old)
    /// and offers it to the engine's QML as `tibiaMouseCursorController`.
    /// Host: the options are the host's option JSON (CipSoft: the client
    /// options), and the controller goes with the engine (Qt objects must
    /// not outlive the application).
    static void createInstance(QQmlEngine* Engine, const QJsonObject& Options);

    /// The region the mouse entered last and has not left, 0 for none.
    int topRegion() const { return m_regions.isEmpty() ? 0 : m_regions.last(); }
    /// The shape shown while `region` is on top.
    void setLayerShape(int region, int shape);
    /// Crosshair while a target is picked (drag and drop, use-with).
    void setTargetSelection(bool enabled);
    void setLenshelp(bool enabled);
    /// Hands the cursor back to the system while set.
    void setCursorHidden(bool hidden);

    Q_INVOKABLE void onEnterRegion(int region);
    Q_INVOKABLE void onExitRegion(int region);
    void onOptionsChanged();

    void setResizeHorizontal(bool enabled) override;
    void setResizeVertical(bool enabled) override;
    void setResizeFDiagonal(bool enabled) override;
    void setResizeBDiagonal(bool enabled) override;
    void setPointingHand(bool enabled) override;
    void setOpenContainer(bool enabled) override;
    void setDefaultCursor() override;
    void onMouseShieldEntered() override { onEnterRegion(MouseShield); }
    void onMouseShieldExited() override { onExitRegion(MouseShield); }
    void onMapWindowEntered() override { onEnterRegion(MapWindow); }
    void onMapWindowExited() override { onExitRegion(MapWindow); }
    void onMinimapEntered() override { onEnterRegion(Minimap); }
    void onMinimapExited() override { onExitRegion(Minimap); }
    void onContainerEntered() override { onEnterRegion(Container); }
    void onContainerExited() override { onExitRegion(Container); }
    void onContainerNoDropEntered() override { onEnterRegion(ContainerNoDrop); }
    void onContainerNoDropExited() override { onExitRegion(ContainerNoDrop); }
    void onBattleListEntered() override { onEnterRegion(BattleList); }
    void onBattleListExited() override { onExitRegion(BattleList); }
    void onTutorialOverlayEntered() override { onEnterRegion(TutorialOverlay); }
    void onTutorialOverlayExited() override { onExitRegion(TutorialOverlay); }
    void onCyclopediaMapEntered() override { onEnterRegion(CyclopediaMap); }
    void onCyclopediaMapExited() override { onExitRegion(CyclopediaMap); }

private:
    struct Cursors
    {
        std::vector<QCursor> frames;
        int fps = 0;
    };

    void loadCursors(const QJsonObject& options);
    void loadStaticCursors(bool big);
    void loadAnimatedCursors(bool animated, bool big);
    void loadSystemCursors();
    void loadImage(Shape shape, const QString& path, int hotX, int hotY, int fps, bool big);
    void loadSystemCursor(Shape shape, Qt::CursorShape cursor);
    void copyShape(Shape from, Shape to);
    void setFlag(bool& flag, bool enabled);
    /// The shape set for `region`, Default when none was set.
    int layerShape(int region) const;
    int currentShape() const;
    void update();

    QTimer m_timer;
    QList<int> m_regions;
    QMap<int, int> m_layerShapes;
    std::array<Cursors, ShapeCount> m_cursors;
    const QCursor* m_applied = nullptr;
    bool m_lenshelp = false;
    bool m_targetSelection = false;
    bool m_resizeHorizontal = false;
    bool m_resizeVertical = false;
    bool m_resizeFDiagonal = false;
    bool m_resizeBDiagonal = false;
    bool m_pointingHand = false;
    bool m_openContainer = false;
    bool m_cursorHidden = false;
};

}
}
