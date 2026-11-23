#include "gamewindow/viprenderinfo.h"

namespace tibia::gamewindow {

namespace {

constexpr int HighlightedTimerIntervalMs = 1000;

}

TVipRenderInfo::TVipRenderInfo(const creatures::TVipStorage::Vip& VipData, const QString& GroupName, const bool FirstGroupEntry)
    : cip::tibia::gamewindow::TVipRenderInfo(nullptr)
    , m_characterId(VipData.id)
    , m_name(VipData.name)
    , m_iconId(static_cast<uint8_t>(VipData.iconId))
    , m_state(VipData.state)
    , m_highlighted(false)
    , m_groups(VipData.groups)
    , m_groupName(GroupName)
    , m_firstGroupEntry(FirstGroupEntry)
{
    m_highlightedTimer.setInterval(HighlightedTimerIntervalMs);
    m_highlightedTimer.setSingleShot(true);
    connect(&m_highlightedTimer, &QTimer::timeout, this, &TVipRenderInfo::onHighlightedTimerTimeout);
}

void TVipRenderInfo::onHighlightedTimerTimeout()
{
    m_highlighted = false;
    Q_EMIT vipRenderInfoChanged(m_characterId);
}

}
