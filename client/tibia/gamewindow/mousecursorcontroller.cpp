#include "mousecursorcontroller.h"

#include "config/clientoptions.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QPixmap>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTransform>

#include <memory>

namespace tibia::gamewindow {

namespace {

QPointer<TMouseCursorController> s_instance;
/// CIP: the controller the factory made last.
std::shared_ptr<TMouseCursorController> s_sharedInstance;

}

TMouseCursorController::TMouseCursorController(std::shared_ptr<config::TClientOptions> ClientOptions, QObject* parent)
    : cip::tibia::gamewindow::TMouseCursorController(parent)
    , m_pClientOptions(std::move(ClientOptions))
{
    s_instance = this;
    loadCursors();
    if (m_pClientOptions)
        connect(m_pClientOptions.get(), &config::TClientOptions::mouseCursorControllerOptionsChanged, this,
                &TMouseCursorController::onOptionsChanged);
    m_timer.setInterval(60);
    m_timer.setSingleShot(false);
    m_timer.start();
    connect(&m_timer, &QTimer::timeout, this, &TMouseCursorController::update);
}

TMouseCursorController::~TMouseCursorController()
{
    if (m_applied && QGuiApplication::overrideCursor())
        QGuiApplication::restoreOverrideCursor();
}

TMouseCursorController* TMouseCursorController::instance()
{
    return s_instance;
}

void TMouseCursorController::createInstance(QQmlEngine* Engine, std::shared_ptr<config::TClientOptions> ClientOptions)
{
    std::shared_ptr<TMouseCursorController> controller(new TMouseCursorController(std::move(ClientOptions)));
    Engine->rootContext()->setContextProperty(QStringLiteral("tibiaMouseCursorController"), controller.get());
    s_sharedInstance = std::move(controller);
    // Host: released with the engine, before the application goes.
    QObject::connect(Engine, &QObject::destroyed, [] { s_sharedInstance.reset(); });
}

void TMouseCursorController::onOptionsChanged()
{
    loadCursors();
}

void TMouseCursorController::loadCursors()
{
    using config::EClientOption;
    const config::TClientOptions* options = m_pClientOptions.get();
    if (options && options->boolOption(EClientOption::mouseSystemCursor)) {
        loadSystemCursors();
        return;
    }
    const bool big = options && options->boolOption(EClientOption::mouseBigCursor);
    loadStaticCursors(big);
    loadAnimatedCursors(!options || options->boolOption(EClientOption::mouseAnimatedCursor), big);
}

void TMouseCursorController::loadStaticCursors(const bool big)
{
    loadImage(Crosshair, QStringLiteral(":/cursors/cursor-crosshair.png"), 9, 9, 0, big);
    loadImage(CrosshairReject, QStringLiteral(":/cursors/cursor-crosshair-reject.png"), 9, 9, 0, big);
    loadImage(Default, QStringLiteral(":/cursors/cursor-default.png"), 0, 0, 0, big);
    loadImage(DefaultReject, QStringLiteral(":/cursors/cursor-default-reject.png"), 0, 0, 0, big);
    loadImage(Reject, QStringLiteral(":/cursors/cursor-reject.png"), 0, 0, 0, big);
    loadImage(ResizeHorizontal, QStringLiteral(":/cursors/cursor-resize-horizontal.png"), 9, 4, 0, big);
    loadImage(ResizeVertical, QStringLiteral(":/cursors/cursor-resize-vertical.png"), 4, 9, 0, big);
    loadImage(ResizeFDiagonal, QStringLiteral(":/cursors/cursor-resize-diagonal-tlbr.png"), 7, 7, 0, big);
    loadImage(ResizeBDiagonal, QStringLiteral(":/cursors/cursor-resize-diagonal-bltr.png"), 7, 7, 0, big);
    loadImage(Lenshelp, QStringLiteral(":/cursors/cursor-lenshelp.png"), 6, 6, 0, big);
    loadImage(PointingHand, QStringLiteral(":/cursors/cursor-pointinghand.png"), 0, 0, 0, big);
}

void TMouseCursorController::loadAnimatedCursors(const bool animated, const bool big)
{
    if (!animated) {
        for (const Shape shape : { Attack, Look, Open, Talk, Use, Walk, QuickLoot })
            copyShape(Default, shape);
        return;
    }
    loadImage(Attack, QStringLiteral(":/cursors/cursor-attack.png"), 0, 0, 10, big);
    loadImage(Look, QStringLiteral(":/cursors/cursor-look.png"), 0, 0, 6, big);
    loadImage(Open, QStringLiteral(":/cursors/cursor-open.png"), 0, 0, 8, big);
    loadImage(Talk, QStringLiteral(":/cursors/cursor-talk.png"), 0, 0, 6, big);
    loadImage(Use, QStringLiteral(":/cursors/cursor-use.png"), 0, 0, 8, big);
    loadImage(Walk, QStringLiteral(":/cursors/cursor-walk.png"), 0, 0, 10, big);
    loadImage(QuickLoot, QStringLiteral(":/cursors/cursor-quick-loot.png"), 0, 0, 10, big);
}

void TMouseCursorController::loadSystemCursors()
{
    loadSystemCursor(Crosshair, Qt::CrossCursor);
    loadSystemCursor(CrosshairReject, Qt::ForbiddenCursor);
    loadSystemCursor(Default, Qt::ArrowCursor);
    loadSystemCursor(DefaultReject, Qt::ForbiddenCursor);
    loadSystemCursor(Reject, Qt::ForbiddenCursor);
    loadSystemCursor(ResizeHorizontal, Qt::SizeHorCursor);
    loadSystemCursor(ResizeVertical, Qt::SizeVerCursor);
    loadSystemCursor(ResizeFDiagonal, Qt::SizeFDiagCursor);
    loadSystemCursor(ResizeBDiagonal, Qt::SizeBDiagCursor);
    loadSystemCursor(Lenshelp, Qt::WhatsThisCursor);
    loadSystemCursor(PointingHand, Qt::PointingHandCursor);
    copyShape(Default, Attack);
    loadSystemCursor(Look, Qt::WhatsThisCursor);
    loadSystemCursor(Open, Qt::OpenHandCursor);
    copyShape(Default, Talk);
    loadSystemCursor(Use, Qt::OpenHandCursor);
    copyShape(Default, Walk);
    loadSystemCursor(QuickLoot, Qt::OpenHandCursor);
}

void TMouseCursorController::loadImage(const Shape shape, const QString& path, const int hotX, const int hotY, const int fps, const bool big)
{
    Cursors& cursors = m_cursors[shape];
    cursors.frames.clear();
    cursors.fps = fps;
    const int scale = big ? 2 : 1;
    const QPixmap image = QPixmap(path).transformed(QTransform::fromScale(scale, scale));
    const int x = hotX * scale, y = hotY * scale;
    if (fps < 1) {
        cursors.frames.emplace_back(image, x, y);
        return;
    }
    const int size = 32 * scale;
    for (int frame = 0; frame < image.width() / size; ++frame)
        cursors.frames.emplace_back(image.copy(frame * size, 0, size, size), x, y);
}

void TMouseCursorController::loadSystemCursor(const Shape shape, const Qt::CursorShape cursor)
{
    m_cursors[shape].frames.assign(1, QCursor(cursor));
    m_cursors[shape].fps = 0;
}

void TMouseCursorController::copyShape(const Shape from, const Shape to)
{
    m_cursors[to] = m_cursors[from];
}

void TMouseCursorController::setLayerShape(const int region, const int shape)
{
    if (m_layerShapes.value(region, -1) == shape)
        return;
    m_layerShapes.insert(region, shape);
    update();
}

void TMouseCursorController::setFlag(bool& flag, const bool enabled)
{
    flag = enabled;
    update();
}

void TMouseCursorController::setTargetSelection(const bool enabled) { setFlag(m_targetSelection, enabled); }
void TMouseCursorController::setLenshelp(const bool enabled) { setFlag(m_lenshelp, enabled); }
void TMouseCursorController::setResizeHorizontal(const bool enabled) { setFlag(m_resizeHorizontal, enabled); }
void TMouseCursorController::setResizeVertical(const bool enabled) { setFlag(m_resizeVertical, enabled); }
void TMouseCursorController::setResizeFDiagonal(const bool enabled) { setFlag(m_resizeFDiagonal, enabled); }
void TMouseCursorController::setResizeBDiagonal(const bool enabled) { setFlag(m_resizeBDiagonal, enabled); }
void TMouseCursorController::setPointingHand(const bool enabled) { setFlag(m_pointingHand, enabled); }
void TMouseCursorController::setOpenContainer(const bool enabled) { setFlag(m_openContainer, enabled); }

void TMouseCursorController::setDefaultCursor()
{
    // The diagonal resize flags stay set.
    m_lenshelp = m_targetSelection = m_resizeHorizontal = m_resizeVertical = false;
    m_pointingHand = m_openContainer = false;
    update();
}

void TMouseCursorController::setCursorHidden(const bool hidden)
{
    if (m_cursorHidden == hidden)
        return;
    m_cursorHidden = hidden;
    update();
}

void TMouseCursorController::onEnterRegion(const int region)
{
    m_regions.removeOne(region);
    m_regions.append(region);
    update();
}

void TMouseCursorController::onExitRegion(const int region)
{
    m_regions.removeOne(region);
    update();
}

int TMouseCursorController::layerShape(const int region) const
{
    return m_layerShapes.value(region, Default);
}

int TMouseCursorController::currentShape() const
{
    if (m_targetSelection)
        return topRegion() == ContainerNoDrop ? CrosshairReject : Crosshair;
    if (m_lenshelp)
        return Lenshelp;
    if (m_resizeHorizontal)
        return ResizeHorizontal;
    if (m_resizeVertical)
        return ResizeVertical;
    if (m_resizeFDiagonal)
        return ResizeFDiagonal;
    if (m_resizeBDiagonal)
        return ResizeBDiagonal;
    if (m_pointingHand)
        return PointingHand;
    if (m_openContainer)
        return Open;
    return layerShape(topRegion());
}

void TMouseCursorController::update()
{
    if (m_cursorHidden) {
        if (m_applied) {
            if (QGuiApplication::overrideCursor())
                QGuiApplication::restoreOverrideCursor();
            m_applied = nullptr;
        }
        return;
    }
    const int shape = currentShape();
    const Cursors& cursors = m_cursors[shape >= 0 && shape < ShapeCount ? shape : Default];
    const QCursor* cursor = nullptr;
    if (!cursors.frames.empty()) {
        const qint64 frame = QDateTime::currentMSecsSinceEpoch() * cursors.fps / 1000;
        cursor = &cursors.frames[static_cast<size_t>(frame % static_cast<qint64>(cursors.frames.size()))];
    }
    if (cursor == m_applied)
        return;
    if (!QGuiApplication::overrideCursor()) {
        if (cursor)
            QGuiApplication::setOverrideCursor(*cursor);
    } else if (!cursor) {
        QGuiApplication::restoreOverrideCursor();
    } else {
        QGuiApplication::changeOverrideCursor(*cursor);
    }
    m_applied = cursor;
}

}
