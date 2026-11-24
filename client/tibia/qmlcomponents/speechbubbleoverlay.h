#pragma once

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QPointer>
#include <QQmlComponent>

#include <memory>
#include <vector>

namespace tibia {
namespace chat {
class TTextOverlayRenderProvider;
struct TTextOverlay;
}
namespace game {
class IGameSession;
}
namespace renderer {
class TWorldMapCamera;
}
namespace worldmap {
class TWorldMapSubfieldCoordinate;
}
}

namespace tibia {
namespace qmlcomponents {

class TWorldMapRenderProvider;

/// One SpeechBubbleTemplate.qml: its text, colours and place (CIP:
/// qmlcomponents::TSpeechBubbleController). QML writes the laid-out size back
/// through calculatedSize.
class TSpeechBubbleController : public cip::tibia::qmlcomponents::TSpeechBubbleController
{
    Q_OBJECT
public:
    TSpeechBubbleController(const QColor& textColor, const QColor& styleColor, int maximumWidth, QObject* parent = nullptr);

    /// Rich text unless `isHtml`; line breaks kept (CIP:).
    void setText(bool visible, const QString& text, const QColor& color, bool isHtml);
    bool isHidden() const { return m_hidden; }
    void setHidden(bool hidden) { m_hidden = hidden; }
    void setVisible(bool visible);
    void setPosition(const QPoint& position);

    /// CIP: whether bubbles show their bounding rectangle ([DEBUG]
    /// showSpeechBubbleBoundingRect, set once by the game client).
    static void setShowBoundingRect(bool Show);
    bool showBoundingRect() const override;

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

    /// The session's text overlay render provider and world map camera (both
    /// unset without a session); the overlay is enabled when both are set.
    /// Host: the session's render provider supplies the frame and pixels and
    /// refreshes the bubbles every frame.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

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
    /// The on-screen lines that changed since the last frame.
    void updateScreenOverlays();
    void updateNpcBubbles();
    void updateCreatureBubbles();
    /// Shows `overlays` in `controllers` (made as needed), each above its
    /// anchor; the rest are hidden.
    void updateWorldBubbles(std::vector<TSpeechBubbleController*>& controllers, const std::vector<chat::TTextOverlay>& overlays);
    void layout();
    QPointF anchorOf(const worldmap::TWorldMapSubfieldCoordinate& anchor) const;
    void placeAbove(TSpeechBubbleController& controller, const worldmap::TWorldMapSubfieldCoordinate& anchor) const;

    bool m_enabled = false;
    std::shared_ptr<chat::TTextOverlayRenderProvider> m_textOverlayRenderProvider;
    std::shared_ptr<renderer::TWorldMapCamera> m_worldMapCamera;
    QPointer<TWorldMapRenderProvider> m_provider;
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
}
