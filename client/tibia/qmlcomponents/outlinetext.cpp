#include "outlinetext.h"

#include <QtQuick/private/qquicktext_p.h>

namespace tibia::qmlcomponents {

namespace {

using Base = cip::tibia::qmlcomponents::TOutlineTextQuickItem;

/// Qt Quick Text's defaults; the generated base starts at zero.
void initDefaults(int& hAlign, int& vAlign, int& textFormat, double& lineHeight)
{
    hAlign = QQuickText::AlignLeft;
    vAlign = QQuickText::AlignTop;
    textFormat = QQuickText::AutoText;
    lineHeight = 1.0;
}

QQuickText* createText(Base* owner, const QQuickText::TextStyle style)
{
    auto* text = new QQuickText(owner);
    text->setStyle(style);
    return text;
}

void copyProperties(Base& to, const Base& from)
{
    to.setText(from.text());
    to.setFont(from.font());
    to.setColor(from.color());
    to.setStyleColor(from.styleColor());
    to.setHorizontalAlignment(from.horizontalAlignment());
    to.setVerticalAlignment(from.verticalAlignment());
    to.setElide(from.elide());
    to.setWrapMode(from.wrapMode());
    to.setTextFormat(from.textFormat());
    to.setRenderType(from.renderType());
    to.setLineHeight(from.lineHeight());
    to.setLineHeightMode(from.lineHeightMode());
}

}

TOutlineTextChildren::TOutlineTextChildren(Base* owner)
    : m_owner(owner)
    , m_outline(createText(owner, QQuickText::Outline))
{
    const auto sync = [this] { this->sync(); };
    QObject::connect(owner, &Base::textChanged, owner, sync);
    QObject::connect(owner, &Base::fontChanged, owner, sync);
    QObject::connect(owner, &Base::colorChanged, owner, sync);
    QObject::connect(owner, &Base::styleColorChanged, owner, sync);
    QObject::connect(owner, &Base::horizontalAlignmentChanged, owner, sync);
    QObject::connect(owner, &Base::verticalAlignmentChanged, owner, sync);
    QObject::connect(owner, &Base::elideModeChanged, owner, sync);
    QObject::connect(owner, &Base::wrapModeChanged, owner, sync);
    QObject::connect(owner, &Base::textFormatChanged, owner, sync);
    QObject::connect(owner, &Base::renderTypeChanged, owner, sync);
    QObject::connect(owner, &Base::lineHeightChanged, owner, sync);
    QObject::connect(owner, &Base::lineHeightModeChanged, owner, sync);
    QObject::connect(owner, &Base::highQualityChanged, owner, sync);
    QObject::connect(owner, &QQuickItem::antialiasingChanged, owner, sync);

    // The item is as big as its text unless sized explicitly.
    const auto implicitSize = [this] {
        m_owner->setImplicitWidth(m_outline->implicitWidth());
        m_owner->setImplicitHeight(m_outline->implicitHeight());
    };
    QObject::connect(m_outline, &QQuickItem::implicitWidthChanged, owner, implicitSize);
    QObject::connect(m_outline, &QQuickItem::implicitHeightChanged, owner, implicitSize);
    QObject::connect(m_outline, &QQuickText::truncatedChanged, owner, &Base::truncatedChanged);
}

bool TOutlineTextChildren::truncated() const
{
    return m_outline->truncated();
}

void TOutlineTextChildren::sync()
{
    const bool highQuality = m_owner->highQuality();
    if (highQuality && !m_fill) {
        m_fill = createText(m_owner, QQuickText::Normal);
        resize();
    } else if (!highQuality && m_fill) {
        delete m_fill;
        m_fill = nullptr;
    }

    for (QQuickText* text : { m_outline, m_fill }) {
        if (!text)
            continue;
        text->setFont(m_owner->font());
        text->setColor(m_owner->color());
        text->setStyleColor(m_owner->styleColor());
        text->setHAlign(static_cast<QQuickText::HAlignment>(m_owner->horizontalAlignment()));
        text->setVAlign(static_cast<QQuickText::VAlignment>(m_owner->verticalAlignment()));
        text->setElideMode(static_cast<QQuickText::TextElideMode>(m_owner->elide()));
        text->setWrapMode(static_cast<QQuickText::WrapMode>(m_owner->wrapMode()));
        text->setTextFormat(static_cast<QQuickText::TextFormat>(m_owner->textFormat()));
        text->setRenderType(static_cast<QQuickText::RenderType>(m_owner->renderType()));
        text->setLineHeight(m_owner->lineHeight());
        text->setLineHeightMode(static_cast<QQuickText::LineHeightMode>(m_owner->lineHeightMode()));
        text->setAntialiasing(m_owner->antialiasing());
        text->setText(m_owner->text());
    }
}

void TOutlineTextChildren::resize()
{
    const QSizeF size = m_owner->size();
    m_outline->setSize(size);
    if (m_fill)
        m_fill->setSize(size);
}

TOutlineTextQuickItem::TOutlineTextQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TOutlineTextQuickItem(parent)
    , m_children(this)
{
    initDefaults(m_horizontalAlignment, m_verticalAlignment, m_textFormat, m_lineHeight);
    m_children.sync();
}

void TOutlineTextQuickItem::syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other)
{
    if (other)
        copyProperties(*this, *other);
}

void TOutlineTextQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TOutlineTextQuickItem::geometryChange(newGeometry, oldGeometry);
    m_children.resize();
}

TCachedOutlineTextQuickItem::TCachedOutlineTextQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TCachedOutlineTextQuickItem(parent)
    , m_children(this)
{
    initDefaults(m_horizontalAlignment, m_verticalAlignment, m_textFormat, m_lineHeight);
    m_children.sync();
}

void TCachedOutlineTextQuickItem::setHighQuality(const bool highQuality)
{
    cip::tibia::qmlcomponents::TCachedOutlineTextQuickItem::setHighQuality(highQuality);
    m_children.sync();
}

void TCachedOutlineTextQuickItem::syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other)
{
    if (other)
        copyProperties(*this, *other);
}

void TCachedOutlineTextQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TCachedOutlineTextQuickItem::geometryChange(newGeometry, oldGeometry);
    m_children.resize();
}

}
