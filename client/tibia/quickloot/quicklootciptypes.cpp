#include "quickloot/quicklootciptypes.h"
#include "quickloot/managedcontainerstorage.h"
#include <protocol.pb.h>

#include <algorithm>
#include <vector>
#include <stdexcept>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include "protocol/protocolciptypes.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "config/clientoptions.h"
#include "config/optionspages.h"
#include "analysers/analyserstorages.h"
#include "quickloot/quicklootblackwhiteliststorage.h"
#include "game/currentgamesession.h"
#include "config/characterconfigurationmanager.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"

namespace tibia::quickloot {

namespace {

/// The categories the server may assign containers to.
bool isObjectCategory(const uint32_t category) { return category - 1u < 31u; }

}

void TManagedContainersProtocolMessageHandler::setLootContainerFallback(const bool useMainBackpack)
{
    ::tibia::protobuf::protocol::GameclientMessageManagedContainer message;
    message.set_action(3);
    message.set_category(0);
    message.set_use_main_as_fallback(useMainBackpack);
    Q_EMIT sendManagedContainer(message);
}

void TManagedContainersProtocolMessageHandler::quickLoot(const Position& position, const uint16_t objectId, const int stackPosition,
                                                         const bool allCorpsesInArea)
{
    // Looting a corpse on the map loots every corpse around it when the option asks for it.
    const bool onMap = position.x != 0xFFFF;
    ::tibia::protobuf::protocol::GameclientMessageQuickLoot message;
    message.set_variant(onMap && allCorpsesInArea ? 1 : 0);
    auto* object = message.mutable_object();
    protocol::gameclient::setClassicObject(object->mutable_object_position(), object->mutable_object_identifier(), position,
                                           std::max(0, stackPosition), objectId);
    Q_EMIT sendQuickLoot(message);
}

void TManagedContainersProtocolMessageHandler::setQuickLootBlackWhitelist(const int listType, const std::set<uint16_t>& objectTypeIds)
{
    if (static_cast<unsigned>(listType) >= 2)
        throw std::runtime_error("unhandled enum value");
    ::tibia::protobuf::protocol::GameclientMessageQuickLootBlackWhitelist message;
    message.set_filter(static_cast<uint32_t>(listType));
    for (const uint16_t objectTypeId : objectTypeIds)
        message.add_object_ids(objectTypeId);
    Q_EMIT sendQuickLootBlackWhitelist(message);
}

void TManagedContainersProtocolMessageHandler::quickLootAreaAtPlayer()
{
    ::tibia::protobuf::protocol::GameclientMessageQuickLoot message;
    message.set_variant(2);
    Q_EMIT sendQuickLoot(message);
}

void TManagedContainersProtocolMessageHandler::manageContainer(const EManagedContainerAction action, const int category,
                                                               const Position& position, const uint16_t objectId, const int stackPosition)
{
    if (!isObjectCategory(static_cast<uint32_t>(category)))
        return;
    ::tibia::protobuf::protocol::GameclientMessageManagedContainer message;
    message.set_action(action);
    message.set_category(static_cast<uint32_t>(category));
    if (action == SelectLootContainer || action == SelectObtainContainer)
        protocol::gameclient::setClassicObject(message.mutable_object_position(), message.mutable_object_identifier(), position,
                                               std::max(0, stackPosition), objectId);
    message.set_use_main_as_fallback(false);
    Q_EMIT sendManagedContainer(message);
}

namespace {

using A = input::EGameAction;
using P = TManagedContainersProtocolMessageHandler;

constexpr A QuickLootActions[] = { A::QUICK_LOOT, A::QUICK_LOOT_AREA_AT_PLAYER, A::LOOT_CONTAINER_SELECT, A::LOOT_CONTAINER_CLEAR,
                                   A::LOOT_CONTAINER_OPEN, A::OBTAIN_CONTAINER_SELECT, A::OBTAIN_CONTAINER_CLEAR, A::OBTAIN_CONTAINER_OPEN,
                                   A::LOOT_CONTAINER_FALLBACK, A::LOOT_ADD_TO_BLACKWHITELIST, A::LOOT_REMOVE_FROM_BLACKWHITELIST };

}

TQuickLootGameActionHandler::TQuickLootGameActionHandler(std::shared_ptr<TManagedContainersProtocolMessageHandler> protocolMessageHandler,
                                                         std::shared_ptr<TQuickLootBlackWhitelistStorage> blackWhitelistStorage,
                                                         std::shared_ptr<config::TClientOptions> clientOptions, QObject* parent)
    : cip::tibia::quickloot::TQuickLootGameActionHandler(parent)
    , m_protocolMessageHandler(std::move(protocolMessageHandler))
    , m_blackWhitelistStorage(std::move(blackWhitelistStorage))
    , m_clientOptions(std::move(clientOptions))
{
    for (const A type : QuickLootActions)
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

TQuickLootGameActionHandler::~TQuickLootGameActionHandler()
{
}

bool TQuickLootGameActionHandler::handleGameAction(const input::IGameAction& pGameAction)
{
    const auto* listStatus = input::gameActionAs<input::TGameActionToggleBlackWhitelistStatus>(&pGameAction);
    switch (pGameAction.gameActionType()) {
    case A::QUICK_LOOT:
        if (const auto* loot = input::gameActionAs<input::TGameActionQuickLoot>(&pGameAction))
            m_protocolMessageHandler->quickLoot(
                loot->position(), loot->objectClientId(), loot->stackPosition(),
                m_clientOptions && m_clientOptions->boolOption(cip::EClientOption::quickLootAllCorpsesInAreaEnabled));
        return true;
    case A::QUICK_LOOT_AREA_AT_PLAYER:
        m_protocolMessageHandler->quickLootAreaAtPlayer();
        return true;
    case A::LOOT_CONTAINER_SELECT:
    case A::OBTAIN_CONTAINER_SELECT:
        if (const auto* select = input::gameActionAs<input::TGameActionSelectManagedContainer>(&pGameAction)) {
            const input::TObjectGameActionContext& container = select->container();
            m_protocolMessageHandler->manageContainer(pGameAction.gameActionType() == A::LOOT_CONTAINER_SELECT ? P::SelectLootContainer
                                                                                                                : P::SelectObtainContainer,
                                                      select->category(), container.position(), container.objectClientId(),
                                                      container.stackPosition());
        }
        return true;
    case A::LOOT_CONTAINER_CLEAR:
    case A::LOOT_CONTAINER_OPEN:
    case A::OBTAIN_CONTAINER_CLEAR:
    case A::OBTAIN_CONTAINER_OPEN:
        if (const auto* managed = input::gameActionAs<input::TGameActionManagedContainer>(&pGameAction)) {
            const A type = pGameAction.gameActionType();
            m_protocolMessageHandler->manageContainer(type == A::LOOT_CONTAINER_CLEAR     ? P::ClearLootContainer
                                                      : type == A::LOOT_CONTAINER_OPEN    ? P::OpenLootContainer
                                                      : type == A::OBTAIN_CONTAINER_CLEAR ? P::ClearObtainContainer
                                                                                          : P::OpenObtainContainer,
                                                      managed->category());
        }
        return true;
    case A::LOOT_CONTAINER_FALLBACK:
        if (const auto* fallback = input::gameActionAs<input::TGameActionSetLootContainerFallback>(&pGameAction))
            m_protocolMessageHandler->setLootContainerFallback(fallback->useMainContainerAsFallback());
        return true;
    case A::LOOT_ADD_TO_BLACKWHITELIST:
        if (listStatus)
            handleAddToBlackWhitelistStatusAction(listStatus->typeId());
        return true;
    case A::LOOT_REMOVE_FROM_BLACKWHITELIST:
        if (TQuickLootBlackWhitelistStorage* storage = m_blackWhitelistStorage.get(); storage && listStatus)
            storage->removeItem(listStatus->typeId());
        return true;
    default:
        return false;
    }
}

void TQuickLootGameActionHandler::handleAddToBlackWhitelistStatusAction(const uint16_t objectId)
{
    TQuickLootBlackWhitelistStorage* storage = m_blackWhitelistStorage.get();
    if (!storage)
        return;
    if (!storage->isListFull()) {
        storage->addItem(objectId);
        return;
    }
    auto status = std::make_shared<input::TGameActionSetStatusLine>(qtTrId("iteminfo_maximum_number_of_blacklisted_items"), chat::systemWhite());
    Q_EMIT publishGameAction(status);
}

TManagedContainersProtocolMessageHandler::TManagedContainersProtocolMessageHandler(
    TManagedContainerStorage* storage, config::TCharacterConfigurationManager* characterConfigurationManager, QObject* parent)
    : cip::tibia::quickloot::TManagedContainersProtocolMessageHandler(parent)
    , m_storage(storage)
    , m_characterConfigurationManager(characterConfigurationManager)
{
    if (!m_characterConfigurationManager)
        return;
    connect(m_characterConfigurationManager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
            &TManagedContainersProtocolMessageHandler::onQuickLootBlackWhitelistConfigurationChanged);
    if (TQuickLootBlackWhitelistStorage* list = m_characterConfigurationManager->quickLootBlackWhitelistStorage())
        connect(list, &TQuickLootBlackWhitelistStorage::configurationChangedFromUserInput, this,
                &TManagedContainersProtocolMessageHandler::onQuickLootBlackWhitelistConfigurationChanged);
}

void TManagedContainersProtocolMessageHandler::onQuickLootBlackWhitelistConfigurationChanged()
{
    if (!m_characterConfigurationManager->isConfigurationLoaded())
        return;
    const TQuickLootBlackWhitelistStorage* list = m_characterConfigurationManager->quickLootBlackWhitelistStorage();
    // Host: the client hands the session its loot list storage once the game
    // window has one (a replay without it has no list to send).
    if (!list)
        return;
    setQuickLootBlackWhitelist(list->listType(), list->list(list->listType()));
}

void TManagedContainersProtocolMessageHandler::handleUpdateManagedContainersMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateManagedContainers& message)
{
    TManagedContainerStorage* storage = m_storage;
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
