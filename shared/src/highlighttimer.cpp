#include "highlighttimer.h"

namespace tibia::shared {

THighlightTimer::THighlightTimer(const int Interval)
    : QObject(nullptr)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(Interval);
    connect(&m_timer, &QTimer::timeout, this, &THighlightTimer::changed);
    connect(&m_timer, &QTimer::timeout, this, [this] { Q_EMIT timeout(m_protected); });
}

bool THighlightTimer::isHighlighted() const
{
    return m_timer.isActive();
}

qint64 THighlightTimer::interval() const
{
    return m_timer.interval();
}

void THighlightTimer::start(const bool Protected)
{
    if (!m_protected)
        m_protected = Protected;
    m_timer.start();
    Q_EMIT changed();
}

void THighlightTimer::stop(const bool Force)
{
    if (m_protected && !Force)
        return;
    m_protected = false;
    m_timer.stop();
    Q_EMIT changed();
}

}
