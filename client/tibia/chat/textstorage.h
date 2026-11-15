#pragma once

#include "shared/cip_typed.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>
#include <QString>

#include <deque>
#include <memory>
#include <vector>

namespace tibia {
namespace creatures {
class TCreature;
}
namespace chat {

using Millis = qint64;

/// How long a text stays on the map (CIP: , ,).
Millis textDuration(const QString& text);
Millis privateTextDuration(const QString& text);
Millis npcTextDuration(const QString& text);

/// Texts shown one after the other, each for its own time (CIP: chat::TFleetingTextQueue).
class TFleetingTextQueue : public ::tibia::CipTyped
{
public:
    virtual ~TFleetingTextQueue() = default;

    virtual void push(const QString& text, Millis duration, const QColor& color);
    /// Shows the next text once the current one has run out (CIP:).
    void update(Millis now);

    bool isVisible() const { return m_visible; }
    const QString& text() const { return m_text; }
    const QColor& color() const { return m_color; }
    Millis start() const { return m_start; }

protected:
    /// Clears the shown text of a queue it owns (hotkey ClearOldestMessage).
    friend class TTextStorage;

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
    TNpcSpeechBubble() = default;

    /// Answers only come in through addText.
    void push(const QString& text, Millis duration, const QColor& color) override;
    /// Starts the bubble over for `name`: drops the waiting and shown answers.
    void setHeader(const QString& name, const QColor& color, const Position& coordinate);
    /// Queues an answer of `name`: tags escaped, keywords in {braces} highlighted.
    void addText(const QString& name, const QString& text, Millis duration);
    /// "Name:" header, a line break and the shown answer.
    QString bubbleText() const;

    const QString& name() const { return m_name; }
    const QColor& bubbleColor() const { return m_bubbleColor; }
    const Position& coordinate() const { return m_coordinate; }

private:
    QString m_name;
    QString m_header;
    QColor m_bubbleColor = Qt::transparent;
    Position m_coordinate;
};

/// A creature's recent lines at one place, said the same way (CIP:
/// chat::TPersonalizedSpeechBubble).
class TCreatureSpeechBubble : public ::tibia::CipTyped
{
public:
    /// CIP's speech volume.
    enum Volume { Whisper = 0, Say = 1, Yell = 2, Monster = 3, Cast = 4 };

    TCreatureSpeechBubble(const QString& speaker, int volume, const QColor& color, const Position& coordinate, size_t maxLines);

    bool matches(const QString& speaker, int volume, const QColor& color, const Position& coordinate) const;
    /// Yells are upper-cased; past maxLines the oldest line goes.
    void addLine(const QString& text, Millis now, Millis duration);
    /// Drops the leading lines that ran out.
    void update(Millis now);
    /// Blanks the oldest line; the next update drops it.
    void clearFirstLine();
    bool isEmpty() const { return m_lines.empty(); }
    /// When the oldest line was said.
    Millis firstLineStart() const { return m_lines.front().start; }
    /// The header and the lines, one per row.
    QString text() const;
    const QColor& color() const { return m_color; }
    const Position& coordinate() const { return m_coordinate; }
    Millis expiry() const { return m_expiry; }

private:
    struct Line
    {
        QString text;
        Millis expiry = 0;
        Millis start = 0;
        bool visible = false;
        QColor color = Qt::white;
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
    explicit TTextStorage(size_t maxCreatureBubbles = 10, size_t maxQueuedMessages = 9);

    static Millis now();

    /// Ages every text. NPC bubbles showing an answer close once the own
    /// character is more than 3 fields away.
    void update(Millis now);
    /// Leaving the world: drops everything.
    void clear();

    size_t maxCreatureBubbles() const { return m_maxCreatureBubbles; }

    /// The bottom status line, shown 5 s.
    void setStatusMessage(const QString& text, const QColor& color, bool isHtml = false);
    bool isStatusVisible() const { return m_statusVisible; }
    const QString& statusText() const { return m_statusText; }
    const QColor& statusColor() const { return m_statusColor; }
    bool isStatusHtml() const { return m_statusHtml; }

    /// Hotkey ClearOldestMessage: blanks the oldest of the look line, the shown
    /// centre and private messages, the NPC answers and the creature lines.
    void clearOldestMessage();

    /// The look line above the centre; `isHtml` for loot.
    void setLookMessage(const QString& text, const QColor& color, bool isHtml);
    bool isLookVisible() const { return m_lookVisible; }
    const QString& lookText() const { return m_lookText; }
    const QColor& lookColor() const { return m_lookColor; }
    bool isLookHtml() const { return m_lookHtml; }

    /// Centre messages and private messages, "Name:\ntext" for a speaker.
    void addCenterMessage(const QString& text, const QColor& color);
    void addGamemasterMessage(const QString& name, const QString& text, const QColor& color);
    void addPrivateMessage(const QString& name, const QString& text, const QColor& color);
    const TTextMessageQueue& centerMessages() const { return m_center; }
    const TTextMessageQueue& privateMessages() const { return m_private; }

    /// A creature's speech; volume per TCreatureSpeechBubble::Volume.
    void addCreatureSpeech(const QString& speaker, int volume, const QString& text, const QColor& color, const Position& coordinate);
    bool hasCreatureBubbles() const { return !m_creatureBubbles.empty(); }
    size_t creatureBubbleCount() const { return m_creatureBubbles.size(); }
    std::shared_ptr<TCreatureSpeechBubble> creatureBubble(size_t index) const { return m_creatureBubbles.at(index); }
    const std::vector<std::shared_ptr<TCreatureSpeechBubble>>& creatureBubbles() const { return m_creatureBubbles; }

    /// An NPC's bubble (brought to the front, emptied) and its answers.
    void setNpcSpeechHeader(const QString& name, const QColor& color, const Position& coordinate);
    void addNpcSpeech(const QString& name, const QString& text);
    bool hasVisibleNpcBubble() const;
    size_t npcBubbleCount() const { return m_npcBubbles.size(); }
    const TNpcSpeechBubble& npcBubble(size_t index) const { return m_npcBubbles.at(index); }
    const std::deque<TNpcSpeechBubble>& npcBubbles() const { return m_npcBubbles; }

    /// Remembers the own character for the NPC bubble range.
    void onPlayerCreatureAddedToGameSession(std::weak_ptr<::tibia::creatures::TCreature> pPlayer);

private:
    /// No own character yet counts as in range.
    bool isInNpcBubbleRange(const Position& coordinate) const;

    std::weak_ptr<::tibia::creatures::TCreature> m_player;
    size_t m_maxCreatureBubbles;
    size_t m_maxQueuedMessages;
    QString m_statusText;
    Millis m_statusExpiry = 0;
    Millis m_statusStart = 0;
    bool m_statusVisible = false;
    QColor m_statusColor = Qt::white;
    bool m_statusHtml = false;
    QString m_lookText;
    Millis m_lookExpiry = 0;
    Millis m_lookStart = 0;
    bool m_lookVisible = false;
    QColor m_lookColor = Qt::white;
    bool m_lookHtml = false;
    TTextMessageQueue m_center;
    TTextMessageQueue m_private;
    std::deque<TNpcSpeechBubble> m_npcBubbles;
    std::vector<std::shared_ptr<TCreatureSpeechBubble>> m_creatureBubbles;
};

}
}
