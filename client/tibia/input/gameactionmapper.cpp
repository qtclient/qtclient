#include "gameactionmapper.h"

#include <algorithm>
#include <span>

namespace tibia::input {

namespace {

constexpr uint32_t Shift = 0x2000000;
constexpr uint32_t Ctrl = 0x4000000;
constexpr uint32_t Alt = 0x8000000;
constexpr uint32_t AnyModifiers = Shift | Ctrl | Alt;

struct Rule
{
    int context;
    int button;
    uint32_t modifiers;
    std::vector<EGameAction> types;
};

using A = EGameAction;

// Rules in file order (CIP: 0x1077b2668 classic, 0x1077b2650 legacy,
// 0x1077b2680 smart). Context 0 applies to every context. Buttons: 1 left,
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

std::span<const Rule> rulesOf(const int controlScheme)
{
    switch (controlScheme) {
    case 1: return LegacyRules;
    case 2: return SmartRules;
    default: return ClassicRules;
    }
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

}

bool TGameActionMapper::isLootSchemeChord(const int cipButton, const int modifiers) const
{
    // lootSchemeIndex: 0 right, 1 shift+right, 2 left (CIP OptionsPageControls).
    switch (m_lootScheme) {
    case 0:
        return cipButton == 2 && modifiers == 0;
    case 2:
        return cipButton == 1 && modifiers == 0;
    default:
        return cipButton == 2 && modifiers == static_cast<int>(Shift);
    }
}

std::vector<EGameAction> TGameActionMapper::ruleTypes(const int context, const Qt::MouseButton button, const int modifiers) const
{
    const int cipButton = buttonOf(button);
    const Rule* match = nullptr;
    for (const Rule& rule : rulesOf(m_controlScheme)) {
        if ((rule.context != context && rule.context != 0) || rule.button != cipButton)
            continue;
        if (rule.modifiers != AnyModifiers && rule.modifiers != static_cast<uint32_t>(modifiers))
            continue;
        match = &rule;
        if (rule.context != 0)
            break;
    }
    std::vector<EGameAction> types = match ? match->types : std::vector<EGameAction>{};
    // Overlay lootSchemeIndex: inject or strip QUICK_LOOT so the option chord wins
    // over the hard-coded shift+right entries in the control-scheme tables.
    const bool lootChord = isLootSchemeChord(cipButton, modifiers);
    const bool lootContext = context == WorldMapContext || context == ContainerContext || context == 0;
    if (lootContext && lootChord) {
        if (std::find(types.begin(), types.end(), A::QUICK_LOOT) == types.end())
            types.insert(types.begin(), A::QUICK_LOOT);
    } else if (!types.empty()) {
        types.erase(std::remove(types.begin(), types.end(), A::QUICK_LOOT), types.end());
    }
    return types;
}

TGameActionPtr TGameActionMapper::map(const TGameActionList& candidates, const int context, const Qt::MouseButton button, const int modifiers) const
{
    const std::vector<EGameAction> types = ruleTypes(context, button, modifiers);
    if (types.empty())
        return nullptr;
    const TGameActionList actions = filterActions(candidates, types);
    return actions.empty() ? nullptr : actions.front();
}

}
