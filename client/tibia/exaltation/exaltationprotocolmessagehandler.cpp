#include "exaltation/exaltationprotocolmessagehandler.h"

#include "client/const.h"
#include "client/staticdata.h"
#include "exaltation/exaltationstorage.h"
#include "input/gameaction.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace tibia::exaltation {

namespace {

using A = input::EGameAction;
using namespace ::tibia::protobuf::protocol;

/// The ShowMessageDialog type the forge's errors use.
constexpr uint32_t ExaltationErrorDialogType = 50;

/// The history entry's action caption by action type.
QString historyActionCaption(const uint32_t action)
{
    switch (action) {
    case 0: return qtTrId("exaltation_forge_fusion_tab_caption");
    case 1: return qtTrId("exaltation_forge_transfer_tab_caption");
    case 2: return qtTrId("exaltation_forge_conversion_tab_caption");
    case 3: return qtTrId("exaltation_forge_convergence_fusion_action_history_caption");
    case 4: return qtTrId("exaltation_forge_convergence_transfer_action_history_caption");
    default: return qtTrId("dummy_unknown");
    }
}

TExaltationObject objectOf(const ProtocolForgeObject& object)
{
    return { static_cast<uint16_t>(object.object().object_id()), static_cast<uint8_t>(object.object().tier()),
             static_cast<uint32_t>(object.count()) };
}

/// Every group with all its objects.
template <typename Groups>
std::vector<TFusionGroup> fusionGroupsOf(const Groups& groups)
{
    std::vector<TFusionGroup> result;
    result.reserve(static_cast<size_t>(groups.size()));
    for (const auto& group : groups) {
        TFusionGroup& objects = result.emplace_back();
        for (const ProtocolForgeObject& object : group.objects())
            objects.push_back(objectOf(object));
    }
    return result;
}

/// The donors with their tiers; the receivers are tier 0.
template <typename Transfers>
std::vector<TTransferGroup> transferGroupsOf(const Transfers& transfers)
{
    std::vector<TTransferGroup> groups;
    groups.reserve(static_cast<size_t>(transfers.size()));
    for (const ProtocolForgeTransfer& transfer : transfers) {
        TTransferGroup& group = groups.emplace_back();
        for (const ProtocolForgeObject& donor : transfer.donors())
            group.donors.push_back(objectOf(donor));
        for (const auto& receiver : transfer.receivers())
            group.receivers.push_back({ static_cast<uint16_t>(receiver.object_id()), 0, static_cast<uint32_t>(receiver.count()) });
    }
    return groups;
}

/// Host: the market and the item information dialog read the forge's
/// classifications from the forge data notification, not from the storage.
void notifyForgeData(const GameserverMessageExaltationBaseData& message)
{
    const auto prices = [](const auto& list) {
        std::vector<ForgeTierPrice> out;
        for (const auto& p : list)
            out.push_back({ .tier = static_cast<uint8_t>(p.tier()), .price = p.price() });
        return out;
    };
    ForgeConfigData config;
    for (const auto& c : message.class_prices())
        config.classPrices.push_back({ .classId = static_cast<uint8_t>(c.class_id()), .tiers = prices(c.tiers()) });
    for (const auto& g : message.fusion_grades())
        config.fusionGrades.push_back({ .tier = static_cast<uint8_t>(g.tier()), .exaltedCores = static_cast<uint8_t>(g.cores()) });
    config.convergenceFusionPrices = prices(message.convergence_fusion_prices());
    config.convergenceTransferPrices = prices(message.convergence_transfer_prices());
    config.hasConvergence = true;
    const auto dust = static_cast<uint8_t>(message.dust_level());
    config.maxDustLevel = 100 + dust;
    config.maxDustCap = (std::min<uint16_t>(dust, 225) + 5) * 20;
    tibia::game::notify("forgeData", config);
}

}

TExaltationProtocolMessageHandler::TExaltationProtocolMessageHandler(std::shared_ptr<TExaltationStorage> exaltationStorage)
    : m_exaltationStorage(std::move(exaltationStorage))
{
}

namespace {

protobuf::protocol::GameclientMessageExaltationAction exaltationAction(const uint32_t action)
{
    protobuf::protocol::GameclientMessageExaltationAction message;
    message.set_action(action);
    return message;
}

}

void TExaltationProtocolMessageHandler::requestDustToSliversConversion()
{
    Q_EMIT sendExaltationAction(exaltationAction(2));
}

void TExaltationProtocolMessageHandler::requestSliversToCoreConversion()
{
    Q_EMIT sendExaltationAction(exaltationAction(3));
}

void TExaltationProtocolMessageHandler::requestDustLimitIncrease()
{
    Q_EMIT sendExaltationAction(exaltationAction(4));
}

void TExaltationProtocolMessageHandler::requestFusion(const uint32_t firstTypeId, const uint32_t tier, const uint32_t secondTypeId,
                                                      const bool improveChance, const bool reduceTierLoss)
{
    auto message = exaltationAction(0);
    message.set_convergence(false);
    message.set_first_object_id(firstTypeId);
    message.set_tier(tier);
    message.set_second_object_id(secondTypeId);
    message.set_improve_chance(improveChance);
    message.set_reduce_tier_loss(reduceTierLoss);
    Q_EMIT sendExaltationAction(message);
}

void TExaltationProtocolMessageHandler::requestConvergenceFusion(const uint32_t firstTypeId, const uint32_t tier, const uint32_t secondTypeId)
{
    auto message = exaltationAction(0);
    message.set_first_object_id(firstTypeId);
    message.set_tier(tier);
    message.set_second_object_id(secondTypeId);
    message.set_convergence(true);
    Q_EMIT sendExaltationAction(message);
}

void TExaltationProtocolMessageHandler::requestTransfer(const uint32_t sourceTypeId, const uint32_t tier, const uint32_t targetTypeId,
                                                        const bool convergence)
{
    auto message = exaltationAction(1);
    message.set_first_object_id(sourceTypeId);
    message.set_tier(tier);
    message.set_second_object_id(targetTypeId);
    message.set_convergence(convergence);
    Q_EMIT sendExaltationAction(message);
}

void TExaltationProtocolMessageHandler::requestHistoryPage(const int page)
{
    protobuf::protocol::GameclientMessageOpenExaltationHistory message;
    message.set_page(static_cast<uint32_t>(page));
    message.set_entries_per_page(9);
    Q_EMIT sendOpenExaltationHistory(message);
}

/// Dust, slivers and cores (resource types 70 to 72), as 16 bits.
void TExaltationProtocolMessageHandler::handleResourceBalanceMessage(const GameserverMessageResourceBalance& message)
{
    const auto balance = static_cast<uint16_t>(message.value());
    switch (message.resource_type()) {
    case tibia::client::RESOURCE_FORGE_DUST: m_exaltationStorage->setDustBalance(balance); break;
    case tibia::client::RESOURCE_FORGE_SLIVER: m_exaltationStorage->setSliversBalance(balance); break;
    case tibia::client::RESOURCE_FORGE_CORES: m_exaltationStorage->setCoresBalance(balance); break;
    default: break;
    }
}

/// The prices, then the dust limit.
void TExaltationProtocolMessageHandler::handleExaltationBaseDataMessage(const GameserverMessageExaltationBaseData& message)
{
    TExaltationBaseData base;
    for (const auto& classPrices : message.class_prices())
        for (const auto& tier : classPrices.tiers())
            base.classPrices.push_back({ static_cast<uint8_t>(classPrices.class_id()), static_cast<uint8_t>(tier.tier()), tier.price() });
    for (const auto& price : message.convergence_fusion_prices())
        base.convergenceFusionPrices.push_back({ static_cast<uint8_t>(price.tier()), price.price() });
    for (const auto& price : message.convergence_transfer_prices())
        base.convergenceTransferPrices.push_back({ static_cast<uint8_t>(price.tier()), price.price() });
    for (const auto& grade : message.fusion_grades())
        base.transferCores.push_back({ static_cast<uint8_t>(grade.tier()), static_cast<uint16_t>(grade.cores()) });
    m_exaltationStorage->setBaseData(base);
    m_exaltationStorage->setDustLimitRaw(static_cast<uint8_t>(message.dust_level()));
    notifyForgeData(message);
}

void TExaltationProtocolMessageHandler::handleCloseExaltationDialogMessage(const GameserverMessageCloseExaltationDialog&)
{
    Q_EMIT publishGameAction(input::makeGameAction(A::CLOSE_EXALTATION_FORGE_DIALOG));
}

/// Every group with all its objects, the dust limit, then
/// OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG.
void TExaltationProtocolMessageHandler::handleExaltationDialogRefreshMessage(const GameserverMessageExaltationDialogRefresh& message)
{
    m_exaltationStorage->setFusionGroups(fusionGroupsOf(message.fusion_groups()));
    m_exaltationStorage->setConvergenceFusionGroups(fusionGroupsOf(message.convergence_fusion_groups()));
    m_exaltationStorage->setTransferGroups(transferGroupsOf(message.transfers()));
    m_exaltationStorage->setConvergenceTransferGroups(transferGroupsOf(message.convergence_transfers()));
    m_exaltationStorage->setDustLimitRaw(static_cast<uint8_t>(message.dust_level()));
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG));
}

/// The history page with the action caption by type.
void TExaltationProtocolMessageHandler::handleExaltationHistoryMessage(const GameserverMessageExaltationHistory& message)
{
    std::vector<THistoryEntry> history;
    history.reserve(static_cast<size_t>(message.entries_size()));
    for (const auto& entry : message.entries())
        history.push_back({ static_cast<int>(entry.timestamp()), historyActionCaption(entry.action_type()),
                            QString::fromStdString(entry.description()), entry.bonus() });
    m_exaltationStorage->setHistory(static_cast<uint16_t>(message.page()), static_cast<uint16_t>(message.last_page()),
                                    std::move(history));
}

/// Bonuses 0-9 map one to one onto the fusion bonus effect (any other value
/// is an error: the message is dropped).
void TExaltationProtocolMessageHandler::handleExaltationResultMessage(const GameserverMessageExaltationResult& message)
{
    if (message.bonus() > 9)
        return;
    m_exaltationStorage->setLastResult({
        .isFusion = message.action_type() == 0,
        .isConvergence = message.convergence(),
        .wasSuccessful = message.success(),
        .inputTypeId = static_cast<uint16_t>(message.first_object().object_id()),
        .inputTier = static_cast<uint8_t>(message.first_object().tier()),
        .resultTypeId = static_cast<uint16_t>(message.second_object().object_id()),
        .resultTier = static_cast<uint8_t>(message.second_object().tier()),
        .fusionBonus = static_cast<int>(message.bonus()),
        .coreCount = static_cast<uint16_t>(message.core_count()),
        .bonusTypeId = static_cast<uint16_t>(message.bonus_object().object_id()),
        .bonusTier = static_cast<uint8_t>(message.bonus_object().tier()),
    });
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowExaltationResultDialog>(m_exaltationStorage->lastResult()));
}

/// Only the forge's message dialog type becomes SHOW_EXALTATION_ERROR_DIALOG.
void TExaltationProtocolMessageHandler::handleShowMessageDialogMessage(const GameserverMessageShowMessageDialog& message)
{
    if (message.dialog_type() != ExaltationErrorDialogType)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowMessageDialog>(A::SHOW_EXALTATION_ERROR_DIALOG,
                                                                                   QString::fromStdString(message.message())));
}

}
