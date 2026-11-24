#pragma once

#include "shared/cip_typed.h"

#include "gameaction.h"

#include <Qt>

#include <cstdint>
#include <map>
#include <vector>

namespace tibia {
namespace input {

/// Picks the action a mouse click runs from the candidates under the mouse
/// (CIP: TGameActionMapper). It keeps a rule table per control scheme
/// (addControlScheme); the active rules are a copy of the current scheme's
/// table (setControlScheme), which the loot scheme rewrites under the classic
/// scheme (setLootScheme). Control schemes (clientOptions controlScheme): 1 classic,
/// 2 legacy, 3 left smart click. Loot schemes (clientOptions lootScheme):
/// 0 right, 1 shift+right, 2 left.
class TGameActionMapper : public ::tibia::CipTyped
{
public:
    /// Rule context of the world map (its rules apply to every context).
    static constexpr int WorldMapContext = 0;
    /// Rule context of containers and the inventory (CIP:).
    static constexpr int ContainerContext = 3;
    /// Rule context of the sidebar minimap (CIP: TMinimapController click,).
    static constexpr int MinimapContext = 6;

    static constexpr int ClassicControlScheme = 1;
    static constexpr int LegacyControlScheme = 2;
    static constexpr int LeftSmartClickControlScheme = 3;
    static constexpr int LootRight = 0;
    static constexpr int LootShiftRight = 1;
    static constexpr int LootLeft = 2;

    /// Host: CipSoft's client adds the three schemes' tables to its mapper
    /// (classic, legacy, left smart click) and then applies the options' control
    /// and loot schemes; this constructor does the same for `controlScheme` and
    /// `lootScheme` (defaults classic and right).
    explicit TGameActionMapper(int controlScheme = ClassicControlScheme, int lootScheme = LootRight);

    int controlScheme() const { return m_controlScheme; }
    /// The rules become the scheme's table (a loot rewrite is dropped);
    /// std::out_of_range for a scheme without one.
    void setControlScheme(int controlScheme);
    /// Under the classic scheme the rules become the classic table rewritten
    /// for the loot scheme: right adds quick loot after use to the right click
    /// and gives containers a right click without it, left puts quick loot
    /// first on the left click; both drop the shift+right quick loot. Other
    /// control schemes keep their rules.
    void setLootScheme(int lootScheme);

    /// The matching rule's action types (CIP:): a rule of the
    /// context wins over the last matching rule of every context.
    std::vector<EGameAction> ruleTypes(int context, Qt::MouseButton button, int modifiers) const;
    /// The first candidate of the matching rule's types, or null.
    IGameActionPtr map(const TGameActionList& candidates, int context, Qt::MouseButton button, int modifiers) const;

    /// One mouse rule: a click of `button` (1 left, 2 right, 4 middle) with
    /// `modifiers` (Qt's Shift / Ctrl / Alt bits, all three: any) in `context`.
    struct TRule
    {
        int context = 0;
        int button = 0;
        uint32_t modifiers = 0;
        std::vector<EGameAction> types;
    };

    /// Adds `rules` as the table of `controlScheme` (a scheme that has a table
    /// keeps it). While the mapper has no active rules, the scheme becomes the
    /// current one and its table the active rules.
    void addControlScheme(int controlScheme, std::vector<TRule> rules);

private:
    int m_controlScheme = 0;
    std::vector<TRule> m_rules;
    std::map<int, std::vector<TRule>> m_controlSchemes;
};

}
}
