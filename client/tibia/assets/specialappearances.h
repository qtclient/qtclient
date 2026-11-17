#pragma once

#include <cstdint>
#include <unordered_set>

namespace tibia::appearances {

/// The object kinds that carry extra data in the game protocol, one id set per kind
/// (CIP: TSpecialAppearancesForProtocolParsing). An object belongs to at most one
/// of the first eight kinds; the constructor throws std::invalid_argument when two
/// of them share an id.
class TSpecialAppearancesForProtocolParsing
{
public:
    using IdSet = std::unordered_set<uint16_t>;

    TSpecialAppearancesForProtocolParsing() = default;
    TSpecialAppearancesForProtocolParsing(IdSet cumulative, IdSet liquid, IdSet container, IdSet showOffSocket,
                                          IdSet upgradeable, IdSet wearOut, IdSet expire, IdSet expireStop,
                                          IdSet decoKit);

    bool isCumulative(uint16_t id) const { return m_cumulative.contains(id); }
    bool isLiquid(uint16_t id) const { return m_liquid.contains(id); }
    bool isContainer(uint16_t id) const { return m_container.contains(id); }
    bool isShowOffSocket(uint16_t id) const { return m_showOffSocket.contains(id); }
    bool isUpgradeable(uint16_t id) const { return m_upgradeable.contains(id); }
    bool isWearOut(uint16_t id) const { return m_wearOut.contains(id); }
    bool isExpire(uint16_t id) const { return m_expire.contains(id); }
    bool isExpireStop(uint16_t id) const { return m_expireStop.contains(id); }
    bool isDecoKit(uint16_t id) const { return m_decoKit.contains(id); }

private:
    void checkExclusive() const;

    IdSet m_cumulative;
    IdSet m_liquid;
    IdSet m_container;
    IdSet m_showOffSocket;
    IdSet m_upgradeable;
    IdSet m_wearOut;
    IdSet m_expire;
    IdSet m_expireStop;
    IdSet m_decoKit;
};

}
