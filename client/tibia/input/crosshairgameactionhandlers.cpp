#include "crosshairgameactionhandlers.h"

#include "gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <creatures/creaturestorage.h>
#include <array>
#include <functional>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;
using CastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

constexpr std::array<A, 5> UseWithActions{ A::USE_WITH, A::USE_ON_CREATURE, A::USE_ON_TARGET, A::USE_ON_YOURSELF, A::TARGET_SELECTED };
constexpr std::array<A, 1> CrossHairSpellActions{ A::CROSSHAIR_SPELL };

bool isMapLocated(const TObjectGameActionContext& object)
{
    return object.objectClientId && object.position.isValid() && object.stackPosition >= 0 && object.position.x != UINT16_MAX;
}

/// CIP : GameclientMessageUseOnCreature (object, its location, creature).
void useOnCreature(const TObjectGameActionContext& object, const uint32_t creatureId)
{
    if (!creatureId)
        return;
    if (isMapLocated(object)) {
        tibia::game::useMapObjectOnCreature(object.position, object.objectClientId, object.stackPosition, creatureId);
        return;
    }
    // Action-bar / inventory / type-only: inventory use by client id.
    if (object.objectClientId)
        tibia::game::useInventoryItemOnCreature(object.objectClientId, creatureId);
}

/// The field a crosshair spell aims at (CIP): a creature's, else
/// a world map field's; container and inventory targets have none.
Position targetPosition(const TObjectGameActionContext& target)
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

/// Selects a target, then publishes the action `withTarget` builds for it.
void selectTargetFor(std::function<IGameActionPtr(const TObjectGameActionContext& target)> withTarget)
{
    startTargetSelection([withTarget = std::move(withTarget)](const TTargetSelectedPtr& target) {
        if (target)
            publish(withTarget(target->context()));
    });
}

/// CIP : GameclientMessageUseTwoObjects (object and target, each with its location).
void useWith(const TObjectGameActionContext& object, const TObjectGameActionContext& target)
{
    if (!object.objectClientId || target.creatureId || !target.objectClientId || !target.position.isValid()
        || target.stackPosition < 0)
        return;
    if (isMapLocated(object)) {
        tibia::game::useWithMapObject(object.position, object.objectClientId, object.stackPosition, target.position,
                                      target.objectClientId, target.stackPosition);
        return;
    }
    tibia::game::useInventoryItemWithMapObject(object.objectClientId, target.position, target.objectClientId, target.stackPosition);
}

void castAtTarget(const TGameActionTargetedSpell& spell, const TObjectGameActionContext& target)
{
    const Position position = targetPosition(target);
    if (!position.isValid() || spell.words().isEmpty())
        return;
    tibia::game::setNextTalkAim(position);
    tibia::game::setNextTalkAimMode(1);
    tibia::game::talkChannel(static_cast<tibia::client::MessageMode>(spell.talkMode()), 0, spell.words().toStdString());
}

}

void say(const tibia::client::MessageMode mode, const QString& text, const int spellCastType)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spellOfText(text);
    if (spell && spell->crossHairTarget) {
        switch (static_cast<CastType>(spellCastType)) {
        case CastType::AtMousePosition:
        case CastType::None:
        case CastType::Crosshair:
            if (static_cast<CastType>(spellCastType) == CastType::AtMousePosition)
                publish(std::make_shared<TGameActionSpellAtMouseCursor>(static_cast<uint16_t>(spell->id), text, mode, spellCastType));
            else
                publish(std::make_shared<TGameActionCrossHairSpell>(static_cast<uint16_t>(spell->id), text, mode, spellCastType));
            return;
        case CastType::AtTargetOrSelf:
            tibia::game::setNextTalkAimMode(3);
            break;
        }
    } else if (spell && spell->aimAtTarget) {
        // Wave/beam towards mouse or attack target (server needDirection + aim tile).
        switch (static_cast<CastType>(spellCastType)) {
        case CastType::AtMousePosition:
            publish(std::make_shared<TGameActionSpellAtMouseCursor>(static_cast<uint16_t>(spell->id), text, mode, spellCastType));
            return;
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
    // CIP : without a target the target is selected first; a creature target
    // becomes USE_ON_CREATURE, anything else GameclientMessageUseTwoObjects.
    setActionHandler(A::USE_WITH, [](const TGameActionUseWith& action) {
        if (!action.hasTarget()) {
            selectTargetFor([object = action.context()](const TObjectGameActionContext& target) {
                return std::make_shared<TGameActionUseWith>(object, target);
            });
        } else if (const uint32_t creatureId = action.target()->creatureId) {
            publish(std::make_shared<TGameActionUseOnCreature>(action.context(), creatureId));
        } else {
            useWith(action.context(), *action.target());
        }
        return true;
    });
    setActionHandler(A::USE_ON_CREATURE, [](const TGameActionUseOnCreature& action) {
        useOnCreature(action.context(), action.targetCreatureId());
        return true;
    });
    // CIP (TCreaturesGameActionHandler): on the attack target, else a USE_WITH selection.
    setActionHandler(A::USE_ON_TARGET, [](const TGameActionUseOnTarget& action) {
        if (const uint32_t targetId = tibia::game::attackingCreatureId())
            publish(std::make_shared<TGameActionUseOnCreature>(action.context(), targetId));
        else
            publish(std::make_shared<TGameActionUseWith>(action.context()));
        return true;
    });
    // CIP : on the own character, if any.
    setActionHandler(A::USE_ON_YOURSELF, [](const TGameActionUseOnYourself& action) {
        if (const uint32_t id = tibia::game::localPlayerId(); id)
            publish(std::make_shared<TGameActionUseOnCreature>(action.context(), id));
        return true;
    });
    // Containers, the inventory, the battle list and the map publish the
    // TARGET_SELECTED under a selection's click; the pending action takes it.
    setActionHandler(A::TARGET_SELECTED, [](const TGameActionTargetSelected& action) {
        if (isSelectingTarget())
            finishTargetSelection(std::make_shared<TGameActionTargetSelected>(action.context()));
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
    setActionHandler(A::CROSSHAIR_SPELL, [](const TGameActionCrossHairSpell& action) {
        if (action.target()) {
            castAtTarget(action, *action.target());
            return true;
        }
        selectTargetFor([spellId = action.spellId(), words = action.words(), talkMode = action.talkMode(),
                         castType = action.spellCastType()](const TObjectGameActionContext& target) {
            return std::make_shared<TGameActionCrossHairSpell>(spellId, words, talkMode, castType, target);
        });
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
