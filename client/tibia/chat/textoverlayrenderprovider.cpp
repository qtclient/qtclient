#include "chat/textoverlayrenderprovider.h"

#include "chat/textstorage.h"

namespace tibia::chat {

namespace {

/// Where a world text hangs: half a field right of its field's top-left corner.
worldmap::TWorldMapSubfieldCoordinate worldTextAnchor(const Position& field)
{
    return worldmap::subfieldOfField(worldmap::TWorldMapCoordinate(field), worldmap::TWorldMapSubfieldExtent(16, 0, 0));
}

/// Builds an on-screen overlay (its coordinate is the zero coordinate).
TTextOverlay screenOverlay(const int type, const bool visible, const QString& text, const QColor& color, const bool isHtml)
{
    TTextOverlay overlay(type);
    overlay.visible = visible;
    overlay.text = text;
    overlay.color = color;
    overlay.isHtml = isHtml;
    return overlay;
}

}

TTextOverlay::TTextOverlay(const int type_)
    : type(type_)
{
}

TTextOverlayRenderProvider::TTextOverlayRenderProvider(std::shared_ptr<TTextStorage> textStorage)
    : m_textStorage(std::move(textStorage))
{
}

TTextOverlay TTextOverlayRenderProvider::statusOverlay() const
{
    return screenOverlay(TTextOverlay::Status, m_textStorage->isStatusVisible(), m_textStorage->statusText(),
                         m_textStorage->statusColor(), m_textStorage->isStatusHtml());
}

TTextOverlay TTextOverlayRenderProvider::lookOverlay() const
{
    return screenOverlay(TTextOverlay::Look, m_textStorage->isLookVisible(), m_textStorage->lookText(), m_textStorage->lookColor(),
                         m_textStorage->isLookHtml());
}

TTextOverlay TTextOverlayRenderProvider::centerOverlay() const
{
    const TTextMessageQueue& messages = m_textStorage->centerMessages();
    return screenOverlay(TTextOverlay::Center, messages.isVisible(), messages.text(), messages.color(), false);
}

TTextOverlay TTextOverlayRenderProvider::privateOverlay() const
{
    const TTextMessageQueue& messages = m_textStorage->privateMessages();
    return screenOverlay(TTextOverlay::Private, messages.isVisible(), messages.text(), messages.color(), false);
}

std::vector<TTextOverlay> TTextOverlayRenderProvider::changedScreenOverlays()
{
    std::vector<TTextOverlay> changed;
    const TTextOverlay current[4] = { statusOverlay(), lookOverlay(), centerOverlay(), privateOverlay() };
    for (int i = 0; i < 4; ++i) {
        TTextOverlay& kept = m_screenOverlays[i];
        const TTextOverlay& overlay = current[i];
        // The rich text flag alone does not count as a change.
        if (overlay.type == kept.type && overlay.visible == kept.visible && overlay.text == kept.text && overlay.color == kept.color
            && overlay.coordinate == kept.coordinate)
            continue;
        kept = overlay;
        changed.push_back(overlay);
    }
    return changed;
}

std::vector<TTextOverlay> TTextOverlayRenderProvider::npcOverlays() const
{
    std::vector<TTextOverlay> overlays;
    if (!m_textStorage->hasVisibleNpcBubble())
        return overlays;
    for (size_t i = 0, count = m_textStorage->npcBubbleCount(); i < count; ++i) {
        const TNpcSpeechBubble& bubble = m_textStorage->npcBubble(i);
        if (!bubble.isVisible())
            continue;
        TTextOverlay overlay(TTextOverlay::World);
        overlay.visible = bubble.isVisible();
        overlay.text = bubble.bubbleText();
        overlay.color = bubble.color();
        overlay.coordinate = worldTextAnchor(bubble.coordinate());
        overlay.isHtml = true;
        overlays.push_back(overlay);
    }
    return overlays;
}

std::vector<TTextOverlay> TTextOverlayRenderProvider::creatureOverlays() const
{
    std::vector<TTextOverlay> overlays;
    if (!m_textStorage->hasCreatureBubbles())
        return overlays;
    for (size_t i = 0, count = m_textStorage->creatureBubbleCount(); i < count; ++i) {
        const std::shared_ptr<TCreatureSpeechBubble> bubble = m_textStorage->creatureBubble(i);
        if (bubble->isEmpty())
            continue;
        TTextOverlay overlay(TTextOverlay::World);
        overlay.visible = !bubble->isEmpty();
        overlay.text = bubble->text();
        overlay.color = bubble->color();
        overlay.coordinate = worldTextAnchor(bubble->coordinate());
        overlay.isHtml = false;
        overlays.push_back(overlay);
    }
    return overlays;
}

}
