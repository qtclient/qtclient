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


bool isMapLocated(const TObjectGameActionContext& object)
{
    return object.objectClientId() && object.position().isValid() && object.stackPosition() >= 0 && object.position().x != UINT16_MAX;
}

/// CIP : GameclientMessageUseOnCreature (object, its location, creature).
void useOnCreature(const TObjectGameActionContext& object, const uint32_t creatureId)
{
    if (!creatureId)
        return;
    if (isMapLocated(object)) {
        tibia::game::useMapObjectOnCreature(object.position(), object.objectClientId(), object.stackPosition(), creatureId);
        return;
    }
    // Action-bar / inventory / type-only: inventory use by client id.
    if (object.objectClientId())
        tibia::game::useInventoryItemOnCreature(object.objectClientId(), creatureId);
}

/// The field a crosshair spell aims at (CIP): a creature's, else
/// a world map field's; container and inventory targets have none.
Position targetPosition(const TObjectGameActionContext& target)
{
    if (target.creatureId()) {
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const std::shared_ptr<tibia::creatures::TCreature> row = storage->creatureAt(target.creatureId()))
                return row->position();
        }
        return {};
    }
    if (target.position().isValid() && target.position().x != UINT16_MAX)
        return target.position();
    return {};
}

/// Selects a target, then hands `publish` the action `withTarget` builds for it.
void selectTargetFor(std::function<IGameActionPtr(const TObjectGameActionContext& target)> withTarget,
                     std::function<void(IGameActionPtr)> publish)
{
    startTargetSelection([withTarget = std::move(withTarget), publish = std::move(publish)](const TTargetSelectedPtr& target) {
        if (target)
            publish(withTarget(target->context()));
    });
}

/// CIP : GameclientMessageUseTwoObjects (object and target, each with its location).
void useWith(const TObjectGameActionContext& object, const TObjectGameActionContext& target)
{
    if (!object.objectClientId() || target.creatureId() || !target.objectClientId() || !target.position().isValid()
        || target.stackPosition() < 0)
        return;
    if (isMapLocated(object)) {
        tibia::game::useWithMapObject(object.position(), object.objectClientId(), object.stackPosition(), target.position(),
                                      target.objectClientId(), target.stackPosition());
        return;
    }
    tibia::game::useInventoryItemWithMapObject(object.objectClientId(), target.position(), target.objectClientId(), target.stackPosition());
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
}

TUseWithGameActionHandler::~TUseWithGameActionHandler() = default;

void TUseWithGameActionHandler::handleGameAction(const IGameActionPtr pGameAction)
{
    const auto emitAction = [this](IGameActionPtr action) { Q_EMIT publishGameAction(std::move(action)); };
    switch (pGameAction->gameActionType()) {
    case A::USE_WITH: {
        // CIP : without a target the target is selected first; a creature target
        // becomes USE_ON_CREATURE, anything else GameclientMessageUseTwoObjects.
        const auto& action = static_cast<const TGameActionUseWith&>(*pGameAction);
        if (!action.hasTarget()) {
            selectTargetFor(
                [object = action.context()](const TObjectGameActionContext& target) {
                    return std::make_shared<TGameActionUseWith>(object, target);
                },
                emitAction);
        } else if (const uint32_t creatureId = action.target()->creatureId()) {
            Q_EMIT publishGameAction(std::make_shared<TGameActionUseOnCreature>(action.context(), creatureId));
        } else {
            useWith(action.context(), *action.target());
        }
        return;
    }
    case A::USE_ON_CREATURE: {
        const auto& action = static_cast<const TGameActionUseOnCreature&>(*pGameAction);
        useOnCreature(action.context(), action.targetCreatureId());
        return;
    }
    case A::USE_ON_TARGET:
        // CIP (TCreaturesGameActionHandler): on the attack target, else a USE_WITH selection.
        if (const uint32_t targetId = tibia::game::attackingCreatureId())
            Q_EMIT publishGameAction(std::make_shared<TGameActionUseOnCreature>(
                static_cast<const TObjectGameAction&>(*pGameAction).context(), targetId));
        else
            Q_EMIT publishGameAction(std::make_shared<TGameActionUseWith>(static_cast<const TObjectGameAction&>(*pGameAction).context()));
        return;
    case A::USE_ON_YOURSELF:
        // CIP : on the own character, if any.
        if (const uint32_t id = tibia::game::localPlayerId(); id)
            Q_EMIT publishGameAction(
                std::make_shared<TGameActionUseOnCreature>(static_cast<const TObjectGameAction&>(*pGameAction).context(), id));
        return;
    case A::TARGET_SELECTED:
        // Containers, the inventory, the battle list and the map publish the
        // TARGET_SELECTED under a selection's click; the pending action takes it.
        if (isSelectingTarget())
            finishTargetSelection(std::static_pointer_cast<const TGameActionTargetSelected>(pGameAction));
        return;
    default:
        return;
    }
}

TCrossHairSpellGameActionHandler::TCrossHairSpellGameActionHandler(QObject* parent)
    : cip::tibia::input::TCrossHairSpellGameActionHandler(parent)
{
}

TCrossHairSpellGameActionHandler::~TCrossHairSpellGameActionHandler() = default;

void TCrossHairSpellGameActionHandler::handleGameAction(const IGameActionPtr pGameAction)
{
    if (pGameAction->gameActionType() != A::CROSSHAIR_SPELL)
        return;
    const auto& action = static_cast<const TGameActionCrossHairSpell&>(*pGameAction);
    if (action.target()) {
        castAtTarget(action, *action.target());
        return;
    }
    selectTargetFor(
        [spellId = action.spellId(), words = action.words(), talkMode = action.talkMode(),
         castType = action.spellCastType()](const TObjectGameActionContext& target) {
            return std::make_shared<TGameActionCrossHairSpell>(spellId, words, talkMode, castType, target);
        },
        [this](IGameActionPtr spell) { Q_EMIT publishGameAction(std::move(spell)); });
}

}
