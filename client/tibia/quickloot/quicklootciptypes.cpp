#include "quickloot/quicklootciptypes.h"
#include "quickloot/managedcontainerstorage.h"
#include <protocol.pb.h>

#include <algorithm>
#include <vector>
#include <stdexcept>
#include <tibia/game/gamesession.h>
#include "protocol/protocolciptypes.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "config/clientoptions.h"
#include "config/optionspages.h"
#include "analysers/analyserstorages.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "input/gameactionhandlers.h"

namespace tibia::quickloot {

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    new TQuickLootGameActionHandler(parent);
}

namespace {

/// The categories the server may assign containers to.
bool isObjectCategory(const uint32_t category) { return category - 1u < 31u; }

}

void TManagedContainersProtocolMessageHandler::setLootContainerFallback(const bool useMainBackpack)
{
    tibia::game::openContainerQuickLoot(3, 0, {}, 0, 0, useMainBackpack);
}

void TManagedContainersProtocolMessageHandler::quickLoot(const Position& position, const uint16_t objectId, const int stackPosition,
                                                         const bool allCorpsesInArea)
{
    // Looting a corpse on the map loots every corpse around it when the option asks for it.
    const bool onMap = position.x != 0xFFFF;
    tibia::game::sendQuickLootAt(onMap && allCorpsesInArea ? 1 : 0, position, objectId, stackPosition);
}

void TManagedContainersProtocolMessageHandler::sendQuickLootBlackWhitelist(const int listType, const std::set<uint16_t>& objectTypeIds)
{
    if (static_cast<unsigned>(listType) >= 2)
        throw std::runtime_error("unhandled enum value");
    const std::vector<uint16_t> items(objectTypeIds.begin(), objectTypeIds.end());
    tibia::game::requestQuickLootBlackWhiteList(static_cast<uint8_t>(listType), static_cast<uint16_t>(items.size()), items);
}

void TManagedContainersProtocolMessageHandler::quickLootAreaAtPlayer() { tibia::game::sendQuickLootAt(2, Position(0, 0, 0), 0, 0); }

void TManagedContainersProtocolMessageHandler::sendManagedContainer(const EManagedContainerAction action, const int category,
                                                                    const Position& position, const uint16_t objectId,
                                                                    const int stackPosition)
{
    if (!isObjectCategory(static_cast<uint32_t>(category)))
        return;
    tibia::game::openContainerQuickLoot(static_cast<uint8_t>(action), static_cast<uint8_t>(category), position, objectId,
                                        static_cast<uint8_t>(std::max(0, stackPosition)), false);
}

namespace {

using A = input::EGameAction;
using P = TManagedContainersProtocolMessageHandler;

TQuickLootBlackWhitelistStorage* lootBlackWhitelist()
{
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    return storages ? storages->lootBlackWhitelist() : nullptr;
}

constexpr A QuickLootActions[] = { A::QUICK_LOOT, A::QUICK_LOOT_AREA_AT_PLAYER, A::LOOT_CONTAINER_SELECT, A::LOOT_CONTAINER_CLEAR,
                                   A::LOOT_CONTAINER_OPEN, A::OBTAIN_CONTAINER_SELECT, A::OBTAIN_CONTAINER_CLEAR, A::OBTAIN_CONTAINER_OPEN,
                                   A::LOOT_CONTAINER_FALLBACK, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST };

bool quickLootAllCorpsesInArea()
{
    return config::TClientOptions::instance()->boolOption(cip::EClientOption::quickLootAllCorpsesInAreaEnabled);
}

}

TQuickLootGameActionHandler::TQuickLootGameActionHandler(QObject* parent)
    : cip::tibia::quickloot::TQuickLootGameActionHandler(parent)
{
    for (const A type : QuickLootActions)
        input::setActionHandler(type, [](const input::IGameAction& action) { return handleGameAction(action); });
}

TQuickLootGameActionHandler::~TQuickLootGameActionHandler()
{
    for (const A type : QuickLootActions)
        input::setActionHandler(type, nullptr);
}

bool TQuickLootGameActionHandler::handleGameAction(const input::IGameAction& pGameAction)
{
    const auto* listStatus = input::gameActionAs<input::TGameActionToggleBlackWhitelistStatus>(&pGameAction);
    switch (pGameAction.gameActionType()) {
    case A::QUICK_LOOT:
        if (const auto* loot = input::gameActionAs<input::TGameActionQuickLoot>(&pGameAction))
            P::quickLoot(loot->position(), loot->objectClientId(), loot->stackPosition(), quickLootAllCorpsesInArea());
        return true;
    case A::QUICK_LOOT_AREA_AT_PLAYER:
        P::quickLootAreaAtPlayer();
        return true;
    case A::LOOT_CONTAINER_SELECT:
    case A::OBTAIN_CONTAINER_SELECT:
        if (const auto* select = input::gameActionAs<input::TGameActionSelectManagedContainer>(&pGameAction)) {
            const input::TObjectGameActionContext& container = select->container();
            P::sendManagedContainer(pGameAction.gameActionType() == A::LOOT_CONTAINER_SELECT ? P::SelectLootContainer
                                                                                              : P::SelectObtainContainer,
                                    select->category(), container.position(), container.objectClientId(), container.stackPosition());
        }
        return true;
    case A::LOOT_CONTAINER_CLEAR:
    case A::LOOT_CONTAINER_OPEN:
    case A::OBTAIN_CONTAINER_CLEAR:
    case A::OBTAIN_CONTAINER_OPEN:
        if (const auto* managed = input::gameActionAs<input::TGameActionManagedContainer>(&pGameAction)) {
            const A type = pGameAction.gameActionType();
            P::sendManagedContainer(type == A::LOOT_CONTAINER_CLEAR   ? P::ClearLootContainer
                                    : type == A::LOOT_CONTAINER_OPEN  ? P::OpenLootContainer
                                    : type == A::OBTAIN_CONTAINER_CLEAR ? P::ClearObtainContainer
                                                                        : P::OpenObtainContainer,
                                    managed->category());
        }
        return true;
    case A::LOOT_CONTAINER_FALLBACK:
        if (const auto* fallback = input::gameActionAs<input::TGameActionSetLootContainerFallback>(&pGameAction))
            P::setLootContainerFallback(fallback->useMainContainerAsFallback());
        return true;
    case A::LOOT_ADD_TO_BLACKWHITELIST:
        if (listStatus)
            handleAddToBlackWhitelistStatusAction(listStatus->typeId());
        return true;
    case A::LOOT_REMOVE_FROM_BLACKWHITELIST:
        if (TQuickLootBlackWhitelistStorage* storage = lootBlackWhitelist(); storage && listStatus)
            storage->setListed(listStatus->typeId(), false);
        return true;
    default:
        return false;
    }
}

void TQuickLootGameActionHandler::handleAddToBlackWhitelistStatusAction(const uint16_t objectId)
{
    TQuickLootBlackWhitelistStorage* storage = lootBlackWhitelist();
    if (!storage)
        return;
    if (!storage->isListFull()) {
        storage->setListed(objectId, true);
        return;
    }
    auto status = std::make_shared<input::TGameActionSetStatusLine>(qtTrId("iteminfo_maximum_number_of_blacklisted_items"), chat::systemWhite());
    input::publish(status);
}

void TManagedContainersProtocolMessageHandler::handleUpdateManagedContainersMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateManagedContainers& message)
{
    TManagedContainerStorage* storage = TManagedContainerStorage::instance();
    if (!storage)
        return;
    TManagedContainerStorage::Assignments assignments;
    for (const auto& container : message.containers()) {
        if (!isObjectCategory(container.category()))
            return;
        auto& assignment = assignments[static_cast<int>(container.category())];
        assignment.lootContainerId = static_cast<uint16_t>(container.loot_container_id());
        assignment.obtainContainerId = static_cast<uint16_t>(container.obtainer_container_id());
    }
    storage->setManagedContainers(assignments);
    storage->setUseMainBackpackAsFallback(message.fallback_to_main_container());
}

} // namespace tibia::quickloot
