#pragma once

#include "worldmap/coordinates.h"

#include <QColor>
#include <QString>

#include <memory>
#include <vector>

namespace tibia::chat {

class TTextStorage;

/// One text drawn over the world map: one of the four on-screen lines, or a
/// speech bubble anchored at a subfield of the map.
struct TTextOverlay
{
    enum EType { Status = 0, Look = 1, Center = 2, Private = 3, World = 4 };

    TTextOverlay() = default;
    /// A hidden overlay of `type` with no text, a transparent colour and the
    /// zero coordinate.
    explicit TTextOverlay(int type);

    int type = Status;
    bool visible = false;
    QString text;
    QColor color = QColor(Qt::transparent);
    worldmap::TWorldMapSubfieldCoordinate coordinate { 0, 0, 0 };
    /// The text is rich text.
    bool isHtml = false;
};

/// What the speech bubble overlay draws, read from the session's text storage:
/// the status, look, centre and private message lines (only the ones that
/// changed since the last call) and the NPC and creature speech bubbles.
class TTextOverlayRenderProvider
{
public:
    explicit TTextOverlayRenderProvider(std::shared_ptr<TTextStorage> textStorage);

    /// The status line at the bottom of the map.
    TTextOverlay statusOverlay() const;
    /// The look line.
    TTextOverlay lookOverlay() const;
    /// The current centre message.
    TTextOverlay centerOverlay() const;
    /// The current private message.
    TTextOverlay privateOverlay() const;
    /// The four on-screen lines that differ (type, visibility, text, colour or
    /// coordinate) from the ones the previous call returned; keeps them.
    std::vector<TTextOverlay> changedScreenOverlays();
    /// The visible NPC speech bubbles, half a field right of their field's
    /// top-left corner.
    std::vector<TTextOverlay> npcOverlays() const;
    /// The creature speech bubbles that have lines, half a field right of
    /// their field's top-left corner.
    std::vector<TTextOverlay> creatureOverlays() const;

private:
    std::shared_ptr<TTextStorage> m_textStorage;
    /// The on-screen lines changedScreenOverlays returned last, by type.
    TTextOverlay m_screenOverlays[4] = { TTextOverlay(TTextOverlay::Status), TTextOverlay(TTextOverlay::Look),
                                         TTextOverlay(TTextOverlay::Center), TTextOverlay(TTextOverlay::Private) };
    /// The world overlay slot; none of the provider's functions reads it.
    TTextOverlay m_worldOverlay = TTextOverlay(TTextOverlay::World);
};

}
