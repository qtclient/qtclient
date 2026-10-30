#include "cipsoftuieventbridge.h"

#include <any>
#include <string>
#include <vector>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

}

TCipSoftUiEventBridge::TCipSoftUiEventBridge(QObject* parent)
    : QObject(parent)
{
    m_hooks.emplace_back("g_game", "onPlayerHelpersUpdate", [this](LuaObject*, const Args& args) {
        m_playerHelpers = static_cast<int>(client::gamecore::argInt(args, 0));
        Q_EMIT playerHelpersChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGemAtelierGemRevealed", [this](LuaObject*, const Args& args) {
        m_lastRevealedGemId = static_cast<quint32>(client::gamecore::argInt(args, 0));
        Q_EMIT gemRevealed(m_lastRevealedGemId);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onVirtuesYellowBorder", [this](LuaObject*, const Args& args) {
        m_virtuesYellowBorder.clear();
        if (const auto* spellIds = args.empty() ? nullptr : std::any_cast<std::vector<uint16_t>>(&args[0])) {
            m_virtuesYellowBorder.reserve(static_cast<int>(spellIds->size()));
            for (const uint16_t id : *spellIds)
                m_virtuesYellowBorder.append(static_cast<int>(id));
        }
        Q_EMIT virtuesYellowBorderChanged();
        return std::any{};
    });
}

}
