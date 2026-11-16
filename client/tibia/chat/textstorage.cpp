#include "textstorage.h"

#include "clockservice.h"

#include "creatures/creaturestorage.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace tibia::chat {

namespace {

constexpr Millis StatusDuration = 5000;
/// NPC bubbles close once the player is farther than this.
constexpr int NpcBubbleRange = 3;
constexpr size_t MaxNpcBubbles = 10;
constexpr size_t MaxCreatureBubbleLines = 10;

/// NPC keywords.
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

/// Escapes the tags an NPC answer could carry.
void escapeTags(QString& text)
{
    text.replace(u'<', QLatin1String("&lt;")).replace(u'>', QLatin1String("&gt;"));
}

/// Colours the {keywords} of an NPC answer.
void highlightKeywords(QString& text, const QColor& color)
{
    text.replace(QStringLiteral("{"), QStringLiteral("<font color=\"%1\">").arg(color.name()));
    text.replace(QStringLiteral("}"), QStringLiteral("</font>"));
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

void TTextMessageQueue::push(const QString& text, const Millis duration, const QColor& color)
{
    if (!m_queue.empty() && m_queue.size() >= m_max)
        m_queue.pop_front();
    TFleetingTextQueue::push(text, duration << (m_visible ? 0 : 1), color);
}

void TNpcSpeechBubble::push(const QString&, const Millis, const QColor&) {}

void TNpcSpeechBubble::setHeader(const QString& name, const QColor& color, const Position& coordinate)
{
    m_name = name;
    m_bubbleColor = color;
    m_coordinate = coordinate;
    m_header = qtTrId("speech_npc_header").arg(m_name);
    m_queue.clear();
    m_text.clear();
    m_expiry = 0;
    m_visible = false;
    m_color = Qt::white;
}

void TNpcSpeechBubble::addText(const QString& name, const QString& text, const Millis duration)
{
    if (m_name != name)
        return;
    QString richText = text;
    escapeTags(richText);
    highlightKeywords(richText, NpcHighlightColor);
    m_queue.push_back({ richText, duration, m_bubbleColor });
}

QString TNpcSpeechBubble::bubbleText() const
{
    return QStringLiteral("%1\n%2").arg(m_header).arg(m_text);
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
    m_lines.push_back({ m_volume == Yell ? text.toUpper() : text, now + duration, now, true, m_color });
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

void TCreatureSpeechBubble::clearFirstLine()
{
    if (m_lines.empty())
        return;
    Line& line = m_lines.front();
    line.text.clear();
    line.expiry = 0;
    line.visible = false;
    line.color = Qt::white;
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
{
    m_creatureBubbles.reserve(maxCreatureBubbles);
}

Millis TTextStorage::now()
{
    return tibia::shared::TClockService::frameMillis();
}

void TTextStorage::update(const Millis now)
{
    m_statusVisible = now < m_statusExpiry;
    m_lookVisible = now < m_lookExpiry;
    m_center.update(now);
    m_private.update(now);

    for (const auto& bubble : m_creatureBubbles)
        bubble->update(now);
    std::erase_if(m_creatureBubbles, [](const auto& bubble) { return bubble->isEmpty(); });

    std::erase_if(m_npcBubbles, [this](const TNpcSpeechBubble& bubble) {
        return bubble.isVisible() && !isInNpcBubbleRange(bubble.coordinate());
    });
    for (TNpcSpeechBubble& bubble : m_npcBubbles)
        bubble.update(now);
}

bool TTextStorage::isInNpcBubbleRange(const Position& coordinate) const
{
    const std::shared_ptr<creatures::TCreature> player = m_player.lock();
    if (!player)
        return true;
    const Position& position = player->position();
    return std::abs(int(coordinate.x) - int(position.x)) <= NpcBubbleRange
        && std::abs(int(coordinate.y) - int(position.y)) <= NpcBubbleRange;
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

void TTextStorage::setStatusMessage(const QString& text, const QColor& color, const bool isHtml)
{
    const Millis time = now();
    m_statusText = text;
    m_statusExpiry = time + StatusDuration;
    m_statusStart = time;
    m_statusColor = color;
    m_statusVisible = true;
    m_statusHtml = isHtml;
}

void TTextStorage::clearOldestMessage()
{
    constexpr Millis None = std::numeric_limits<Millis>::max();
    Millis oldest = m_lookVisible ? m_lookStart : None;
    if (m_center.m_visible)
        oldest = std::min(oldest, m_center.m_start);
    if (m_private.m_visible)
        oldest = std::min(oldest, m_private.m_start);
    TNpcSpeechBubble* oldestNpcBubble = nullptr;
    for (TNpcSpeechBubble& bubble : m_npcBubbles) {
        if (!bubble.m_visible)
            continue;
        if (bubble.m_start < oldest)
            oldestNpcBubble = &bubble;
        oldest = std::min(oldest, bubble.m_start);
    }
    std::shared_ptr<TCreatureSpeechBubble> oldestCreatureBubble;
    for (const auto& bubble : m_creatureBubbles) {
        if (!bubble->isEmpty() && bubble->firstLineStart() < oldest) {
            oldest = bubble->firstLineStart();
            oldestCreatureBubble = bubble;
        }
    }
    if (oldest == None)
        return;

    const auto blank = [](TFleetingTextQueue& queue) {
        queue.m_text.clear();
        queue.m_expiry = 0;
        queue.m_visible = false;
        queue.m_color = Qt::white;
    };
    if (oldest == m_lookStart) {
        m_lookText.clear();
        m_lookExpiry = 0;
        m_lookVisible = false;
        m_lookColor = Qt::white;
    } else if (oldest == m_center.m_start) {
        blank(m_center);
    } else if (oldest == m_private.m_start) {
        blank(m_private);
    } else if (oldestNpcBubble) {
        blank(*oldestNpcBubble);
    } else if (oldestCreatureBubble) {
        oldestCreatureBubble->clearFirstLine();
    }
}

void TTextStorage::setLookMessage(const QString& text, const QColor& color, const bool isHtml)
{
    const Millis duration = textDuration(text);
    const Millis time = now();
    m_lookText = text;
    m_lookExpiry = duration + time;
    m_lookStart = time;
    m_lookColor = color;
    m_lookVisible = true;
    m_lookHtml = isHtml;
}

void TTextStorage::addCenterMessage(const QString& text, const QColor& color)
{
    m_center.push(text, textDuration(text), color);
    m_center.update(now());
}

void TTextStorage::addGamemasterMessage(const QString& name, const QString& text, const QColor& color)
{
    const Millis duration = textDuration(text);
    m_center.push(QStringLiteral("%1:\n%2").arg(name).arg(text), duration, color);
    m_center.update(now());
}

void TTextStorage::addPrivateMessage(const QString& name, const QString& text, const QColor& color)
{
    const Millis duration = privateTextDuration(text);
    m_private.push(QStringLiteral("%1:\n%2").arg(name).arg(text), duration, color);
    m_private.update(now());
}

void TTextStorage::addCreatureSpeech(const QString& speaker, const int volume, const QString& text, const QColor& color,
                                     const Position& coordinate)
{
    const Millis duration = textDuration(text);
    const Millis time = now();
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
        m_npcBubbles.emplace_front();
        m_npcBubbles.front().setHeader(name, color, coordinate);
        if (m_npcBubbles.size() > MaxNpcBubbles)
            m_npcBubbles.resize(MaxNpcBubbles);
        return;
    }
    it->setHeader(name, color, coordinate);
    std::rotate(m_npcBubbles.begin(), it, it + 1);
}

void TTextStorage::addNpcSpeech(const QString& name, const QString& text)
{
    const Millis duration = npcTextDuration(text);
    const auto it = std::find_if(m_npcBubbles.begin(), m_npcBubbles.end(),
                                 [&name](const TNpcSpeechBubble& bubble) { return bubble.name() == name; });
    if (it == m_npcBubbles.end())
        return;
    it->addText(name, text, duration);
    it->update(now());
}

bool TTextStorage::hasVisibleNpcBubble() const
{
    return std::any_of(m_npcBubbles.begin(), m_npcBubbles.end(), [](const TNpcSpeechBubble& bubble) { return bubble.isVisible(); });
}

void TTextStorage::onPlayerCreatureAddedToGameSession(std::weak_ptr<::tibia::creatures::TCreature> pPlayer)
{
    m_player = std::move(pPlayer);
}

}
