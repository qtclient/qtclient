#include "container/containergameactionhandler.h"

#include "appearances/objectappearanceinstancetypeid.h"
#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"
#include "protocol/gameactioncontexthelper.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/game/thingsession_forward.h>

#include <QString>

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace tibia::container {

namespace {

using A = input::EGameAction;
using C = input::EGameActionContextType;
using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;

/// The stash actions take an object from the inventory, a container or the
/// depot search's depot and inbox lists.
bool isStashLocation(const input::TGameActionContextPtr& location)
{
    if (!location)
        return false;
    switch (location->gameActionContextType()) {
    case C::LOCATION_INVENTORY:
    case C::LOCATION_CONTAINER_WITH_SLOT:
    case C::LOCATION_DEPOT_SEARCH_DEPOT_ITEM:
    case C::LOCATION_DEPOT_SEARCH_INBOX_ITEM:
        return true;
    default:
        return false;
    }
}

/// The object instance an object context names: with its liquid or its tier.
TObjectAppearanceInstanceTypeID instanceTypeOf(const input::TGameActionContextPtr& object)
{
    if (const auto* liquid = input::gameActionContextAs<input::TLiquidObjectApperanceTypeGameActionContext>(object))
        return TObjectAppearanceInstanceTypeID::withLiquid(liquid->appearanceTypeId(), liquid->liquidType());
    if (const auto* upgradeable = input::gameActionContextAs<input::TUpgradeableObjectApperanceTypeGameActionContext>(object))
        return TObjectAppearanceInstanceTypeID::withTier(upgradeable->appearanceTypeId(), upgradeable->tier());
    return TObjectAppearanceInstanceTypeID::plain(input::appearanceTypeIdOf(object.get()));
}

const TAppearanceType* objectType(const uint16_t typeId)
{
    return tibia::game::rawThingType(typeId, ThingCategoryItem);
}

/// A stash message on an object: its place and its type.
::tibia::protobuf::protocol::GameclientMessageStashAction stashObjectMessage(const uint32_t action,
                                                                            const input::TObjectGameActionContext& object)
{
    ::tibia::protobuf::protocol::GameclientMessageStashAction message;
    message.set_action(action);
    const protocol::TGameActionContextHelper helper;
    if (object.location())
        helper.fillObjectPosition(object.location(), message.mutable_object_position());
    if (object.object())
        helper.fillObjectIdentifier(object.object(), message.mutable_object_identifier());
    return message;
}

/// CipSoft's stash message actions.
constexpr uint32_t StashStowItem = 0;
constexpr uint32_t StashStowContainerContent = 1;
constexpr uint32_t StashStowType = 2;
constexpr uint32_t StashRetrieve = 3;
constexpr uint32_t StashReopen = 4;

}

TContainerGameActionHandler::TContainerGameActionHandler(std::shared_ptr<TContainerStorage> containerStorage,
                                                         std::shared_ptr<TObjectCountStorage> objectCountStorage,
                                                         QObject* parent)
    : cip::tibia::container::TContainerGameActionHandler(parent)
    , m_containerStorage(std::move(containerStorage))
    , m_objectCountStorage(std::move(objectCountStorage))
{
}

TContainerGameActionHandler::~TContainerGameActionHandler() = default;

void TContainerGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case A::ADD_GUI_TUTORIAL_MARKER:
        handleAddGUITutorialMarkerGameAction(std::dynamic_pointer_cast<const input::TGameActionAddGUITutorialMarker>(pGameAction));
        break;
    case A::REMOVE_ALL_TUTORIAL_MARKERS:
        handleRemoveAllTutorialMarkersGameAction();
        break;
    case A::STASH_STOW_TYPE:
        handleStashStowTypeGameAction(std::dynamic_pointer_cast<const input::TGameActionStashStowType>(pGameAction));
        break;
    case A::STASH_STOW_CONTAINER_CONTENT:
        handleStowContainerContentGameAction(
            std::dynamic_pointer_cast<const input::TGameActionStashStowContainerContent>(pGameAction));
        break;
    case A::STASH_STOW_STACK:
        handleStashStowStackGameAction(std::dynamic_pointer_cast<const input::TGameActionStashStowStack>(pGameAction));
        break;
    case A::STASH_RETRIEVE:
        handleStashRetrieveGameAction(std::dynamic_pointer_cast<const input::TGameActionStashRetrieve>(pGameAction));
        break;
    case A::STASH_REQUEST_REOPEN:
        handleReqeustReopenStash();
        break;
    case A::DEPOT_SERACH_RETRIEVE:
        handleDepotSearchRetrieveGameAction(std::dynamic_pointer_cast<const input::TGameActionDepotSearchRetrieve>(pGameAction));
        break;
    case A::TRY_TO_EQUIP:
        handleTryToEquipGameAction(std::dynamic_pointer_cast<const input::TGameActionTryToEquip>(pGameAction));
        break;
    case A::OPEN_PARENT_CONTAINER:
        handleOpenParentContainerGameAction(std::dynamic_pointer_cast<const input::TGameActionOpenParentContainer>(pGameAction));
        break;
    case A::REQUEST_DEPOT_SEARCH:
        handleRequestDepotSearchGameAction();
        break;
    case A::DEPOT_SEARCH_CLOSED:
        handleDepotSearchClosedGameAction();
        break;
    case A::REQUEST_SEARCH_TYPE_IN_DEPOT:
        handleRequestSearchTypeInDepotGameAction(
            std::dynamic_pointer_cast<const input::TGameActionRequestSearchTypeInDepot>(pGameAction));
        break;
    case A::MOVE_OBJECT:
        handleMoveObjectGameAction(std::dynamic_pointer_cast<const input::TGameActionMoveObject>(pGameAction));
        break;
    default:
        break;
    }
}

void TContainerGameActionHandler::handleMoveObjectGameAction(const std::shared_ptr<const input::TGameActionMoveObject>& pGameActionMoveObject)
{
    if (!pGameActionMoveObject || !pGameActionMoveObject->destination())
        return;
    const input::TGameActionMoveObject& move = *pGameActionMoveObject;
    const Position& to = *move.destination();
    const input::TGameActionContextPtr destination = input::locationContextFromPosition(to, 0);
    const C destinationType = destination ? destination->gameActionContextType() : C::LOCATION_UNKNOWN;
    if (destinationType != C::LOCATION_INVENTORY && destinationType != C::LOCATION_CONTAINER_WITH_SLOT)
        return;

    // A container dropped onto the supply stash: ask before its content is stowed.
    if (!move.isStashConfirmed() && config::TClientOptions::instance()->boolOption(config::EClientOption::stashAskBeforeStowContainerContent)
        && destinationType == C::LOCATION_CONTAINER_WITH_SLOT) {
        const auto* slot = static_cast<const input::TLocationContainerWithSlotGameActionContext*>(destination.get());
        if (const TStoredContainer* container = m_containerStorage ? m_containerStorage->containerAt(slot->containerId()) : nullptr) {
            const TInventorySlot item = container->itemAt(static_cast<uint32_t>(slot->slot()));
            if (!item.empty()) {
                const TAppearanceType* type = objectType(move.objectClientId());
                if (item.typeId == tibia::game::supplyStashId() && type && type->isContainer()) {
                    Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowConfirmMoveObjectToStashDialog>(pGameActionMoveObject));
                    return;
                }
            }
        }
    }

    move.setStashConfirmed(true);
    ::tibia::protobuf::protocol::GameclientMessageMoveObject message;
    const Position from = move.position();
    const protocol::TGameActionContextHelper helper;
    if (move.location())
        helper.fillObjectPosition(move.location(), message.mutable_source_object_position());
    if (move.object())
        helper.fillObjectIdentifier(move.object(), message.mutable_object_identifier());
    helper.fillObjectPosition(destination, message.mutable_destination_object_position());
    message.set_count(static_cast<uint32_t>(input::moveObjectCount(move)));
    if (destinationType == C::LOCATION_CONTAINER_WITH_SLOT)
        message.set_extra_flag(static_cast<const input::TLocationContainerWithSlotGameActionContext*>(destination.get())->flag12());
    // An object put back where it lies is not sent.
    if (from == to)
        return;
    Q_EMIT sendMoveObject(message);
}

void TContainerGameActionHandler::handleTryToEquipGameAction(const std::shared_ptr<const input::TGameActionTryToEquip>& pGameActioTryToEquip)
{
    if (!pGameActioTryToEquip)
        return;
    const TObjectAppearanceInstanceTypeID object = instanceTypeOf(pGameActioTryToEquip->object());
    TObjectAppearanceInstanceTypeID equip = object;
    const TAppearanceType* type = objectType(object.typeId);
    if (type && type->expireCounterpart() && pGameActioTryToEquip->smartMode()) {
        // A ring or an amulet in use is its expire counterpart: equip the one
        // worn, else the one the player has.
        const uint16_t counterpartId = *type->expireCounterpart();
        const TAppearanceType* counterpartType = objectType(counterpartId);
        TObjectAppearanceInstanceTypeID counterpart = TObjectAppearanceInstanceTypeID::plain(counterpartId);
        if (counterpartType && (counterpartType->isSplash() || counterpartType->isFluidContainer()))
            counterpart = TObjectAppearanceInstanceTypeID::withLiquid(counterpartId, object.liquid);
        else if (counterpartType && counterpartType->hasUpgradeClassification())
            counterpart = TObjectAppearanceInstanceTypeID::withTier(counterpartId, object.tier);
        const std::optional<uint8_t> tier =
            type->hasUpgradeClassification() ? std::optional<uint8_t>(object.tier) : std::nullopt;
        const TInventoryContainer* inventory = TInventoryContainer::instance();
        const auto count = [this](const TObjectAppearanceInstanceTypeID& instance) {
            return m_objectCountStorage ? m_objectCountStorage->count(instance.typeId, instance.tier) : 0u;
        };
        if (inventory && inventory->isObjectEquipped(object.typeId, tier))
            equip = object;
        else if (inventory && inventory->isObjectEquipped(counterpartId, tier))
            equip = counterpart;
        else if (count(object) != 0)
            equip = object;
        else if (count(counterpart) != 0)
            equip = counterpart;
    }
    if (!m_objectCountStorage || m_objectCountStorage->count(equip.typeId, equip.tier) == 0)
        return;
    ::tibia::protobuf::protocol::GameclientMessageEquipObject message;
    message.set_object_id(equip.typeId);
    message.set_tier(equip.hasLiquid() ? equip.liquid : equip.hasTier() ? equip.tier : 0);
    Q_EMIT sendEquipObject(message);
}

void TContainerGameActionHandler::handleRemoveAllTutorialMarkersGameAction()
{
    if (m_containerStorage)
        m_containerStorage->clearAllTutorialMarkers();
}

void TContainerGameActionHandler::handleAddGUITutorialMarkerGameAction(
    const std::shared_ptr<const input::TGameActionAddGUITutorialMarker>& pAddGUITutorialMarkerGameAction)
{
    if (!pAddGUITutorialMarkerGameAction || !m_containerStorage)
        return;
    const input::TGameActionContextPtr location =
        input::locationContextFromPosition(pAddGUITutorialMarkerGameAction->position(), -1);
    if (const auto* inventory = input::gameActionContextAs<input::TLocationInventoryGameActionContext>(location))
        m_containerStorage->setInventoryTutorialMarker(pAddGUITutorialMarkerGameAction->markerId(), inventory->slot());
    else if (const auto* slot = input::gameActionContextAs<input::TLocationContainerWithSlotGameActionContext>(location))
        m_containerStorage->setContainerTutorialMarker(pAddGUITutorialMarkerGameAction->markerId(), slot->containerId(), slot->slot());
}

void TContainerGameActionHandler::handleStashStowTypeGameAction(
    const std::shared_ptr<const input::TGameActionStashStowType>& pStashStowTypeGameAction)
{
    if (!pStashStowTypeGameAction || !isStashLocation(pStashStowTypeGameAction->location()))
        return;
    Q_EMIT sendStashAction(stashObjectMessage(StashStowType, pStashStowTypeGameAction->context()));
}

void TContainerGameActionHandler::handleStowContainerContentGameAction(
    const std::shared_ptr<const input::TGameActionStashStowContainerContent>& pStowContainerContentGameAction)
{
    if (!pStowContainerContentGameAction || !isStashLocation(pStowContainerContentGameAction->location()))
        return;
    if (!pStowContainerContentGameAction->isStashConfirmed()
        && config::TClientOptions::instance()->boolOption(config::EClientOption::stashAskBeforeStowContainerContent)) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowConfirmMoveObjectToStashDialog>(pStowContainerContentGameAction));
        return;
    }
    pStowContainerContentGameAction->setStashConfirmed(true);
    Q_EMIT sendStashAction(stashObjectMessage(StashStowContainerContent, pStowContainerContentGameAction->context()));
}

void TContainerGameActionHandler::handleStashStowStackGameAction(
    const std::shared_ptr<const input::TGameActionStashStowStack>& pStashStowStackGameAction)
{
    if (!pStashStowStackGameAction || !isStashLocation(pStashStowStackGameAction->location()))
        return;
    ::tibia::protobuf::protocol::GameclientMessageStashAction message =
        stashObjectMessage(StashStowItem, pStashStowStackGameAction->context());
    // CipSoft's count is a byte.
    message.set_count(static_cast<uint8_t>(pStashStowStackGameAction->amount()));
    Q_EMIT sendStashAction(message);
}

void TContainerGameActionHandler::handleStashRetrieveGameAction(
    const std::shared_ptr<const input::TGameActionStashRetrieve>& pStashRetrieveGameAction)
{
    if (!pStashRetrieveGameAction)
        return;
    ::tibia::protobuf::protocol::GameclientMessageStashAction message;
    message.set_action(StashRetrieve);
    message.mutable_object_identifier()->set_type_id(pStashRetrieveGameAction->typeId());
    message.set_count(static_cast<uint32_t>(pStashRetrieveGameAction->amount()));
    // The retrieve source (0 or 1; CipSoft throws on any other); qtclient's
    // message carries it in the stack position.
    const int source = pStashRetrieveGameAction->source();
    if (source < 0 || source > 1)
        throw std::runtime_error(QStringLiteral("EStashRetrieveSource %1 couldn't be converted").arg(source).toStdString());
    message.set_stack_position(static_cast<uint32_t>(source));
    Q_EMIT sendStashAction(message);
}

void TContainerGameActionHandler::handleReqeustReopenStash()
{
    ::tibia::protobuf::protocol::GameclientMessageStashAction message;
    message.set_action(StashReopen);
    Q_EMIT sendStashAction(message);
}

void TContainerGameActionHandler::handleRequestDepotSearchGameAction()
{
    Q_EMIT sendOpenDepotSearch(::tibia::protobuf::protocol::GameclientMessageOpenDepotSearch());
}

void TContainerGameActionHandler::handleDepotSearchClosedGameAction()
{
    Q_EMIT sendCloseDepotSearch(::tibia::protobuf::protocol::GameclientMessageCloseDepotSearch());
}

void TContainerGameActionHandler::handleRequestSearchTypeInDepotGameAction(
    const std::shared_ptr<const input::TGameActionRequestSearchTypeInDepot>& pRequestSearchTypeInDepotGameAction)
{
    if (!pRequestSearchTypeInDepotGameAction)
        return;
    ::tibia::protobuf::protocol::GameclientMessageDepotSearchType message;
    message.set_object_id(pRequestSearchTypeInDepotGameAction->typeId());
    if (const std::optional<uint8_t> tier = pRequestSearchTypeInDepotGameAction->tier())
        message.set_tier(*tier);
    Q_EMIT sendDepotSearchType(message);
}

void TContainerGameActionHandler::handleOpenParentContainerGameAction(
    const std::shared_ptr<const input::TGameActionOpenParentContainer>& pOpenParentContainerGameAction)
{
    if (!pOpenParentContainerGameAction)
        return;
    ::tibia::protobuf::protocol::GameclientMessageOpenParentContainer message;
    if (const input::TGameActionContextPtr location =
            input::locationContextFromPosition(pOpenParentContainerGameAction->position(), -1))
        protocol::TGameActionContextHelper().fillObjectPosition(location, message.mutable_object_position());
    Q_EMIT sendOpenParentContainer(message);
}

void TContainerGameActionHandler::handleDepotSearchRetrieveGameAction(
    const std::shared_ptr<const input::TGameActionDepotSearchRetrieve>& pDepotSearchRetrieveGameAction)
{
    if (!pDepotSearchRetrieveGameAction)
        return;
    ::tibia::protobuf::protocol::GameclientMessageDepotSearchRetrieve message;
    message.set_object_id(pDepotSearchRetrieveGameAction->typeId());
    if (const std::optional<uint8_t> tier = pDepotSearchRetrieveGameAction->tier())
        message.set_tier(*tier);
    // The protocol knows the depot (1) and the inbox (2); any other location is
    // a programming error.
    const int location = pDepotSearchRetrieveGameAction->location();
    if (location != 1 && location != 2)
        throw std::runtime_error("unhandled enum value");
    message.set_retrieve_type(static_cast<uint32_t>(location));
    Q_EMIT sendDepotSearchRetrieve(message);
}

} // namespace tibia::container
