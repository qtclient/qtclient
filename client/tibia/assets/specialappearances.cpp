#include "assets/specialappearances.h"

#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tibia::appearances {

TSpecialAppearancesForProtocolParsing::TSpecialAppearancesForProtocolParsing(
    IdSet cumulative, IdSet liquid, IdSet container, IdSet showOffSocket, IdSet upgradeable, IdSet wearOut,
    IdSet expire, IdSet expireStop, IdSet decoKit)
    : m_cumulative(std::move(cumulative))
    , m_liquid(std::move(liquid))
    , m_container(std::move(container))
    , m_showOffSocket(std::move(showOffSocket))
    , m_upgradeable(std::move(upgradeable))
    , m_wearOut(std::move(wearOut))
    , m_expire(std::move(expire))
    , m_expireStop(std::move(expireStop))
    , m_decoKit(std::move(decoKit))
{
    checkExclusive();
}

void TSpecialAppearancesForProtocolParsing::checkExclusive() const
{
    const std::vector<std::pair<std::string, std::set<uint16_t>>> kinds{
        { "cumulative appearance", { m_cumulative.begin(), m_cumulative.end() } },
        { "liquid appearance", { m_liquid.begin(), m_liquid.end() } },
        { "container appearances", { m_container.begin(), m_container.end() } },
        { "showoffplattform appearances", { m_showOffSocket.begin(), m_showOffSocket.end() } },
        { "upgradeable appearances", { m_upgradeable.begin(), m_upgradeable.end() } },
        { "wearout appearances", { m_wearOut.begin(), m_wearOut.end() } },
        { "expire appearances", { m_expire.begin(), m_expire.end() } },
        { "expirestop appearances", { m_expireStop.begin(), m_expireStop.end() } },
    };
    for (const auto& [name, ids] : kinds) {
        for (const auto& [otherName, otherIds] : kinds) {
            if (name == otherName)
                continue;
            for (const uint16_t id : ids) {
                if (!otherIds.contains(id))
                    continue;
                std::ostringstream message;
                message << "A " << name << " can't be at the same time a " << otherName << " and vice versa";
                throw std::invalid_argument(message.str());
            }
        }
    }
}

}
