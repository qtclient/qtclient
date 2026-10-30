#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <Qt>

namespace tibia::input {

/// Picks the action a mouse click runs from the candidates under the mouse
/// (CIP: TGameActionMapper, FUN_100e7c040). Rules come from the control scheme
/// (option controlSchemeIndex: 0 classic, 1 legacy, 2 smart) and loot scheme
/// (option lootSchemeIndex: 0 right, 1 shift+right, 2 left).
class TGameActionMapper : public ::tibia::CipTyped
{
public:
    /// Rule context of the world map (its rules apply to every context).
    static constexpr int WorldMapContext = 0;
    /// Rule context of containers and the inventory (CIP: 0x100b79fc0).
    static constexpr int ContainerContext = 3;
    /// Rule context of the sidebar minimap (CIP: TMinimapController click, 0x100b94510).
    static constexpr int MinimapContext = 6;

    explicit TGameActionMapper(int controlScheme = 0, int lootScheme = 1)
        : m_controlScheme(controlScheme)
        , m_lootScheme(lootScheme)
    {
    }

    int controlScheme() const { return m_controlScheme; }
    void setControlScheme(int controlScheme) { m_controlScheme = controlScheme; }
    int lootScheme() const { return m_lootScheme; }
    void setLootScheme(int lootScheme) { m_lootScheme = lootScheme; }

    /// The matching rule's action types (CIP: FUN_100e7be50): a rule of the
    /// context wins over the last matching rule of every context.
    std::vector<EGameAction> ruleTypes(int context, Qt::MouseButton button, int modifiers) const;
    /// The first candidate of the matching rule's types, or null.
    TGameActionPtr map(const TGameActionList& candidates, int context, Qt::MouseButton button, int modifiers) const;

private:
    bool isLootSchemeChord(int cipButton, int modifiers) const;

    int m_controlScheme;
    int m_lootScheme;
};

}
