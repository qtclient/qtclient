#include "textstorage.h"

#include <framework/core/clock.h>

#include <algorithm>
#include <cstdlib>

namespace tibia::chat {

namespace {

constexpr Millis StatusDuration = 5000;
/// NPC bubbles close once the player is farther than this (DAT_102734bc8).
constexpr int NpcBubbleRange = 3;
constexpr size_t MaxNpcBubbles = 10;
constexpr size_t MaxCreatureBubbleLines = 10;

/// NPC keywords (FUN_1009911a0).
const QColor NpcHighlightColor(32, 160, 255);

QString volumeHeader(const int volume)
{
    switch (volume) {
        case TCreatureSpeechBubble::Whisper: return qtTrId("speech_bubble_volume_whisper");
        case TCreatureSpeechBubble::Say: return qtTrId("speech_bubble_volume_say");
        case TCreatureSpeechBubble::Yell: return qtTrId("speech_bubble_volume_yell");
        case TCreatureSpeechBubble::Cast: return qtTrId("speech_bubble_volume_cast");
        default: return {};
    }
}

/// FUN_10099c020 then FUN_10099bb40: escape tags, colour {keywords}.
QString npcRichText(QString text)
{
    text.replace(u'<', QLatin1String("&lt;")).replace(u'>', QLatin1String("&gt;"));
    text.replace(u'{', QStringLiteral("<font color=\"%1\">").arg(NpcHighlightColor.name()));
    text.replace(u'}', QLatin1String("</font>"));
    return text;
}

}

Millis textDuration(const QString& text) { return text.size() * 50 + 3000; }
Millis privateTextDuration(const QString& text) { return text.size() * 25 + 1500; }
Millis npcTextDuration(const QString& text) { return (text.size() * 4000 >> 8) + 4000; }

void TFleetingTextQueue::push(const QString& text, const Millis duration, const QColor& color)
{
    m_queue.push_back({ text, duration, color });
}

void TFleetingTextQueue::update(const Millis now)
{
    m_visible = now < m_expiry;
    if (m_visible || m_queue.empty())
        return;
    Entry entry = std::move(m_queue.front());
    m_queue.pop_front();
    m_text = std::move(entry.text);
    m_expiry = now + entry.duration;
    m_start = now;
    m_color = entry.color;
    m_visible = true;
}

void TFleetingTextQueue::dismiss(const Millis now)
{
    m_expiry = 0;
    m_visible = false;
    update(now);
}

void TTextMessageQueue::push(const QString& text, const Millis duration, const QColor& color)
{
    if (!m_queue.empty() && m_queue.size() >= m_max)
        m_queue.pop_front();
    TFleetingTextQueue::push(text, duration << (m_visible ? 0 : 1), color);
}

TNpcSpeechBubble::TNpcSpeechBubble(const QString& name, const QColor& color, const Position& coordinate)
    : m_name(name)
    , m_header(qtTrId("speech_npc_header").arg(name))
    , m_bubbleColor(color)
    , m_coordinate(coordinate)
{}

void TNpcSpeechBubble::setHeader(const QColor& color, const Position& coordinate)
{
    m_bubbleColor = color;
    m_coordinate = coordinate;
}

void TNpcSpeechBubble::pushText(const QString& text, const Millis duration)
{
    push(npcRichText(text), duration, m_bubbleColor);
}

QString TNpcSpeechBubble::bubbleText() const
{
    return QStringLiteral("%1\n%2").arg(m_header, m_text);
}

TCreatureSpeechBubble::TCreatureSpeechBubble(const QString& speaker, const int volume, const QColor& color,
                                             const Position& coordinate, const size_t maxLines)
    : m_speaker(speaker)
    , m_volume(volume)
    , m_color(color)
    , m_coordinate(coordinate)
    , m_maxLines(maxLines)
{
    if (volume != Monster)
        m_header = volumeHeader(volume).arg(speaker);
}

bool TCreatureSpeechBubble::matches(const QString& speaker, const int volume, const QColor& color, const Position& coordinate) const
{
    return m_speaker == speaker && m_volume == volume && m_color == color && m_coordinate == coordinate;
}

void TCreatureSpeechBubble::addLine(const QString& text, const Millis now, const Millis duration)
{
    m_lines.push_back({ m_volume == Yell ? text.toUpper() : text, now + duration, true });
    while (m_lines.size() > m_maxLines)
        m_lines.pop_front();
    m_expiry = std::max(m_expiry, now + duration);
}

void TCreatureSpeechBubble::update(const Millis now)
{
    for (Line& line : m_lines)
        line.visible = now < line.expiry;
    while (!m_lines.empty() && !m_lines.front().visible)
        m_lines.pop_front();
}

QString TCreatureSpeechBubble::text() const
{
    QString text = m_header;
    if (!text.isEmpty())
        text += u'\n';
    QString separator;
    for (const Line& line : m_lines) {
        text += separator + line.text;
        separator = QStringLiteral("\n");
    }
    return text;
}

TTextStorage::TTextStorage(const size_t maxCreatureBubbles, const size_t maxQueuedMessages)
    : m_maxCreatureBubbles(maxCreatureBubbles)
    , m_maxQueuedMessages(maxQueuedMessages)
    , m_center(maxQueuedMessages)
    , m_private(maxQueuedMessages)
{}

Millis TTextStorage::now()
{
    return g_clock.millis();
}

void TTextStorage::update(const Millis now, const Position& player)
{
    m_statusVisible = now < m_statusExpiry;
    m_lookVisible = now < m_lookExpiry;
    m_center.update(now);
    m_private.update(now);

    for (const auto& bubble : m_creatureBubbles)
        bubble->update(now);
    std::erase_if(m_creatureBubbles, [](const auto& bubble) { return bubble->isEmpty(); });

    std::erase_if(m_npcBubbles, [&player](const TNpcSpeechBubble& bubble) {
        if (!player.isValid())
            return false;
        return std::abs(bubble.coordinate().x - player.x) > NpcBubbleRange
            || std::abs(bubble.coordinate().y - player.y) > NpcBubbleRange;
    });
    for (TNpcSpeechBubble& bubble : m_npcBubbles)
        bubble.update(now);
}

void TTextStorage::clear()
{
    m_statusExpiry = m_lookExpiry = 0;
    m_statusVisible = m_lookVisible = false;
    m_center = TTextMessageQueue(m_maxQueuedMessages);
    m_private = TTextMessageQueue(m_maxQueuedMessages);
    m_npcBubbles.clear();
    m_creatureBubbles.clear();
}

void TTextStorage::setStatusMessage(const QString& text, const QColor& color)
{
    const Millis time = now();
    m_statusText = text;
    m_statusExpiry = time + StatusDuration;
    m_statusVisible = true;
    m_statusColor = color;
}

void TTextStorage::clearOldestMessage()
{
    if (m_statusVisible) {
        m_statusVisible = false;
        m_statusExpiry = 0;
        return;
    }
    if (m_lookVisible) {
        m_lookVisible = false;
        m_lookExpiry = 0;
        return;
    }
    if (m_center.isVisible())
        m_center.dismiss(now());
    else if (m_private.isVisible())
        m_private.dismiss(now());
}

void TTextStorage::setLookMessage(const QString& text, const QColor& color, const bool isHtml)
{
    m_lookText = text;
    m_lookExpiry = now() + textDuration(text);
    m_lookVisible = true;
    m_lookColor = color;
    m_lookHtml = isHtml;
}

void TTextStorage::addCenterMessage(const QString& text, const QColor& color)
{
    m_center.push(text, textDuration(text), color);
    m_center.update(now());
}

void TTextStorage::addGamemasterMessage(const QString& name, const QString& text, const QColor& color)
{
    const QString message = QStringLiteral("%1:\n%2").arg(name, text);
    m_center.push(message, textDuration(message), color);
    m_center.update(now());
}

void TTextStorage::addPrivateMessage(const QString& name, const QString& text, const QColor& color)
{
    const QString message = QStringLiteral("%1:\n%2").arg(name, text);
    m_private.push(message, privateTextDuration(message), color);
    m_private.update(now());
}

void TTextStorage::addCreatureSpeech(const QString& speaker, const int volume, const QString& text, const QColor& color,
                                     const Position& coordinate)
{
    const Millis time = now();
    const Millis duration = textDuration(text);
    for (const auto& bubble : m_creatureBubbles) {
        if (bubble->matches(speaker, volume, color, coordinate)) {
            bubble->addLine(text, time, duration);
            return;
        }
    }
    if (!m_creatureBubbles.empty() && m_creatureBubbles.size() == m_maxCreatureBubbles) {
        const auto oldest = std::min_element(m_creatureBubbles.begin(), m_creatureBubbles.end(),
                                             [](const auto& a, const auto& b) { return a->expiry() < b->expiry(); });
        m_creatureBubbles.erase(oldest);
    }
    auto bubble = std::make_shared<TCreatureSpeechBubble>(speaker, volume, color, coordinate, MaxCreatureBubbleLines);
    bubble->addLine(text, time, duration);
    m_creatureBubbles.push_back(std::move(bubble));
}

void TTextStorage::setNpcSpeechHeader(const QString& name, const QColor& color, const Position& coordinate)
{
    const auto it = std::find_if(m_npcBubbles.begin(), m_npcBubbles.end(),
                                 [&name](const TNpcSpeechBubble& bubble) { return bubble.name() == name; });
    if (it == m_npcBubbles.end()) {
        m_npcBubbles.emplace_front(name, color, coordinate);
        while (m_npcBubbles.size() > MaxNpcBubbles)
            m_npcBubbles.pop_back();
        return;
    }
    it->setHeader(color, coordinate);
    std::rotate(m_npcBubbles.begin(), it, it + 1);
}

void TTextStorage::addNpcSpeech(const QString& name, const QString& text)
{
    const auto it = std::find_if(m_npcBubbles.begin(), m_npcBubbles.end(),
                                 [&name](const TNpcSpeechBubble& bubble) { return bubble.name() == name; });
    if (it == m_npcBubbles.end())
        return;
    it->pushText(text, npcTextDuration(text));
    it->update(now());
}

}
