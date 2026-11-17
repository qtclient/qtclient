#include "chatchanneltabrenderinfo.h"

namespace tibia::gamewindow {

namespace {
/// How long a new entry flashes its tab.
constexpr int FlashMilliseconds = 1000;
}

TChatChannelTabRenderInfo::TChatChannelTabRenderInfo(chat::TChatChannel* channel, QObject* parent)
    : cip::tibia::gamewindow::TChatChannelTabRenderInfo(parent)
    , m_channel(channel)
{
    m_flashTimer.setInterval(FlashMilliseconds);
    m_flashTimer.setSingleShot(true);
    connect(&m_flashTimer, &QTimer::timeout, this, &TChatChannelTabRenderInfo::onFlashTimerTimeout);
}

void TChatChannelTabRenderInfo::setTabHighlight(const EHighlight highlight)
{
    const EHighlight previous = m_tabHighlight;
    // Only CURRENT ends CURRENT, and only for NONE; NONE ends nothing else.
    const bool set = highlight == CURRENT || (previous == CURRENT ? highlight == NONE : highlight != NONE);
    if (set) {
        m_tabHighlight = highlight;
        if (previous != highlight) {
            m_flashTimer.stop();
            Q_EMIT tabHighlightChanged(m_channel);
        }
    }
    if (highlight == FLASH_NEW_ENTRY)
        m_flashTimer.start();
}

void TChatChannelTabRenderInfo::resetTabHighlight()
{
    if (m_tabHighlight == CURRENT)
        return;
    m_tabHighlight = NONE;
    Q_EMIT tabHighlightChanged(m_channel);
}

void TChatChannelTabRenderInfo::onFlashTimerTimeout()
{
    const EHighlight previous = m_tabHighlight;
    if (previous == CURRENT)
        return;
    m_tabHighlight = NEW_ENTRY;
    if (previous != NEW_ENTRY) {
        m_flashTimer.stop();
        Q_EMIT tabHighlightChanged(m_channel);
    }
}

}
