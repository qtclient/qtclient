#include "speechbubbleoverlay.h"

#include "chat/textoverlayrenderprovider.h"
#include "renderer/mapscale.h"
#include "renderer/worldmaprenderprovider.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace tibia::qmlcomponents {

namespace {

using Provider = TWorldMapRenderProvider;

struct Box
{
    double x, y, w, h;
    double centerX() const { return x + w * 0.5; }
    double centerY() const { return y + h * 0.5; }
};

/// Pushes overlapping bubbles apart one pixel per round, each within half its
/// size of where it wants to be, until the overlap stops shrinking (CIP:
/// ,).
void separate(const double width, const double height, std::vector<Box>& boxes)
{
    struct Range
    {
        double x, y, w, h;
    };
    std::vector<Range> ranges;
    ranges.reserve(boxes.size());
    for (Box& box : boxes) {
        const double maxX = width - box.w, maxY = height - box.h;
        const double loX = std::max(std::min(maxX, box.x - box.w * 0.5), 0.0);
        const double loY = std::max(std::min(maxY, box.y - box.h * 0.5), 0.0);
        const double hiX = std::max(std::min(maxX, box.x + box.w * 0.5), 0.0);
        const double hiY = std::max(std::min(maxY, box.y + box.h * 0.5), 0.0);
        ranges.push_back({ loX, loY, hiX - loX, hiY - loY });
        if (box.x + box.w > width)
            box.x = width - box.w;
        if (box.y + box.h > height)
            box.y = height - box.h;
        if (box.x < 0)
            box.x = 0;
        if (box.y < 0)
            box.y = 0;
    }

    const size_t count = boxes.size();
    std::vector<QPointF> forces(count);
    double previous = std::numeric_limits<double>::infinity();
    while (true) {
        std::fill(forces.begin(), forces.end(), QPointF());
        double overlap = 0;
        for (size_t i = 0; i + 1 < count; ++i) {
            const Box& a = boxes[i];
            const QRectF rectA(a.x, a.y, a.w, a.h);
            const double inverseDiagonal = 1.0 / std::sqrt(a.w * a.w + a.h * a.h);
            for (size_t j = i + 1; j < count; ++j) {
                const Box& b = boxes[j];
                const QRectF common = rectA & QRectF(b.x, b.y, b.w, b.h);
                if (common.width() <= 0 || common.height() <= 0)
                    continue;
                const double dx = a.centerX() - b.centerX();
                const double dy = a.centerY() - b.centerY();
                if (dx < 0) {
                    forces[i].rx() -= common.width();
                    forces[j].rx() += common.width();
                } else if (dx > 0) {
                    forces[i].rx() += common.width();
                    forces[j].rx() -= common.width();
                }
                if (dy < 0) {
                    forces[i].ry() -= common.height();
                    forces[j].ry() += common.height();
                } else if (dy > 0) {
                    forces[i].ry() += common.height();
                    forces[j].ry() -= common.height();
                }
                overlap += common.width() * common.height() * std::exp2(1.0 - std::sqrt(dx * dx + dy * dy) * inverseDiagonal);
                if (dx == 0 && dy == 0) {
                    // CIP converts (i - j) as unsigned: the earlier bubble goes up-left.
                    const double push = static_cast<double>(static_cast<uint64_t>(static_cast<int64_t>(i) - static_cast<int64_t>(j)));
                    forces[i].rx() += common.width() * -0.5 * push;
                    forces[j].rx() += common.width() * 0.5 * push;
                    forces[i].ry() += common.height() * -0.5 * push;
                    forces[j].ry() += common.height() * 0.5 * push;
                }
            }
        }
        for (size_t i = 0; i < count; ++i) {
            Box& box = boxes[i];
            const Range& range = ranges[i];
            if (forces[i].x() > 0)
                box.x = std::min(box.x + 1, range.x + range.w);
            else if (forces[i].x() < 0)
                box.x = std::max(box.x - 1, range.x);
            if (forces[i].y() < 0)
                box.y = std::max(box.y - 1, range.y);
            else if (forces[i].y() > 0)
                box.y = std::min(box.y + 1, range.y + range.h);
        }
        if (previous <= overlap)
            return;
        previous = overlap;
    }
}

}

TSpeechBubbleController::TSpeechBubbleController(const QColor& textColor, const QColor& styleColor, const int maximumWidth, QObject* parent)
    : cip::tibia::qmlcomponents::TSpeechBubbleController(parent)
{
    m_textColor = textColor;
    m_styleColor = styleColor;
    m_isHtmlText = true;
    m_maximumWidth = maximumWidth;
}

void TSpeechBubbleController::setText(const bool visible, const QString& text, const QColor& color, const bool isHtml)
{
    if (!visible) {
        m_hidden = true;
        return;
    }
    QString html = isHtml ? text : text.toHtmlEscaped();
    html.replace(QLatin1String("\n"), QLatin1String("<br>\n"));
    if (m_textColor != color) {
        m_textColor = color;
        Q_EMIT formatChanged();
    }
    if (m_text != html) {
        m_text = html;
        Q_EMIT textChanged();
    }
    m_hidden = false;
}

void TSpeechBubbleController::setVisible(const bool visible)
{
    if (m_isVisible == visible)
        return;
    m_isVisible = visible;
    Q_EMIT visibleChanged();
}

void TSpeechBubbleController::setPosition(const QPoint& position)
{
    if (m_position == position)
        return;
    m_position = position;
    Q_EMIT positionChanged();
}

TSpeechBubbleOverlayQuickItem::TSpeechBubbleOverlayQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TSpeechBubbleOverlayQuickItem(parent)
{}

TSpeechBubbleOverlayQuickItem::~TSpeechBubbleOverlayQuickItem() = default;

void TSpeechBubbleOverlayQuickItem::setSources(TWorldMapRenderProvider* provider,
                                               std::shared_ptr<chat::TTextOverlayRenderProvider> textOverlays)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    if (textOverlays != m_textOverlays) {
        // A provider reports the on-screen lines that differ from hidden ones
        // first: the lines start hidden.
        for (TSpeechBubbleController* controller : m_fixed) {
            if (controller)
                controller->setHidden(true);
        }
    }
    m_textOverlays = std::move(textOverlays);
    if (provider && m_textOverlays) {
        setup();
        m_frameConnection = connect(provider, &Provider::overlaysAdvanced, this, &TSpeechBubbleOverlayQuickItem::triggerUpdate);
    }
    triggerUpdate();
}

void TSpeechBubbleOverlayQuickItem::setStatusBottomMargin(const int margin)
{
    const int next = std::max(0, margin);
    if (next == m_statusBottomMargin)
        return;
    m_statusBottomMargin = next;
    triggerUpdate();
}

void TSpeechBubbleOverlayQuickItem::setup()
{
    if (m_component)
        return;
    QQmlEngine* engine = qmlEngine(this);
    if (!engine)
        return;
    m_component = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/SpeechBubbleTemplate.qml")),
                                    QQmlComponent::PreferSynchronous, this);
    if (m_component->isError()) {
        return;
    }
    for (auto& fixed : m_fixed)
        fixed = createController();
}

TSpeechBubbleController* TSpeechBubbleOverlayQuickItem::createController()
{
    auto* controller = new TSpeechBubbleController(Qt::white, Qt::black, m_maximumWidth, this);
    m_all.push_back(controller);
    if (!m_component || !m_component->isReady())
        return controller;
    QObject* object = m_component->beginCreate(qmlContext(this));
    controller->item = qobject_cast<QQuickItem*>(object);
    if (!controller->item) {
        delete object;
        return controller;
    }
    m_component->completeCreate();
    controller->item->setProperty("controller", QVariant::fromValue<QObject*>(controller));
    controller->item->setParentItem(this);
    return controller;
}

QPointF TSpeechBubbleOverlayQuickItem::anchorOf(const worldmap::TWorldMapSubfieldCoordinate& anchor) const
{
    const int MapWidth = m_provider->frame().visibleWidth * Provider::FieldSize;
    const int MapHeight = m_provider->frame().visibleHeight * Provider::FieldSize;
    const MapCoverScale cover = MapCoverScale::placed(width(), height(), MapWidth, MapHeight, m_provider->frame().viewScale);
    const QPoint camera = m_provider->frame().camera;
    // The anchor's field on the screen, then the anchor's place in the field.
    const QPoint subfield = anchor.toMapPixel();
    const QPoint inField(subfield.x() % Provider::FieldSize, subfield.y() % Provider::FieldSize);
    const Position field((subfield.x() - inField.x()) / Provider::FieldSize, (subfield.y() - inField.y()) / Provider::FieldSize,
                         static_cast<uint8_t>(anchor.z));
    const QPoint pixel = m_provider->mapPixelOf(field) + inField - camera;
    return cover.toPaneRounded(pixel.x(), pixel.y());
}

void TSpeechBubbleOverlayQuickItem::placeAbove(TSpeechBubbleController& controller, const worldmap::TWorldMapSubfieldCoordinate& anchor) const
{
    const QPointF point = anchorOf(anchor);
    const QSize size = controller.calculatedSize();
    controller.desiredPosition = QPoint(static_cast<int>(std::floor(point.x() - size.width() * 0.5)),
                                        static_cast<int>(std::floor(point.y() - size.height())));
}

void TSpeechBubbleOverlayQuickItem::updateScreenOverlays()
{
    for (const chat::TTextOverlay& overlay : m_textOverlays->changedScreenOverlays()) {
        if (overlay.type >= Status && overlay.type < FixedCount)
            m_fixed[overlay.type]->setText(overlay.visible, overlay.text, overlay.color, overlay.isHtml);
    }
}

void TSpeechBubbleOverlayQuickItem::updateWorldBubbles(std::vector<TSpeechBubbleController*>& controllers,
                                                       const std::vector<chat::TTextOverlay>& overlays)
{
    for (TSpeechBubbleController* controller : controllers)
        controller->setHidden(true);
    size_t index = 0;
    for (const chat::TTextOverlay& overlay : overlays) {
        if (index == controllers.size())
            controllers.push_back(createController());
        TSpeechBubbleController& controller = *controllers[index++];
        controller.setText(overlay.visible, overlay.text, overlay.color, overlay.isHtml);
        placeAbove(controller, overlay.coordinate);
    }
}

void TSpeechBubbleOverlayQuickItem::updateNpcBubbles()
{
    updateWorldBubbles(m_npcBubbles, m_textOverlays->npcOverlays());
}

void TSpeechBubbleOverlayQuickItem::updateCreatureBubbles()
{
    updateWorldBubbles(m_creatureBubbles, m_textOverlays->creatureOverlays());
}

void TSpeechBubbleOverlayQuickItem::layout()
{
    const double w = width(), h = height();
    const auto place = [](TSpeechBubbleController* controller, const double x, const double y) {
        controller->desiredPosition = QPoint(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    };
    const auto size = [](TSpeechBubbleController* controller) { return QSizeF(controller->calculatedSize()); };
    // Classic: CIP bottom inset. Packs raise the line via setStatusBottomMargin.
    double textH = size(m_fixed[Status]).height();
    if (m_fixed[Status]->item)
        textH = std::max(textH, m_fixed[Status]->item->height());
    const double bottom = m_margin + m_statusBottomMargin;
    place(m_fixed[Status], w * 0.5 - size(m_fixed[Status]).width() * 0.5, h - textH - bottom);
    place(m_fixed[Look], w * 0.5 - size(m_fixed[Look]).width() * 0.5, h * 0.5 - size(m_fixed[Look]).height());
    place(m_fixed[Center], w * 0.5 - size(m_fixed[Center]).width() * 0.5, h * 0.5);
    place(m_fixed[Private], w * 0.5 - size(m_fixed[Private]).width() * 0.5, h * 0.25 - size(m_fixed[Private]).height());

    std::vector<TSpeechBubbleController*> shown;
    std::vector<Box> boxes;
    for (TSpeechBubbleController* controller : m_all) {
        if (controller->isHidden()) {
            controller->setVisible(false);
            continue;
        }
        const QSize s = controller->calculatedSize();
        shown.push_back(controller);
        boxes.push_back({ static_cast<double>(controller->desiredPosition.x()), static_cast<double>(controller->desiredPosition.y()),
                          static_cast<double>(s.width()), static_cast<double>(s.height()) });
    }
    separate(w, h, boxes);
    for (size_t i = 0; i < shown.size(); ++i) {
        shown[i]->setPosition(QPoint(static_cast<int>(std::floor(boxes[i].x)), static_cast<int>(std::floor(boxes[i].y))));
        shown[i]->setVisible(true);
    }
}

void TSpeechBubbleOverlayQuickItem::triggerUpdate()
{
    if (!m_fixed[Status])
        return;
    if (!m_provider || !m_textOverlays || !m_provider->frame().valid) {
        // The on-screen lines keep their state: the provider reports only
        // the lines that change.
        for (TSpeechBubbleController* controller : m_all)
            controller->setVisible(false);
        for (TSpeechBubbleController* controller : m_npcBubbles)
            controller->setHidden(true);
        for (TSpeechBubbleController* controller : m_creatureBubbles)
            controller->setHidden(true);
        return;
    }
    updateScreenOverlays();
    updateNpcBubbles();
    updateCreatureBubbles();
    layout();
}

void TSpeechBubbleOverlayQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TSpeechBubbleOverlayQuickItem::geometryChange(newGeometry, oldGeometry);
    triggerUpdate();
}

}
