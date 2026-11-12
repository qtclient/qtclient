#include "crosshairgameactionhandlers.h"

#include "gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <creatures/creaturestorage.h>
#include <array>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;
using CastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

constexpr std::array<A, 5> UseWithActions{ A::USE_WITH, A::USE_ON_CREATURE, A::USE_ON_TARGET, A::USE_ON_YOURSELF, A::TARGET_SELECTED };
constexpr std::array<A, 1> CrossHairSpellActions{ A::CROSSHAIR_SPELL };

uint32_t targetCreatureId(const TGameAction& target)
{
    return target.creatureId;
}

bool targetMapObject(const TGameAction& target, Position& position, uint16_t& typeId, int& stackPos)
{
    if (target.creatureId || !target.objectClientId || !target.position.isValid() || target.stackPosition < 0)
        return false;
    position = target.position;
    typeId = target.objectClientId;
    stackPos = target.stackPosition;
    return true;
}

void publishUseOn(const TGameAction& action, const A type, const uint32_t creatureId)
{
    auto use = std::make_shared<TGameActionUseWith>();
    use->type = type;
    use->objectClientId = action.objectClientId;
    use->position = action.position;
    use->stackPosition = action.stackPosition;
    use->creatureId = creatureId;
    publish(use);
}

bool isMapLocated(const TGameAction& action)
{
    return action.objectClientId && action.position.isValid() && action.stackPosition >= 0
        && action.position.x != UINT16_MAX;
}

/// CIP : GameclientMessageUseOnCreature (object, its location, creature).
void useOnCreature(const TGameAction& action)
{
    if (!action.creatureId)
        return;
    if (isMapLocated(action)) {
        tibia::game::useMapObjectOnCreature(action.position, action.objectClientId, action.stackPosition, action.creatureId);
        return;
    }
    // Action-bar / inventory / type-only: inventory use by client id.
    if (action.objectClientId)
        tibia::game::useInventoryItemOnCreature(action.objectClientId, action.creatureId);
}

/// The field a crosshair spell aims at (CIP): a creature's, else
/// a world map field's; container and inventory targets have none.
Position targetPosition(const TGameAction& target)
{
    if (target.creatureId) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(target.creatureId))
                return row->position();
        }
        return {};
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
    if (!action.target || !action.objectClientId)
        return;
    if (const uint32_t creatureId = targetCreatureId(*action.target)) {
        if (isMapLocated(action))
            tibia::game::useMapObjectOnCreature(action.position, action.objectClientId, action.stackPosition, creatureId);
        else
            tibia::game::useInventoryItemOnCreature(action.objectClientId, creatureId);
        return;
    }
    Position toPos;
    uint16_t toId = 0;
    int toStack = -1;
    if (!targetMapObject(*action.target, toPos, toId, toStack))
        return;
    if (isMapLocated(action)) {
        tibia::game::useWithMapObject(action.position, action.objectClientId, action.stackPosition, toPos, toId, toStack);
        return;
    }
    tibia::game::useInventoryItemWithMapObject(action.objectClientId, toPos, toId, toStack);
}

void castAtTarget(const TGameAction& action)
{
    const Position position = targetPosition(*action.target);
    if (!position.isValid() || action.text.isEmpty())
        return;
    tibia::game::setNextTalkAim(position);
    tibia::game::setNextTalkAimMode(1);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(action.talkMode), 0, action.text.toStdString());
}

}

void say(const tibia::client::MessageMode mode, const QString& text, const int spellCastType)
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
            tibia::game::setNextTalkAimMode(3);
            break;
        }
    } else if (spell && spell->aimAtTarget) {
        // Wave/beam towards mouse or attack target (server needDirection + aim tile).
        switch (static_cast<CastType>(spellCastType)) {
        case CastType::AtMousePosition: {
            auto action = std::make_shared<TGameActionSpellAtMouseCursor>();
            action->text = text;
            action->isSpell = true;
            action->spellCastType = spellCastType;
            action->talkMode = mode;
            publish(action);
            return;
        }
        case CastType::AtTargetOrSelf: {
            const uint32_t targetId = tibia::game::attackingCreatureId();
            if (targetId) {
                if (creatures::TCreatureStorage* storage = creatures::TCreatureStorage::instance()) {
                    if (const auto creature = storage->creatureAt(targetId)) {
                        tibia::game::setNextTalkAim(creature->position());
                        tibia::game::setNextTalkAimMode(2);
                    }
                }
            } else {
                tibia::game::setNextTalkAimMode(3);
            }
            break;
        }
        default:
            break;
        }
    }
    tibia::game::talkChannel(mode, 0, text.toStdString());
}

TUseWithGameActionHandler::TUseWithGameActionHandler(QObject* parent)
    : cip::tibia::input::TUseWithGameActionHandler(parent)
{
    // CIP : a creature target becomes USE_ON_CREATURE, anything else USE_TWO_OBJECTS.
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
    // CIP (TCreaturesGameActionHandler): on the attack target, else a USE_WITH selection.
    setActionHandler(A::USE_ON_TARGET, [](const TGameAction& action) {
        const uint32_t targetId = tibia::game::attackingCreatureId();
        publishUseOn(action, targetId ? A::USE_ON_CREATURE : A::USE_WITH, targetId);
        return true;
    });
    // CIP : on the own character, if any.
    setActionHandler(A::USE_ON_YOURSELF, [](const TGameAction& action) {
        if (const uint32_t id = tibia::game::localPlayerId(); id)
            publishUseOn(action, A::USE_ON_CREATURE, id);
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
