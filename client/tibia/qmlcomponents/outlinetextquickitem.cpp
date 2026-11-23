#include "outlinetextquickitem.h"

#include <QtQuick/private/qquicktext_p.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace tibia::qmlcomponents {

using Base = cip::tibia::qmlcomponents::TOutlineTextQuickItem;

namespace {

std::size_t hashMix(std::uint64_t x)
{
    constexpr std::uint64_t multiplier = 0xe9846af9b1a615dULL;
    x ^= x >> 32;
    x *= multiplier;
    x ^= x >> 32;
    x *= multiplier;
    x ^= x >> 28;
    return static_cast<std::size_t>(x);
}

void hashCombine(std::size_t& seed, const std::size_t value)
{
    seed = hashMix(seed + 0x9e3779b9 + value);
}

}

bool TOutlineTextQuickItem::TTextProperties::operator==(const TTextProperties& other) const
{
    return size == other.size && text == other.text && font == other.font && color == other.color
        && styleColor == other.styleColor && elide == other.elide && wrapMode == other.wrapMode
        && horizontalAlignment == other.horizontalAlignment && verticalAlignment == other.verticalAlignment
        && textFormat == other.textFormat && renderType == other.renderType && lineHeight == other.lineHeight
        && lineHeightMode == other.lineHeightMode && highQuality == other.highQuality
        && antialiasing == other.antialiasing;
}

std::size_t TOutlineTextQuickItem::TTextPropertiesHash::operator()(const TTextProperties& properties) const
{
    std::size_t seed = 0;
    hashCombine(seed, std::hash<std::string>()(properties.text.toStdString()));
    hashCombine(seed, static_cast<std::uint32_t>(properties.elide));
    hashCombine(seed, static_cast<std::uint32_t>(properties.wrapMode));
    std::size_t sizeSeed = 0;
    hashCombine(sizeSeed, static_cast<std::size_t>(static_cast<qint64>(properties.size.width())));
    hashCombine(sizeSeed, static_cast<std::size_t>(static_cast<qint64>(properties.size.height())));
    hashCombine(seed, sizeSeed);
    return seed;
}

TOutlineTextQuickItem::TOutlineTextQuickItem(QQuickItem* parent)
    : Base(parent)
{
    m_color = QColor(Qt::white);
    m_styleColor = QColor(Qt::black);
    m_elide = QQuickText::ElideNone;
    m_wrapMode = QQuickText::NoWrap;
    m_horizontalAlignment = QQuickText::AlignLeft;
    m_verticalAlignment = QQuickText::AlignTop;
    m_textFormat = QQuickText::AutoText;
    m_renderType = QQuickText::QtRendering;
    m_lineHeight = 1.0;
    m_lineHeightMode = QQuickText::ProportionalHeight;
    m_highQuality = false;

    connect(this, &QQuickItem::antialiasingChanged, this, &TOutlineTextQuickItem::onAntialiasingChanged);
    m_antialiasing = antialiasing();
}

void TOutlineTextQuickItem::createTextItems()
{
    if (!m_textItem) {
        m_textItem = new QQuickText(this);
        m_textItem->setParent(this);
        m_textItem->setParentItem(this);
        m_textItem->setStyle(QQuickText::Outline);
        connect(m_textItem, &QQuickText::truncatedChanged, this, &Base::truncatedChanged);
        connect(m_textItem, &QQuickItem::implicitWidthChanged, this, &TOutlineTextQuickItem::onImplicitSizeChanged);
        connect(m_textItem, &QQuickItem::implicitHeightChanged, this, &TOutlineTextQuickItem::onImplicitSizeChanged);
    }

    if (!m_highQuality) {
        if (m_highQualityTextItem) {
            m_highQualityTextItem->setParent(nullptr);
            m_highQualityTextItem->deleteLater();
            m_highQualityTextItem->setVisible(false);
            m_highQualityTextItem = nullptr;
        }
    } else if (!m_highQualityTextItem) {
        m_highQualityTextItem = new QQuickText(this);
        m_highQualityTextItem->setVisible(true);
        m_highQualityTextItem->setParent(m_textItem);
        m_highQualityTextItem->setParentItem(m_textItem);
        m_highQualityTextItem->setStyle(QQuickText::Normal);
    }
}

bool TOutlineTextQuickItem::syncTextItem(QQuickText* text, const bool isOutline)
{
    if (!text)
        return false;

    bool changed = false;
    if (text->size() != m_size) {
        text->setSize(m_size);
        changed = true;
    }
    if (text->text() != m_text) {
        text->setText(m_text);
        changed = true;
    }
    if (text->elideMode() != m_elide) {
        text->setElideMode(static_cast<QQuickText::TextElideMode>(m_elide));
        changed = true;
    }
    if (text->wrapMode() != m_wrapMode) {
        text->setWrapMode(static_cast<QQuickText::WrapMode>(m_wrapMode));
        changed = true;
    }
    if (text->font() != m_font) {
        text->setFont(m_font);
        changed = true;
    }
    if (text->hAlign() != m_horizontalAlignment) {
        text->setHAlign(static_cast<QQuickText::HAlignment>(m_horizontalAlignment));
        changed = true;
    }
    if (text->vAlign() != m_verticalAlignment) {
        text->setVAlign(static_cast<QQuickText::VAlignment>(m_verticalAlignment));
        changed = true;
    }
    if (text->textFormat() != m_textFormat) {
        text->setTextFormat(static_cast<QQuickText::TextFormat>(m_textFormat));
        changed = true;
    }
    if (text->renderType() != m_renderType) {
        text->setRenderType(static_cast<QQuickText::RenderType>(m_renderType));
        changed = true;
    }
    if (text->lineHeight() != m_lineHeight) {
        text->setLineHeight(m_lineHeight);
        changed = true;
    }
    if (text->lineHeightMode() != m_lineHeightMode) {
        text->setLineHeightMode(static_cast<QQuickText::LineHeightMode>(m_lineHeightMode));
        changed = true;
    }
    if (text->antialiasing() != m_antialiasing) {
        text->setAntialiasing(m_antialiasing);
        changed = true;
    }

    // In high quality the outline text draws its glyphs in the outline colour too;
    // the plain text on top of it draws the fill colour.
    const QColor color = m_highQuality && isOutline ? m_styleColor : m_color;
    if (text->color() != color) {
        text->setColor(color);
        changed = true;
    }
    if (text->styleColor() != m_styleColor) {
        text->setStyleColor(m_styleColor);
        changed = true;
    }
    return changed;
}

bool TOutlineTextQuickItem::syncTextItems()
{
    if (!m_textItem)
        return false;

    bool changed = syncTextItem(m_textItem, true);
    if (!m_highQualityTextItem)
        return changed;

    if (m_highQualityTextItem->size() != m_textItem->size())
        m_highQualityTextItem->setSize(m_textItem->size());
    changed |= syncTextItem(m_highQualityTextItem, false);
    return changed;
}

void TOutlineTextQuickItem::updateTextItems()
{
    createTextItems();
    if (syncTextItems())
        Q_EMIT textPropertiesChanged();
}

void TOutlineTextQuickItem::setHighQuality(const bool highQuality)
{
    if (m_highQuality == highQuality)
        return;
    m_highQuality = highQuality;
    updateTextItems();
    Q_EMIT highQualityChanged();
}

void TOutlineTextQuickItem::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    updateTextItems();
    Q_EMIT textChanged(m_text);
}

void TOutlineTextQuickItem::setFont(const QFont& font)
{
    if (font == m_font)
        return;
    m_font = font;
    updateTextItems();
    Q_EMIT fontChanged(m_font);
}

void TOutlineTextQuickItem::setColor(const QColor& color)
{
    if (color == m_color)
        return;
    m_color = color;
    updateTextItems();
    Q_EMIT colorChanged();
}

void TOutlineTextQuickItem::setHorizontalAlignment(const int alignment)
{
    if (m_horizontalAlignment == alignment)
        return;
    m_horizontalAlignment = alignment;
    updateTextItems();
    Q_EMIT horizontalAlignmentChanged(alignment);
}

void TOutlineTextQuickItem::setVerticalAlignment(const int alignment)
{
    if (m_verticalAlignment == alignment)
        return;
    m_verticalAlignment = alignment;
    updateTextItems();
    Q_EMIT verticalAlignmentChanged(alignment);
}

void TOutlineTextQuickItem::setStyleColor(const QColor& color)
{
    if (color == m_styleColor)
        return;
    m_styleColor = color;
    updateTextItems();
    Q_EMIT styleColorChanged();
}

void TOutlineTextQuickItem::setElide(const int mode)
{
    if (m_elide == mode)
        return;
    m_elide = mode;
    updateTextItems();
    Q_EMIT elideModeChanged(mode);
}

void TOutlineTextQuickItem::setWrapMode(const int mode)
{
    if (m_wrapMode == mode)
        return;
    m_wrapMode = mode;
    updateTextItems();
    Q_EMIT wrapModeChanged();
}

void TOutlineTextQuickItem::setTextFormat(const int textFormat)
{
    if (m_textFormat == textFormat)
        return;
    m_textFormat = textFormat;
    updateTextItems();
    Q_EMIT textFormatChanged(textFormat);
}

void TOutlineTextQuickItem::setRenderType(const int renderType)
{
    if (m_renderType == renderType)
        return;
    m_renderType = renderType;
    updateTextItems();
    Q_EMIT renderTypeChanged();
}

void TOutlineTextQuickItem::setLineHeight(const double lineHeight)
{
    if (m_lineHeight == lineHeight)
        return;
    m_lineHeight = lineHeight;
    updateTextItems();
    Q_EMIT lineHeightChanged(lineHeight);
}

void TOutlineTextQuickItem::setLineHeightMode(const int mode)
{
    if (m_lineHeightMode == mode)
        return;
    m_lineHeightMode = mode;
    updateTextItems();
    Q_EMIT lineHeightModeChanged(mode);
}

bool TOutlineTextQuickItem::truncated() const
{
    return m_textItem && m_textItem->truncated();
}

void TOutlineTextQuickItem::onAntialiasingChanged(const bool antialiasing)
{
    m_antialiasing = antialiasing;
    updateTextItems();
}

void TOutlineTextQuickItem::onImplicitSizeChanged()
{
    if (!m_textItem)
        return;
    if (implicitWidth() != m_textItem->implicitWidth())
        setImplicitWidth(m_textItem->implicitWidth());
    if (implicitHeight() != m_textItem->implicitHeight())
        setImplicitHeight(m_textItem->implicitHeight());
}

void TOutlineTextQuickItem::syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other)
{
    const auto* source = qobject_cast<const TOutlineTextQuickItem*>(other);
    if (!source)
        return;

    // The properties are copied as a block, without their change signals.
    m_size = source->m_size;
    m_text = source->m_text;
    m_font = source->m_font;
    m_color = source->m_color;
    m_styleColor = source->m_styleColor;
    m_elide = source->m_elide;
    m_wrapMode = source->m_wrapMode;
    m_horizontalAlignment = source->m_horizontalAlignment;
    m_verticalAlignment = source->m_verticalAlignment;
    m_textFormat = source->m_textFormat;
    m_renderType = source->m_renderType;
    m_lineHeight = source->m_lineHeight;
    m_lineHeightMode = source->m_lineHeightMode;
    m_highQuality = source->m_highQuality;
    m_antialiasing = source->m_antialiasing;

    updateTextItems();
    setSize(m_size);
}

TOutlineTextQuickItem::TTextProperties TOutlineTextQuickItem::textProperties() const
{
    TTextProperties properties;
    properties.size = m_size;
    properties.text = m_text;
    properties.font = m_font;
    properties.color = m_color;
    properties.styleColor = m_styleColor;
    properties.elide = m_elide;
    properties.wrapMode = m_wrapMode;
    properties.horizontalAlignment = m_horizontalAlignment;
    properties.verticalAlignment = m_verticalAlignment;
    properties.textFormat = m_textFormat;
    properties.renderType = m_renderType;
    properties.lineHeight = m_lineHeight;
    properties.lineHeightMode = m_lineHeightMode;
    properties.highQuality = m_highQuality;
    properties.antialiasing = m_antialiasing;
    return properties;
}

void TOutlineTextQuickItem::setTextProperties(TTextProperties properties)
{
    m_size = properties.size;
    m_text = std::move(properties.text);
    m_font = std::move(properties.font);
    m_color = properties.color;
    m_styleColor = properties.styleColor;
    m_elide = properties.elide;
    m_wrapMode = properties.wrapMode;
    m_horizontalAlignment = properties.horizontalAlignment;
    m_verticalAlignment = properties.verticalAlignment;
    m_textFormat = properties.textFormat;
    m_renderType = properties.renderType;
    m_lineHeight = properties.lineHeight;
    m_lineHeightMode = properties.lineHeightMode;
    m_highQuality = properties.highQuality;
    m_antialiasing = properties.antialiasing;

    updateTextItems();
    // The requested size, not m_size: while the item has no explicit size yet,
    // the new text's implicit size resizes it and geometryChange overwrites m_size.
    setSize(properties.size);
}

void TOutlineTextQuickItem::componentComplete()
{
    Base::componentComplete();
    updateTextItems();
}

void TOutlineTextQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    Base::geometryChange(newGeometry, oldGeometry);
    m_size = size();
    updateTextItems();
}

}
