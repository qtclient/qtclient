#pragma once

#include <QColor>
#include <QFont>
#include <QSizeF>
#include <QString>

#include <cip_gen.h>

#include <cstddef>

class QQuickText;

namespace tibia {
namespace qmlcomponents {

/// QML OutlineText: a text drawn with an outline. The item keeps the text
/// properties itself and pushes them into a child Qt Quick Text with the outline
/// style. In high quality a second, plain text sits on top of it (a child of the
/// outline text): the outline text then draws its glyphs in the outline colour and
/// the plain text draws the fill.
class TOutlineTextQuickItem : public cip::tibia::qmlcomponents::TOutlineTextQuickItem
{
    Q_OBJECT
public:
    /// Everything that decides how the text looks, copied as one block: the key
    /// of the outline text texture cache and the request of an offscreen image.
    /// The defaults are the item's own.
    struct TTextProperties
    {
        QSizeF size;
        QString text;
        QFont font;
        QColor color = QColor(Qt::white);
        QColor styleColor = QColor(Qt::black);
        int elide = 3;               // QQuickText::ElideNone
        int wrapMode = 0;            // QQuickText::NoWrap
        int horizontalAlignment = 1; // QQuickText::AlignLeft
        int verticalAlignment = 0x20; // QQuickText::AlignTop
        int textFormat = 2;          // QQuickText::AutoText
        int renderType = 0;          // QQuickText::QtRendering
        double lineHeight = 1.0;
        int lineHeightMode = 0;      // QQuickText::ProportionalHeight
        bool highQuality = false;
        bool antialiasing = false;

        /// The sizes compare fuzzily (as QSizeF does), the line height exactly,
        /// everything else by value.
        bool operator==(const TTextProperties& other) const;
        bool operator!=(const TTextProperties& other) const { return !(*this == other); }
    };
    /// Hash of the text, the elide and wrap modes and the size truncated to
    /// whole pixels, combined as boost::hash_combine does.
    struct TTextPropertiesHash
    {
        std::size_t operator()(const TTextProperties& properties) const;
    };

    explicit TOutlineTextQuickItem(QQuickItem* parent = nullptr);

    /// The text properties and the size as one block.
    TTextProperties textProperties() const;
    /// Takes over the whole block without the property signals, updates the texts
    /// (textPropertiesChanged when they changed) and resizes the item to the
    /// block's size.
    void setTextProperties(TTextProperties properties);

    void setHighQuality(bool highQuality) override;
    void setText(const QString& text) override;
    void setFont(const QFont& font) override;
    void setColor(const QColor& color) override;
    void setHorizontalAlignment(int alignment) override;
    void setVerticalAlignment(int alignment) override;
    void setStyleColor(const QColor& color) override;
    void setElide(int mode) override;
    void setWrapMode(int mode) override;
    void setTextFormat(int textFormat) override;
    void setRenderType(int renderType) override;
    void setLineHeight(double lineHeight) override;
    void setLineHeightMode(int mode) override;
    bool truncated() const override;

    /// Copies all text properties and the size of another outline text.
    void syncFromOther(cip::tibia::qmlcomponents::TOutlineTextQuickItem* other) override;

public Q_SLOTS:
    void onAntialiasingChanged(bool antialiasing);
    void onImplicitSizeChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    /// The outline text (null until the first update).
    QQuickText* textItem() const { return m_textItem; }
    /// The size the texts were last laid out for (part of the text properties).
    const QSizeF& textSize() const { return m_size; }

    /// Creates the texts when missing, pushes the properties into them and emits
    /// textPropertiesChanged when one of them changed.
    void updateTextItems();

private:
    /// Creates the outline text, and creates or drops the high-quality fill text.
    void createTextItems();
    /// Pushes the properties into both texts; true when anything changed.
    bool syncTextItems();
    /// Pushes the properties into one text; isOutline selects the colours.
    bool syncTextItem(QQuickText* text, bool isOutline);

    QSizeF m_size;
    bool m_antialiasing = false;
    QQuickText* m_textItem = nullptr;
    QQuickText* m_highQualityTextItem = nullptr;
};

}
}
