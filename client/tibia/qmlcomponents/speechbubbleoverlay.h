#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QPointer>
#include <QQmlComponent>

#include <vector>

namespace tibia::chat {
class TTextStorage;
}

namespace tibia::qmlcomponents {

class TWorldMapRenderProvider;

/// One SpeechBubbleTemplate.qml: its text, colours and place (CIP:
/// qmlcomponents::TSpeechBubbleController). QML writes the laid-out size back
/// through calculatedSize.
class TSpeechBubbleController : public cip::tibia::qmlcomponents::TSpeechBubbleController
{
    Q_OBJECT
public:
    TSpeechBubbleController(const QColor& textColor, const QColor& styleColor, int maximumWidth, QObject* parent = nullptr);

    /// Rich text unless `isHtml`; line breaks kept (CIP: FUN_1010d4860).
    void setText(bool visible, const QString& text, const QColor& color, bool isHtml);
    bool isHidden() const { return m_hidden; }
    void setHidden(bool hidden) { m_hidden = hidden; }
    void setVisible(bool visible);
    void setPosition(const QPoint& position);

    /// Where the layout wants the bubble before overlaps are resolved.
    QPoint desiredPosition;
    QPointer<QQuickItem> item;

private:
    bool m_hidden = true;
};

/// Speech bubbles and the status, look, centre and private message lines over
/// the map (CIP: qmlcomponents::TSpeechBubbleOverlayQuickItem).
class TSpeechBubbleOverlayQuickItem : public cip::tibia::qmlcomponents::TSpeechBubbleOverlayQuickItem
{
    Q_OBJECT
public:
    explicit TSpeechBubbleOverlayQuickItem(QQuickItem* parent = nullptr);
    ~TSpeechBubbleOverlayQuickItem() override;

    /// Shows `texts` over `provider`'s world, refreshed every frame.
    void setSources(TWorldMapRenderProvider* provider, const chat::TTextStorage* texts);

    /// Extra bottom inset for the status line (product automation / FABs band).
    void setStatusBottomMargin(int margin);
    int statusBottomMargin() const { return m_statusBottomMargin; }

public Q_SLOTS:
    void triggerUpdate() override;

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    enum Fixed { Status, Look, Center, Private, FixedCount };

    void setup();
    TSpeechBubbleController* createController();
    void updateNpcBubbles();
    void updateCreatureBubbles();
    void layout();
    QPointF anchorOf(const Position& field) const;
    void placeAbove(TSpeechBubbleController& controller, const Position& field) const;

    QPointer<TWorldMapRenderProvider> m_provider;
    const chat::TTextStorage* m_texts = nullptr;
    QMetaObject::Connection m_frameConnection;
    QQmlComponent* m_component = nullptr;
    /// CIP's overlay+0x20 and +0x24.
    int m_margin = 3;
    int m_statusBottomMargin = 0;
    int m_maximumWidth = 288;
    TSpeechBubbleController* m_fixed[FixedCount] = {};
    std::vector<TSpeechBubbleController*> m_npcBubbles;
    std::vector<TSpeechBubbleController*> m_creatureBubbles;
    std::vector<TSpeechBubbleController*> m_all;
};

}
