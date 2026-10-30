#include "crosshairgameactionhandlers.h"

#include "gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>

#include <array>

namespace tibia::input {

namespace {

using A = EGameAction;
using CastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

constexpr std::array<A, 5> UseWithActions{ A::USE_WITH, A::USE_ON_CREATURE, A::USE_ON_TARGET, A::USE_ON_YOURSELF, A::TARGET_SELECTED };
constexpr std::array<A, 1> CrossHairSpellActions{ A::CROSSHAIR_SPELL };

ThingPtr targetThing(const TGameAction& target)
{
    if (target.creatureId)
        return g_map.getCreatureById(target.creatureId);
    return target.object;
}

uint32_t targetCreatureId(const TGameAction& target)
{
    if (target.creatureId)
        return target.creatureId;
    return target.object && target.object->isCreature() ? target.object->static_self_cast<Creature>()->getId() : 0;
}

void publishUseOn(const TGameAction& action, const A type, const uint32_t creatureId)
{
    auto use = std::make_shared<TGameActionUseWith>();
    use->type = type;
    use->object = action.object;
    use->position = action.position;
    use->stackPosition = action.stackPosition;
    use->creatureId = creatureId;
    publish(use);
}

/// CIP FUN_100e78440: GameclientMessageUseOnCreature (object, its location, creature).
void useOnCreature(const TGameAction& action)
{
    const CreaturePtr creature = g_map.getCreatureById(action.creatureId);
    if (!creature || !action.object || !action.object->isItem())
        return;
    const ItemPtr item = action.object->static_self_cast<Item>();
    if (item->getPosition().isValid())
        g_game.useWith(item, creature);
    else
        g_game.useInventoryItemWith(item->getId(), creature);
}

/// The field a crosshair spell aims at (CIP FUN_100e78e70): a creature's, else
/// a world map field's; container and inventory targets have none.
Position targetPosition(const TGameAction& target)
{
    if (target.creatureId) {
        const CreaturePtr creature = g_map.getCreatureById(target.creatureId);
        return creature ? creature->getPosition() : Position();
    }
    if (target.position.isValid() && target.position.x != UINT16_MAX)
        return target.position;
    return {};
}

/// Selects a target, then publishes the action again with it.
void selectTargetFor(const TGameAction& action)
{
    auto pending = mutableClone(action);
    startTargetSelection([pending](const TGameActionPtr& target) {
        if (!target)
            return;
        pending->target = target;
        publish(pending);
    });
}

void useWith(const TGameAction& action)
{
    const ThingPtr target = targetThing(*action.target);
    if (!target || !action.object || !action.object->isItem())
        return;
    const ItemPtr item = action.object->static_self_cast<Item>();
    if (item->getPosition().isValid())
        g_game.useWith(item, target);
    else
        g_game.useInventoryItemWith(item->getId(), target);
}

void castAtTarget(const TGameAction& action)
{
    const Position position = targetPosition(*action.target);
    if (!position.isValid() || action.text.isEmpty())
        return;
    g_game.setNextTalkAim(position);
    g_game.setNextTalkAimMode(1);
    g_game.talkChannel(static_cast<Otc::MessageMode>(action.talkMode), 0, action.text.toStdString());
}

}

void say(const Otc::MessageMode mode, const QString& text, const int spellCastType)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spellOfText(text);
    if (spell && spell->crossHairTarget) {
        switch (static_cast<CastType>(spellCastType)) {
        case CastType::AtMousePosition:
        case CastType::None:
        case CastType::Crosshair: {
            std::shared_ptr<TGameAction> action = static_cast<CastType>(spellCastType) == CastType::AtMousePosition
                ? std::shared_ptr<TGameAction>(std::make_shared<TGameActionSpellAtMouseCursor>())
                : std::shared_ptr<TGameAction>(std::make_shared<TGameActionCrossHairSpell>());
            action->text = text;
            action->isSpell = true;
            action->spellCastType = spellCastType;
            action->talkMode = mode;
            publish(action);
            return;
        }
        case CastType::AtTargetOrSelf:
            g_game.setNextTalkAimMode(3);
            break;
        }
    }
    g_game.talkChannel(mode, 0, text.toStdString());
}

TUseWithGameActionHandler::TUseWithGameActionHandler(QObject* parent)
    : cip::tibia::input::TUseWithGameActionHandler(parent)
{
    // CIP FUN_100e78620: a creature target becomes USE_ON_CREATURE, anything else USE_TWO_OBJECTS.
    setActionHandler(A::USE_WITH, [](const TGameAction& action) {
        if (!action.target)
            selectTargetFor(action);
        else if (const uint32_t creatureId = targetCreatureId(*action.target))
            publishUseOn(action, A::USE_ON_CREATURE, creatureId);
        else
            useWith(action);
        return true;
    });
    setActionHandler(A::USE_ON_CREATURE, [](const TGameAction& action) {
        useOnCreature(action);
        return true;
    });
    // CIP 0x100a47200 (TCreaturesGameActionHandler): on the attack target, else a USE_WITH selection.
    setActionHandler(A::USE_ON_TARGET, [](const TGameAction& action) {
        const CreaturePtr target = g_game.getAttackingCreature();
        publishUseOn(action, target ? A::USE_ON_CREATURE : A::USE_WITH, target ? target->getId() : 0);
        return true;
    });
    // CIP 0x100a474d0: on the own character, if any.
    setActionHandler(A::USE_ON_YOURSELF, [](const TGameAction& action) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && player->getId())
            publishUseOn(action, A::USE_ON_CREATURE, player->getId());
        return true;
    });
    // Containers, the inventory, the battle list and the map publish the
    // TARGET_SELECTED under a selection's click; the pending action takes it.
    setActionHandler(A::TARGET_SELECTED, [](const TGameAction& action) {
        if (isSelectingTarget())
            finishTargetSelection(mutableClone(action));
        return true;
    });
}

TUseWithGameActionHandler::~TUseWithGameActionHandler()
{
    for (const A type : UseWithActions)
        setActionHandler(type, nullptr);
}

TCrossHairSpellGameActionHandler::TCrossHairSpellGameActionHandler(QObject* parent)
    : cip::tibia::input::TCrossHairSpellGameActionHandler(parent)
{
    setActionHandler(A::CROSSHAIR_SPELL, [](const TGameAction& action) {
        if (action.target)
            castAtTarget(action);
        else
            selectTargetFor(action);
        return true;
    });
}

TCrossHairSpellGameActionHandler::~TCrossHairSpellGameActionHandler()
{
    for (const A type : CrossHairSpellActions)
        setActionHandler(type, nullptr);
}

TCrossHairGameActionHandlers::TCrossHairGameActionHandlers() = default;

}
