#include "gameactionmapper.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace tibia::input {

namespace {

constexpr uint32_t Shift = 0x2000000;
constexpr uint32_t Ctrl = 0x4000000;
constexpr uint32_t Alt = 0x8000000;
constexpr uint32_t AnyModifiers = Shift | Ctrl | Alt;

using Rule = TGameActionMapper::TRule;
using A = EGameAction;

// Rules in file order (CIP: classic, legacy,
// smart). Context 0 applies to every context. Buttons: 1 left,
// 2 right, 4 middle.
const Rule ClassicRules[] = {
    { 0, 1, 0, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 0, 4, 0, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Shift, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Ctrl, { A::CONTEXT_MENU, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Alt, { A::ATTACK_CREATURE, A::START_TALKING_TO_NPC, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Ctrl | Alt, { A::ATTACK_CREATURE, A::START_TALKING_TO_NPC, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, 0, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC, A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, Ctrl, { A::CONTEXT_MENU, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, Shift, { A::QUICK_LOOT } },
    { 1, 1, 0, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC } },
    { 1, 2, 0, { A::CONTEXT_MENU } },
    { 2, 1, 0, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 2, 2, 0, { A::CONTEXT_MENU } },
    { 3, 1, 0, { A::QUICK_LOOT } },
    { 3, 1, Alt, { A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW } },
    { 3, 1, Ctrl | Alt, { A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW } },
    { 3, 2, Shift, { A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 4, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 5, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 6, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 6, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 6, 1, AnyModifiers, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 7, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 8, 1, 0, { A::STASH_OPEN_RETRIEVE_DIALOG } },
    { 8, 2, 0, { A::CONTEXT_MENU } },
    { 9, 1, 0, { A::CYCLOPEDIA_MAP_SELECT_ITEM } },
    { 9, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 9, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 9, 1, Shift, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 10, 2, 0, { A::CONTEXT_MENU } },
};

const Rule LegacyRules[] = {
    { 0, 1, 0, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 0, 4, 0, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 0, 1, Shift, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Ctrl, { A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Alt, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC, A::START_TALKING_TO_NPC, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Ctrl | Alt, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC, A::START_TALKING_TO_NPC, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, AnyModifiers, { A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, Ctrl, { A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, 0, { A::CONTEXT_MENU, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, Shift, { A::QUICK_LOOT } },
    { 1, 1, 0, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC } },
    { 1, 1, Ctrl, { A::CONTEXT_MENU } },
    { 2, 1, 0, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 2, 1, Ctrl, { A::CONTEXT_MENU } },
    { 3, 1, Alt, { A::CONTEXT_MENU } },
    { 3, 1, Ctrl | Alt, { A::CONTEXT_MENU } },
    { 3, 2, Shift, { A::QUICK_LOOT } },
    { 4, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 5, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 6, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 6, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 6, 1, AnyModifiers, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 7, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 8, 1, 0, { A::STASH_OPEN_RETRIEVE_DIALOG } },
    { 8, 2, 0, { A::CONTEXT_MENU } },
    { 9, 1, 0, { A::CYCLOPEDIA_MAP_SELECT_ITEM } },
    { 9, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 9, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 9, 1, Shift, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 10, 2, 0, { A::CONTEXT_MENU } },
};

const Rule SmartRules[] = {
    { 0, 1, 0, { A::SMART_CLICK } },
    { 0, 1, Shift, { A::LOOK, A::LOOK_AT_CREATURE, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Ctrl, { A::SET_PLAYER_MOVEMENT_INTENT, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 1, Alt, { A::QUICK_LOOT, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 4, 0, { A::SET_PLAYER_MOVEMENT_INTENT, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 0, 2, 0, { A::CONTEXT_MENU, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } },
    { 3, 1, Ctrl, { A::OPEN_IN_NEW_WINDOW } },
    { 3, 1, Alt, { A::QUICK_LOOT } },
    { 6, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 4, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 5, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 7, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 8, 1, 0, { A::STASH_OPEN_RETRIEVE_DIALOG } },
    { 8, 2, 0, { A::CONTEXT_MENU } },
    { 9, 1, 0, { A::CYCLOPEDIA_MAP_SELECT_ITEM } },
    { 9, 2, AnyModifiers, { A::CONTEXT_MENU } },
    { 9, 1, Shift | Ctrl, { A::GOD_TELEPORT } },
    { 9, 1, Shift, { A::SET_PLAYER_MOVEMENT_INTENT } },
    { 10, 2, 0, { A::CONTEXT_MENU } },
};

/// The table of `controlScheme` (CIP: the scheme -> rules map's at()).
std::span<const Rule> rulesOf(const int controlScheme)
{
    switch (controlScheme) {
    case TGameActionMapper::ClassicControlScheme: return ClassicRules;
    case TGameActionMapper::LegacyControlScheme: return LegacyRules;
    case TGameActionMapper::LeftSmartClickControlScheme: return SmartRules;
    default: throw std::out_of_range("map::at:  key not found");
    }
}

std::vector<Rule>::iterator findRule(std::vector<Rule>& rules, const int context, const int button, const uint32_t modifiers)
{
    return std::find_if(rules.begin(), rules.end(), [&](const Rule& rule) {
        return rule.context == context && rule.button == button && rule.modifiers == modifiers;
    });
}

int buttonOf(const Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return 1;
    case Qt::RightButton: return 2;
    case Qt::MiddleButton: return 4;
    default: return 0;
    }
}

/// CipSoft control-scheme tables use Qt::ControlModifier for the menu/use
/// chords. On macOS the same chords are Cmd (Qt::MetaModifier) — fold Meta into
/// Ctrl and drop unrelated modifier bits so rule matches stay exact.
int cipKeyboardModifiers(const int modifiers)
{
    int cip = modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (cip & Qt::MetaModifier)
        cip = (cip & ~Qt::MetaModifier) | Qt::ControlModifier;
    return cip;
}

}

TGameActionMapper::TGameActionMapper(const int controlScheme, const int lootScheme)
{
    setControlScheme(controlScheme);
    setLootScheme(lootScheme);
}

void TGameActionMapper::setControlScheme(const int controlScheme)
{
    const std::span<const Rule> rules = rulesOf(controlScheme);
    m_controlScheme = controlScheme;
    m_rules.assign(rules.begin(), rules.end());
}

void TGameActionMapper::setLootScheme(const int lootScheme)
{
    if (m_controlScheme != ClassicControlScheme)
        return;
    const std::span<const Rule> classic = rulesOf(ClassicControlScheme);
    std::vector<Rule> rules(classic.begin(), classic.end());
    if (lootScheme == LootRight) {
        // Right click: quick loot right after use.
        if (const auto right = findRule(rules, 0, 2, 0); right != rules.end()) {
            std::vector<A>& types = right->types;
            const auto use = std::find(types.begin(), types.end(), A::USE);
            types.insert(use == types.end() ? use : use + 1, A::QUICK_LOOT);
        }
        if (const auto shiftRight = findRule(rules, 0, 2, Shift); shiftRight != rules.end())
            rules.erase(shiftRight);
        // Containers: a right click without quick loot, before the left click's rule.
        rules.insert(findRule(rules, 3, 1, 0),
                     Rule{ 3, 2, 0, { A::ATTACK_CREATURE, A::STOP_ATTACKING_CREATURE, A::START_TALKING_TO_NPC, A::USE, A::USE_ON_CREATURE, A::USE_WITH, A::OPEN, A::OPEN_IN_NEW_WINDOW, A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE } });
    } else if (lootScheme == LootLeft) {
        if (const auto left = findRule(rules, 0, 1, 0); left != rules.end())
            left->types.insert(left->types.begin(), A::QUICK_LOOT);
        if (const auto shiftRight = findRule(rules, 0, 2, Shift); shiftRight != rules.end())
            rules.erase(shiftRight);
    }
    m_rules = std::move(rules);
}

std::vector<EGameAction> TGameActionMapper::ruleTypes(const int context, const Qt::MouseButton button, const int modifiers) const
{
    const int cipButton = buttonOf(button);
    const int cipMods = cipKeyboardModifiers(modifiers);
    const Rule* match = nullptr;
    for (const Rule& rule : m_rules) {
        if ((rule.context != context && rule.context != 0) || rule.button != cipButton)
            continue;
        if (rule.modifiers != AnyModifiers && rule.modifiers != static_cast<uint32_t>(cipMods))
            continue;
        match = &rule;
        if (rule.context != 0)
            break;
    }
    return match ? match->types : std::vector<EGameAction>{};
}

IGameActionPtr TGameActionMapper::map(const TGameActionList& candidates, const int context, const Qt::MouseButton button, const int modifiers) const
{
    const std::vector<EGameAction> types = ruleTypes(context, button, modifiers);
    if (types.empty())
        return nullptr;
    const TGameActionList actions = filterActions(candidates, types);
    return actions.empty() ? nullptr : actions.front();
}

}
