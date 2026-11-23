#pragma once

#include "creatures/vipstorage.h"

#include <cip_gen.h>

#include <QString>
#include <QTimer>

#include <cstdint>
#include <vector>

namespace tibia {
namespace gamewindow {

class TVipWidgetDataModel;

/// One row of the VIP list (CIP: gamewindow::TVipRenderInfo): a VIP shown
/// in one of its groups, with the group's name and whether it is the VIP's
/// first row. A VIP coming online is highlighted until the single-shot
/// highlight timer runs out. The data model owns the rows and updates them.
class TVipRenderInfo final : public cip::tibia::gamewindow::TVipRenderInfo
{
    Q_OBJECT
public:
    /// The highlight timer is single-shot, 1000 ms.
    TVipRenderInfo(const creatures::TVipStorage::Vip& VipData, const QString& GroupName, bool FirstGroupEntry);

    uint32_t characterId() const { return m_characterId; }
    const QString& name() const { return m_name; }
    uint8_t iconId() const { return m_iconId; }
    int state() const { return m_state; }
    bool highlighted() const { return m_highlighted; }
    const std::vector<uint8_t>& groups() const { return m_groups; }
    const QString& groupName() const { return m_groupName; }
    /// The VIP's first row (the only one shown while groups are hidden).
    bool firstGroupEntry() const { return m_firstGroupEntry; }

public Q_SLOTS:
    /// The highlight ends.
    void onHighlightedTimerTimeout();

Q_SIGNALS:
    void vipRenderInfoChanged(uint32_t CharacterID);

private:
    /// The data model writes state and highlight directly.
    friend class TVipWidgetDataModel;

    QTimer m_highlightedTimer;
    uint32_t m_characterId;
    QString m_name;
    uint8_t m_iconId;
    int m_state;
    bool m_highlighted;
    std::vector<uint8_t> m_groups;
    QString m_groupName;
    bool m_firstGroupEntry;
};

}
}
