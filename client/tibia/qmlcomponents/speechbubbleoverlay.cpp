#include "speechbubbleoverlay.h"

#include "chat/textstorage.h"
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
/// FUN_1010d1d70, FUN_1010d2140).
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

void TSpeechBubbleOverlayQuickItem::setSources(TWorldMapRenderProvider* provider, const chat::TTextStorage* texts)
{
    disconnect(m_frameConnection);
    m_provider = provider;
    m_texts = texts;
    if (provider && texts) {
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
        qWarning().noquote() << "SpeechBubbleTemplate:" << m_component->errorString();
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

QPointF TSpeechBubbleOverlayQuickItem::anchorOf(const Position& field) const
{
    const int MapWidth = m_provider->frame().visibleWidth * Provider::FieldSize;
    const int MapHeight = m_provider->frame().visibleHeight * Provider::FieldSize;
    const MapCoverScale cover = MapCoverScale::placed(width(), height(), MapWidth, MapHeight, m_provider->frame().viewScale);
    const QPoint camera = m_provider->frame().camera;
    const QPoint pixel = m_provider->mapPixelOf(field) + QPoint(Provider::FieldSize / 2, 0) - camera;
    return cover.toPaneRounded(pixel.x(), pixel.y());
}

void TSpeechBubbleOverlayQuickItem::placeAbove(TSpeechBubbleController& controller, const Position& field) const
{
    const QPointF anchor = anchorOf(field);
    const QSize size = controller.calculatedSize();
    controller.desiredPosition = QPoint(static_cast<int>(std::floor(anchor.x() - size.width() * 0.5)),
                                        static_cast<int>(std::floor(anchor.y() - size.height())));
}

void TSpeechBubbleOverlayQuickItem::updateNpcBubbles()
{
    for (TSpeechBubbleController* controller : m_npcBubbles)
        controller->setHidden(true);
    size_t index = 0;
    for (const chat::TNpcSpeechBubble& bubble : m_texts->npcBubbles()) {
        if (!bubble.isVisible())
            continue;
        if (index == m_npcBubbles.size())
            m_npcBubbles.push_back(createController());
        TSpeechBubbleController& controller = *m_npcBubbles[index++];
        controller.setText(true, bubble.bubbleText(), bubble.color(), true);
        placeAbove(controller, bubble.coordinate());
    }
}

void TSpeechBubbleOverlayQuickItem::updateCreatureBubbles()
{
    for (TSpeechBubbleController* controller : m_creatureBubbles)
        controller->setHidden(true);
    size_t index = 0;
    for (const auto& bubble : m_texts->creatureBubbles()) {
        if (index == m_creatureBubbles.size())
            m_creatureBubbles.push_back(createController());
        TSpeechBubbleController& controller = *m_creatureBubbles[index++];
        controller.setText(true, bubble->text(), bubble->color(), false);
        placeAbove(controller, bubble->coordinate());
    }
}

void TSpeechBubbleOverlayQuickItem::layout()
{
    const double w = width(), h = height();
    const auto place = [](TSpeechBubbleController* controller, const double x, const double y) {
        controller->desiredPosition = QPoint(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    };
    const auto size = [](TSpeechBubbleController* controller) { return QSizeF(controller->calculatedSize()); };
    // Classic: CIP bottom inset only. HUD: clear above SELL/NPC (ServicesBar) when
    // those pills are up; otherwise clear the automation strip.
    double textH = size(m_fixed[Status]).height();
    if (m_fixed[Status]->item)
        textH = std::max(textH, m_fixed[Status]->item->height());
    double bottom = m_margin + m_statusBottomMargin;
    if (QQuickItem* host = parentItem()) {
        const auto named = [](QQuickItem* parent, const char* objectName) -> QQuickItem* {
            for (QQuickItem* child : parent->childItems()) {
                if (child->objectName() == QLatin1String(objectName))
                    return child;
            }
            return nullptr;
        };
        QQuickItem* pills = named(host, "modServicesBar");
        if (pills && pills->isVisible() && pills->height() > 0) {
            const double pillsTop = pills->mapToItem(this, QPointF(0, 0)).y();
            if (pillsTop >= 0)
                bottom = std::max(bottom, h - pillsTop + 12.0);
        } else if (QQuickItem* automation = named(host, "modAutomationBar")) {
            if (automation->isVisible() && automation->height() > 0) {
                const double autoTop = automation->mapToItem(this, QPointF(0, 0)).y();
                if (autoTop >= 0)
                    bottom = std::max(bottom, h - autoTop + 12.0);
            }
        }
    }
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
    if (!m_provider || !m_texts || !m_provider->frame().valid) {
        for (TSpeechBubbleController* controller : m_all) {
            controller->setHidden(true);
            controller->setVisible(false);
        }
        return;
    }
    m_fixed[Status]->setText(m_texts->isStatusVisible(), m_texts->statusText(), m_texts->statusColor(), false);
    m_fixed[Look]->setText(m_texts->isLookVisible(), m_texts->lookText(), m_texts->lookColor(), m_texts->isLookHtml());
    m_fixed[Center]->setText(m_texts->centerMessages().isVisible(), m_texts->centerMessages().text(), m_texts->centerMessages().color(), false);
    m_fixed[Private]->setText(m_texts->privateMessages().isVisible(), m_texts->privateMessages().text(), m_texts->privateMessages().color(), false);
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
