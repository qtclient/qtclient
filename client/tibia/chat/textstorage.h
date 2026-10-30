#pragma once

#include "shared/cip_typed.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>
#include <QString>

#include <deque>
#include <memory>
#include <vector>

namespace tibia::chat {

using Millis = qint64;

/// How long a text stays on the map (CIP: FUN_1010198f0, FUN_101019910, FUN_101019930).
Millis textDuration(const QString& text);
Millis privateTextDuration(const QString& text);
Millis npcTextDuration(const QString& text);

/// Texts shown one after the other, each for its own time (CIP: chat::TFleetingTextQueue).
class TFleetingTextQueue : public ::tibia::CipTyped
{
public:
    virtual ~TFleetingTextQueue() = default;

    virtual void push(const QString& text, Millis duration, const QColor& color);
    /// Shows the next text once the current one has run out (CIP: FUN_10099d340).
    void update(Millis now);
    /// Drop the shown text immediately and promote the next waiting one.
    void dismiss(Millis now);

    bool isVisible() const { return m_visible; }
    const QString& text() const { return m_text; }
    const QColor& color() const { return m_color; }
    Millis start() const { return m_start; }

protected:
    struct Entry
    {
        QString text;
        Millis duration = 0;
        QColor color;
    };

    std::deque<Entry> m_queue;
    QString m_text;
    Millis m_expiry = 0;
    Millis m_start = 0;
    bool m_visible = false;
    QColor m_color = Qt::white;
};

/// A fleeting queue of at most `max` waiting texts (CIP: chat::TTextMessageQueue).
class TTextMessageQueue : public TFleetingTextQueue
{
public:
    explicit TTextMessageQueue(size_t max) : m_max(max) {}

    /// Drops the oldest waiting text when full; a text arriving while none is
    /// shown stays twice as long.
    void push(const QString& text, Millis duration, const QColor& color) override;

private:
    size_t m_max;
};

/// An NPC's answers over its head, one at a time (CIP: chat::TNpcSpeechBubble).
class TNpcSpeechBubble : public TFleetingTextQueue
{
public:
    TNpcSpeechBubble(const QString& name, const QColor& color, const Position& coordinate);

    const QString& name() const { return m_name; }
    const QColor& bubbleColor() const { return m_bubbleColor; }
    const Position& coordinate() const { return m_coordinate; }
    void setHeader(const QColor& color, const Position& coordinate);
    /// Rich text: keywords in {braces} highlighted (CIP: FUN_10099dd90).
    void pushText(const QString& text, Millis duration);
    /// "Name:\n" and the shown answer (CIP: TNpcSpeechBubble::v3).
    QString bubbleText() const;

private:
    QString m_name;
    QString m_header;
    QColor m_bubbleColor;
    Position m_coordinate;
};

/// A creature's recent lines at one place, said the same way (CIP's 0xb8-byte
/// speech bubble, FUN_10099e260).
class TCreatureSpeechBubble : public ::tibia::CipTyped
{
public:
    /// CIP's speech volume (FUN_10099b410).
    enum Volume { Whisper = 0, Say = 1, Yell = 2, Monster = 3, Cast = 4 };

    TCreatureSpeechBubble(const QString& speaker, int volume, const QColor& color, const Position& coordinate, size_t maxLines);

    bool matches(const QString& speaker, int volume, const QColor& color, const Position& coordinate) const;
    /// Yells are upper-cased; past maxLines the oldest line goes (CIP: FUN_10099e6b0).
    void addLine(const QString& text, Millis now, Millis duration);
    /// Drops the leading lines that ran out (CIP: FUN_10099e500).
    void update(Millis now);
    bool isEmpty() const { return m_lines.empty(); }
    /// The header and the lines, one per row (CIP: FUN_10099ebf0).
    QString text() const;
    const QColor& color() const { return m_color; }
    const Position& coordinate() const { return m_coordinate; }
    Millis expiry() const { return m_expiry; }

private:
    struct Line
    {
        QString text;
        Millis expiry = 0;
        bool visible = true;
    };

    QString m_speaker;
    int m_volume;
    QColor m_color;
    Position m_coordinate;
    size_t m_maxLines;
    QString m_header;
    std::deque<Line> m_lines;
    Millis m_expiry = 0;
};

/// Everything written over the map (CIP: chat::TTextStorage): the status line,
/// the look line, the centre and private message queues, and the speech bubbles.
class TTextStorage : public ::tibia::CipTyped
{
public:
    /// CIP builds it with 10 creature bubbles and 9 queued messages (FUN_100b31e90).
    explicit TTextStorage(size_t maxCreatureBubbles = 10, size_t maxQueuedMessages = 9);

    static Millis now();

    /// Ages every text (CIP: v12, slot 0x60). `player` is the own character's
    /// field; NPC bubbles farther than 3 fields away close.
    void update(Millis now, const Position& player);
    void clear();

    /// The bottom status line, shown 5 s (CIP: slot 0x80).
    void setStatusMessage(const QString& text, const QColor& color);
    /// Hotkey ClearOldestMessage: dismiss status, else look, else the centre queue.
    void clearOldestMessage();
    bool isStatusVisible() const { return m_statusVisible; }
    const QString& statusText() const { return m_statusText; }
    const QColor& statusColor() const { return m_statusColor; }

    /// The look line above the centre (CIP: slot 0xa8); `isHtml` for loot.
    void setLookMessage(const QString& text, const QColor& color, bool isHtml);
    bool isLookVisible() const { return m_lookVisible; }
    const QString& lookText() const { return m_lookText; }
    const QColor& lookColor() const { return m_lookColor; }
    bool isLookHtml() const { return m_lookHtml; }

    /// Centre messages (CIP: slots 0xd0, 0xd8) and private messages (slot 0xf8).
    void addCenterMessage(const QString& text, const QColor& color);
    void addGamemasterMessage(const QString& name, const QString& text, const QColor& color);
    void addPrivateMessage(const QString& name, const QString& text, const QColor& color);
    const TTextMessageQueue& centerMessages() const { return m_center; }
    const TTextMessageQueue& privateMessages() const { return m_private; }

    /// A creature's speech (CIP: slot 0x118); volume per TCreatureSpeechBubble::Volume.
    void addCreatureSpeech(const QString& speaker, int volume, const QString& text, const QColor& color, const Position& coordinate);
    const std::vector<std::shared_ptr<TCreatureSpeechBubble>>& creatureBubbles() const { return m_creatureBubbles; }

    /// An NPC's bubble (CIP: slot 0x138) and its answers (slot 0x140).
    void setNpcSpeechHeader(const QString& name, const QColor& color, const Position& coordinate);
    void addNpcSpeech(const QString& name, const QString& text);
    const std::deque<TNpcSpeechBubble>& npcBubbles() const { return m_npcBubbles; }

private:
    size_t m_maxCreatureBubbles;
    size_t m_maxQueuedMessages;
    QString m_statusText;
    Millis m_statusExpiry = 0;
    bool m_statusVisible = false;
    QColor m_statusColor = Qt::white;
    QString m_lookText;
    Millis m_lookExpiry = 0;
    bool m_lookVisible = false;
    QColor m_lookColor = Qt::white;
    bool m_lookHtml = false;
    TTextMessageQueue m_center;
    TTextMessageQueue m_private;
    std::deque<TNpcSpeechBubble> m_npcBubbles;
    std::vector<std::shared_ptr<TCreatureSpeechBubble>> m_creatureBubbles;
};

}
