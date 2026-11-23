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

TFleetingText::TFleetingText()
    : m_color(Qt::white)
{
}

TFleetingText::TFleetingText(const QString& text, const Millis start, const Millis duration, const QColor& color, const bool isHtml)
    : TFleetingText()
{
    set(text, start, duration, color, isHtml);
}

void TFleetingText::set(const QString& text, const Millis start, const Millis duration, const QColor& color, const bool isHtml)
{
    m_text = text;
    m_expiry = duration + start;
    m_start = start;
    m_color = color;
    m_visible = true;
    m_html = isHtml;
}

void TFleetingText::clear()
{
    m_text.clear();
    m_color = Qt::white;
}

bool TFleetingText::operator==(const TFleetingText& other) const
{
    return m_text == other.m_text && m_expiry == other.m_expiry && m_color == other.m_color;
}

QString TFleetingTextQueue::nextText() const
{
    if (m_queue.empty())
        return QString();
    return m_queue.front().text;
}

void TFleetingTextQueue::push(const QString& text, const Millis duration, const QColor& color)
{
    m_queue.push_back({ text, duration, color });
}

void TFleetingTextQueue::update(const Millis now)
{
    m_current.update(now);
    if (m_current.isVisible() || m_queue.empty())
        return;
    Entry entry = std::move(m_queue.front());
    m_queue.pop_front();
    m_current.set(entry.text, now, entry.duration, entry.color, false);
}

void TTextMessageQueue::push(const QString& text, const Millis duration, const QColor& color)
{
    if (!m_queue.empty() && m_queue.size() >= m_max)
        m_queue.pop_front();
    TFleetingTextQueue::push(text, duration << (m_current.isVisible() ? 0 : 1), color);
}

void TNpcSpeechBubble::push(const QString&, const Millis, const QColor&) {}

void TNpcSpeechBubble::setHeader(const QString& name, const QColor& color, const Position& coordinate)
{
    m_name = name;
    m_bubbleColor = color;
    m_coordinate = coordinate;
    m_header = qtTrId("speech_npc_header").arg(m_name);
    m_queue.clear();
    m_current.m_text.clear();
    m_current.m_expiry = 0;
    m_current.m_visible = false;
    m_current.m_color = Qt::white;
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
    return QStringLiteral("%1\n%2").arg(m_header).arg(m_current.text());
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
    m_lines.emplace_back(m_volume == Yell ? text.toUpper() : text, now, duration, m_color, false);
    while (m_lines.size() > m_maxLines)
        m_lines.pop_front();
    m_expiry = std::max(m_expiry, now + duration);
}

void TCreatureSpeechBubble::update(const Millis now)
{
    for (TFleetingText& line : m_lines)
        line.update(now);
    while (!m_lines.empty() && !m_lines.front().isVisible())
        m_lines.pop_front();
}

void TCreatureSpeechBubble::clearFirstLine()
{
    if (m_lines.empty())
        return;
    TFleetingText& line = m_lines.front();
    line.m_text.clear();
    line.m_expiry = 0;
    line.m_visible = false;
    line.m_color = Qt::white;
}

QString TCreatureSpeechBubble::text() const
{
    QString text = m_header;
    if (!text.isEmpty())
        text += u'\n';
    QString separator;
    for (const TFleetingText& line : m_lines) {
        text += separator + line.text();
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
    m_status.update(now);
    m_look.update(now);
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
    m_status.m_expiry = m_look.m_expiry = 0;
    m_status.m_visible = m_look.m_visible = false;
    m_center = TTextMessageQueue(m_maxQueuedMessages);
    m_private = TTextMessageQueue(m_maxQueuedMessages);
    m_npcBubbles.clear();
    m_creatureBubbles.clear();
}

void TTextStorage::setStatusMessage(const QString& text, const QColor& color, const bool isHtml)
{
    m_status.set(text, now(), StatusDuration, color, isHtml);
}

void TTextStorage::clearOldestMessage()
{
    constexpr Millis None = std::numeric_limits<Millis>::max();
    Millis oldest = m_look.isVisible() ? m_look.start() : None;
    if (m_center.isVisible())
        oldest = std::min(oldest, m_center.start());
    if (m_private.isVisible())
        oldest = std::min(oldest, m_private.start());
    TNpcSpeechBubble* oldestNpcBubble = nullptr;
    for (TNpcSpeechBubble& bubble : m_npcBubbles) {
        if (!bubble.isVisible())
            continue;
        if (bubble.start() < oldest)
            oldestNpcBubble = &bubble;
        oldest = std::min(oldest, bubble.start());
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

    const auto blank = [](TFleetingText& text) {
        text.m_text.clear();
        text.m_expiry = 0;
        text.m_visible = false;
        text.m_color = Qt::white;
    };
    if (oldest == m_look.start()) {
        blank(m_look);
    } else if (oldest == m_center.start()) {
        blank(m_center.m_current);
    } else if (oldest == m_private.start()) {
        blank(m_private.m_current);
    } else if (oldestNpcBubble) {
        blank(oldestNpcBubble->m_current);
    } else if (oldestCreatureBubble) {
        oldestCreatureBubble->clearFirstLine();
    }
}

void TTextStorage::setLookMessage(const QString& text, const QColor& color, const bool isHtml)
{
    const Millis duration = textDuration(text);
    m_look.set(text, now(), duration, color, isHtml);
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
