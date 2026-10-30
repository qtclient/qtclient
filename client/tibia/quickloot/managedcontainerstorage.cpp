#include "managedcontainerstorage.h"

#include <tibia/world/game.h>
#include <client/position.h>

#include <any>

namespace tibia::quickloot {

namespace {

TManagedContainerStorage* g_instance = nullptr;

}

TManagedContainerStorage::TManagedContainerStorage(QObject* parent)
    : cip::tibia::quickloot::TManagedContainerStorage(parent)
{
    g_instance = this;
    m_hooks.emplace_back("g_game", "onQuickLootContainers", [this](LuaObject*, const std::vector<std::any>& args) {
        if (args.size() < 2)
            return std::any{};
        const bool fallback = client::gamecore::argBool(args, 0);
        const auto list = std::any_cast<std::vector<std::tuple<uint8_t, uint16_t, uint16_t>>>(args[1]);
        onContainers(fallback, list);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const std::vector<std::any>&) {
        m_fallback = false;
        m_assignments.clear();
        Q_EMIT useMainBackpackAsFallbackChanged();
        Q_EMIT managedContainersUpdated();
        return std::any{};
    });
}

TManagedContainerStorage::~TManagedContainerStorage()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TManagedContainerStorage* TManagedContainerStorage::instance() { return g_instance; }

void TManagedContainerStorage::setUseMainBackpackAsFallback(const bool value)
{
    if (m_fallback == value)
        return;
    m_fallback = value;
    if (g_game.isOnline())
        g_game.openContainerQuickLoot(3, 0, {}, 0, 0, value);
    Q_EMIT useMainBackpackAsFallbackChanged();
}

TManagedContainerStorage::Assignment TManagedContainerStorage::assignment(const uint8_t category) const
{
    const auto it = m_assignments.find(category);
    return it == m_assignments.end() ? Assignment{} : it->second;
}

void TManagedContainerStorage::onContainers(const bool fallback,
                                            const std::vector<std::tuple<uint8_t, uint16_t, uint16_t>>& list)
{
    // CIP 0x100f74e60: replace when the map or fallback differs.
    std::map<uint8_t, Assignment> next;
    for (const auto& [category, lootId, obtainId] : list)
        next[category] = { lootId, obtainId };
    const bool fallbackChanged = m_fallback != fallback;
    const bool mapChanged = m_assignments != next;
    m_fallback = fallback;
    m_assignments = std::move(next);
    if (fallbackChanged)
        Q_EMIT useMainBackpackAsFallbackChanged();
    if (mapChanged || fallbackChanged)
        Q_EMIT managedContainersUpdated();
}

}
