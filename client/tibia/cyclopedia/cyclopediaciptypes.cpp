#include "cyclopedia/cyclopediaciptypes.h"

#include <any>
#include <vector>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

struct CipTypedKeep {
    TCyclopediaMapGameActionProvider tCyclopediaMapGameActionProvider;
    TGeneralPlayerStatisticsProvider tGeneralPlayerStatisticsProvider;
    TPlayerOffenceStatsProvider tPlayerOffenceStatsProvider;
    TPlayerDefenceStatsProvider tPlayerDefenceStatsProvider;
    TPlayerMiscStatsProvider tPlayerMiscStatsProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

}

TGeneralPlayerStatisticsProvider::TGeneralPlayerStatisticsProvider()
{
    m_hooks.emplace_back("g_game", "onParseCyclopediaCharacterCombatStats", [this](LuaObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterCombatStats>(&args[0]))
            m_combat = *data;
        if (const auto* mitigation = args.size() > 1 ? std::any_cast<double>(&args[1]) : nullptr)
            m_mitigation = *mitigation;
        else if (args.size() > 1)
            m_mitigation = static_cast<double>(client::gamecore::argInt(args, 1));
        return std::any{};
    });
}

TPlayerOffenceStatsProvider::TPlayerOffenceStatsProvider()
{
    m_hooks.emplace_back("g_game", "onCyclopediaCharacterOffenceStats", [this](LuaObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterOffenceStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

TPlayerDefenceStatsProvider::TPlayerDefenceStatsProvider()
{
    m_hooks.emplace_back("g_game", "onCyclopediaCharacterDefenceStats", [this](LuaObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterDefenceStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

TPlayerMiscStatsProvider::TPlayerMiscStatsProvider()
{
    m_hooks.emplace_back("g_game", "onCyclopediaCharacterMiscStats", [this](LuaObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterMiscStats>(&args[0]))
            m_last = *data;
        return std::any{};
    });
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::cyclopedia
