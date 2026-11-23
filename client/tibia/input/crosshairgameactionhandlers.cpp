#include "crosshairgameactionhandlers.h"

#include "chat/chatchannel.h"
#include "gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <creatures/creaturestorage.h>
#include <array>
#include <functional>
#include <utility>

#include <QPointer>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;
using CastType = cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;


/// The classic position and stack position of an object's location; no
/// location is anywhere the player has the object (as TLocationUnknownContext).
void classicLocation(const TObjectGameActionContext& object, Position& position, int& stackPosition)
{
    if (!object.location()) {
        position = Position(UINT16_MAX, 0, 0);
        stackPosition = 0;
        return;
    }
    position = object.position();
    stackPosition = object.stackPosition();
}

void setCoordinate(tibia::protobuf::protocol::ProtocolCoordinate* out, const Position& position)
{
    out->set_x(position.x);
    out->set_y(position.y);
    out->set_z(position.z);
}

/// CIP's location kinds a use with can target: the inventory, a container
/// slot, a world map object, a depot search item.
bool isUseTwoObjectsTarget(const EGameActionContextType type)
{
    switch (type) {
    case EGameActionContextType::LOCATION_INVENTORY:
    case EGameActionContextType::LOCATION_CONTAINER_WITH_SLOT:
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_DEPOT_ITEM:
    case EGameActionContextType::LOCATION_DEPOT_SEARCH_INBOX_ITEM:
        return true;
    default:
        return false;
    }
}

}

TUseWithGameActionHandler::TUseWithGameActionHandler(QObject* parent)
    : cip::tibia::input::TUseWithGameActionHandler(parent)
{
}

TUseWithGameActionHandler::~TUseWithGameActionHandler() = default;

void TUseWithGameActionHandler::handleGameAction(const IGameActionPtr pGameAction)
{
    switch (pGameAction->gameActionType()) {
    case A::USE_WITH:
        handleUseWith(std::static_pointer_cast<const TGameActionUseWith>(pGameAction));
        return;
    case A::USE_ON_CREATURE:
        handleUseOnCreature(static_cast<const TGameActionUseOnCreature&>(*pGameAction));
        return;
    case A::TARGET_SELECTED:
        handleTargetSelected(std::static_pointer_cast<const TGameActionTargetSelected>(pGameAction));
        return;
    case A::CANCEL_USE_WITH:
        m_pPendingAction.reset();
        return;
    case A::USE_ON_TARGET:
        // CIP (TCreaturesGameActionHandler): on the attack target, else a USE_WITH selection.
        if (const uint32_t targetId = tibia::game::attackingCreatureId())
            Q_EMIT publishGameAction(std::make_shared<TGameActionUseOnCreature>(
                static_cast<const TObjectGameAction&>(*pGameAction).context(), targetId));
        else
            Q_EMIT publishGameAction(std::make_shared<TGameActionUseWith>(static_cast<const TObjectGameAction&>(*pGameAction).context()));
        return;
    case A::USE_ON_YOURSELF:
        // CIP (TCreaturesGameActionHandler): on the own character, if any.
        if (const uint32_t id = tibia::game::localPlayerId(); id)
            Q_EMIT publishGameAction(
                std::make_shared<TGameActionUseOnCreature>(static_cast<const TObjectGameAction&>(*pGameAction).context(), id));
        return;
    default:
        return;
    }
}

void TUseWithGameActionHandler::handleUseWith(const std::shared_ptr<const TGameActionUseWith>& action)
{
    if (!action->hasTarget()) {
        // The target is selected first; a pending crosshair spell gives way.
        Q_EMIT publishGameAction(makeGameAction(A::CANCEL_CROSSHAIR_SPELL));
        startTargetSelectionBridge();
        m_pPendingAction = action;
        Q_EMIT publishGameAction(std::make_shared<TGameActionStartTargetSelection>());
        return;
    }
    const TObjectGameActionContext& target = *action->target();
    if (!isUseTwoObjectsTarget(target.locationType()))
        return;
    tibia::protobuf::protocol::GameclientMessageUseTwoObjects message;
    Position position;
    int stackPosition = 0;
    classicLocation(action->context(), position, stackPosition);
    setCoordinate(message.mutable_from_position(), position);
    message.set_from_stack_position(static_cast<uint32_t>(std::max(stackPosition, 0)));
    message.set_object_id(action->objectClientId());
    classicLocation(target, position, stackPosition);
    setCoordinate(message.mutable_to_position(), position);
    message.set_to_stack_position(static_cast<uint32_t>(std::max(stackPosition, 0)));
    message.set_to_object_id(target.objectClientId());
    Q_EMIT sendUseTwoObjects(message);
}

void TUseWithGameActionHandler::handleUseOnCreature(const TGameActionUseOnCreature& action)
{
    tibia::protobuf::protocol::GameclientMessageUseOnCreature message;
    Position position;
    int stackPosition = 0;
    classicLocation(action.context(), position, stackPosition);
    setCoordinate(message.mutable_position(), position);
    message.set_stack_position(static_cast<uint32_t>(std::max(stackPosition, 0)));
    message.set_object_id(action.objectClientId());
    message.set_creature_id(action.targetCreatureId());
    Q_EMIT sendUseOnCreature(message);
}

void TUseWithGameActionHandler::handleTargetSelected(const TTargetSelectedPtr& target)
{
    // qtclient's callback selections (loot containers, trade, ...) end here too.
    if (isSelectingTarget())
        finishTargetSelection(target);
    std::shared_ptr<const TGameActionUseWith> pending = std::exchange(m_pPendingAction, nullptr);
    if (!pending || !target)
        return;
    if (const uint32_t creatureId = target->context().creatureId()) {
        Q_EMIT publishGameAction(std::make_shared<TGameActionUseOnCreature>(pending->context(), creatureId));
        return;
    }
    Q_EMIT publishGameAction(std::make_shared<TGameActionUseWith>(pending->context(), target->context()));
}

void TUseWithGameActionHandler::startTargetSelectionBridge()
{
    // The world map hands its click to the selection callback; it comes back
    // as the TARGET_SELECTED every window publishes.
    // A selection that ends without a target drops the pending use with.
    QPointer<TUseWithGameActionHandler> self(this);
    ::tibia::input::startTargetSelection([self](const TTargetSelectedPtr& target) {
        if (!self)
            return;
        if (target)
            Q_EMIT self->publishGameAction(target);
        else
            self->m_pPendingAction.reset();
    });
}

TCrossHairSpellGameActionHandler::TCrossHairSpellGameActionHandler(QObject* parent)
    : cip::tibia::input::TCrossHairSpellGameActionHandler(parent)
{
}

TCrossHairSpellGameActionHandler::~TCrossHairSpellGameActionHandler() = default;

void TCrossHairSpellGameActionHandler::handleGameAction(const IGameActionPtr pGameAction)
{
    switch (pGameAction->gameActionType()) {
    case A::CROSSHAIR_SPELL:
        handleCrossHairSpell(std::static_pointer_cast<const TGameActionCrossHairSpell>(pGameAction));
        return;
    case A::TARGET_SELECTED:
        handleTargetSelected(std::static_pointer_cast<const TGameActionTargetSelected>(pGameAction));
        return;
    case A::CANCEL_CROSSHAIR_SPELL:
        m_pPendingAction.reset();
        return;
    default:
        return;
    }
}

void TCrossHairSpellGameActionHandler::handleCrossHairSpell(const std::shared_ptr<const TGameActionCrossHairSpell>& action)
{
    if (!action->target()) {
        // The target is selected first; a pending use with gives way. The
        // world map's click comes back as a TARGET_SELECTED; a selection that
        // ends without one drops the spell.
        Q_EMIT publishGameAction(makeGameAction(A::CANCEL_USE_WITH));
        QPointer<TCrossHairSpellGameActionHandler> self(this);
        ::tibia::input::startTargetSelection([self](const TTargetSelectedPtr& target) {
            if (!self)
                return;
            if (target)
                Q_EMIT self->publishGameAction(target);
            else
                self->m_pPendingAction.reset();
        });
        m_pPendingAction = action;
        Q_EMIT publishGameAction(std::make_shared<TGameActionStartTargetSelection>());
        return;
    }
    TObjectGameActionContext target = *action->target();
    switch (target.locationType()) {
    case EGameActionContextType::LOCATION_WORLD_MAP_COORDINATE_WITH_STACK_POSITION:
        break;
    case EGameActionContextType::LOCATION_BATTLE_LIST:
        if (target.object() && target.object()->gameActionContextType() == EGameActionContextType::OBJECT_CREATURE) {
            tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance();
            if (const std::shared_ptr<tibia::creatures::TCreature> creature = storage ? storage->creatureAt(target.creatureId()) : nullptr)
                target = TObjectGameActionContext(std::make_shared<TLocationWorldMapCoordinateContext>(creature->position()), target.object());
        }
        break;
    default:
        return;
    }
    tibia::protobuf::protocol::GameclientMessageTalk message;
    message.set_text(action->words().toStdString());
    message.set_aim_mode(1);
    message.set_mode(chat::protocolMessageMode(action->talkMode()));
    setCoordinate(message.mutable_aim_position(), target.position());
    Q_EMIT sendTalkMessage(message);
}

void TCrossHairSpellGameActionHandler::handleTargetSelected(const TTargetSelectedPtr& target)
{
    std::shared_ptr<const TGameActionCrossHairSpell> pending = std::exchange(m_pPendingAction, nullptr);
    if (!pending || !target)
        return;
    Q_EMIT publishGameAction(std::make_shared<TGameActionCrossHairSpell>(
        pending->spellId(), pending->words(), pending->talkMode(), pending->spellCastType(), target->context()));
}

}
