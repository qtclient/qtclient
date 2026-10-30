#include "exaltationstorage.h"

#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <algorithm>
#include <utility>

namespace tibia::exaltation {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

/// CIP's ShowMessageDialog handler 0x100ad0760: message dialog type 50.
constexpr int ExaltationErrorDialogType = 50;

/// CIP 0x100acfec0: the history entry's action caption.
QString historyActionCaption(const int action)
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

TExaltationObject objectOf(const ForgeItemInfo& item)
{
    return { item.id, item.tier, item.count };
}

std::vector<TTransferGroup> transferGroupsOf(const std::vector<ForgeTransferData>& transfers)
{
    std::vector<TTransferGroup> groups;
    groups.reserve(transfers.size());
    for (const ForgeTransferData& transfer : transfers) {
        TTransferGroup& group = groups.emplace_back();
        for (const ForgeItemInfo& donor : transfer.donors)
            group.donors.push_back(objectOf(donor));
        for (const ForgeItemInfo& receiver : transfer.receivers)
            group.receivers.push_back({ receiver.id, 0, receiver.count });
    }
    return groups;
}

void publish(const A type)
{
    input::publish(input::makeAction(type));
}

uint64_t resourceBalance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

}

TExaltationStorage::TExaltationStorage(QObject* parent)
    : cip::tibia::exaltation::TExaltationStorage(parent)
{
    // CIP 0x100acd380: base data, then the dust limit.
    m_hooks.emplace_back("g_game", "forgeData", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeConfigData>(&args[0]);
        if (!data)
            return std::any{};
        m_classPrices.clear();
        for (const ForgeClassTierPrices& classPrices : data->classPrices) {
            std::map<int, uint64_t>& tiers = m_classPrices[classPrices.classId];
            for (const ForgeTierPrice& tier : classPrices.tiers)
                tiers[tier.tier] = tier.price;
        }
        m_convergenceFusionPrices.clear();
        for (const ForgeTierPrice& price : data->convergenceFusionPrices)
            m_convergenceFusionPrices[price.tier] = price.price;
        m_convergenceTransferPrices.clear();
        for (const ForgeTierPrice& price : data->convergenceTransferPrices)
            m_convergenceTransferPrices[price.tier] = price.price;
        m_transferCoreCosts.clear();
        for (const ForgeGradeData& grade : data->fusionGrades)
            m_transferCoreCosts[grade.tier] = grade.exaltedCores;
        Q_EMIT baseDataChanged();
        // The engine reports 15.25's raw dust limit byte as 100 + raw.
        if (g_game.getClientVersion() >= 1530 && data->maxDustLevel >= 100)
            setDustLimitRaw(static_cast<uint8_t>(data->maxDustLevel - 100));
        return std::any{};
    });
    // CIP 0x100acdfe0. The engine reads each fusion group's single object.
    m_hooks.emplace_back("g_game", "onOpenForge", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeOpenData>(&args[0]);
        if (!data)
            return std::any{};
        std::vector<TFusionGroup> fusion;
        fusion.reserve(data->fusionItems.size());
        for (const ForgeItemInfo& item : data->fusionItems)
            fusion.push_back({ objectOf(item) });
        std::vector<TFusionGroup> convergence;
        convergence.reserve(data->convergenceFusion.size());
        for (const std::vector<ForgeItemInfo>& items : data->convergenceFusion) {
            TFusionGroup& group = convergence.emplace_back();
            for (const ForgeItemInfo& item : items)
                group.push_back(objectOf(item));
        }
        setFusionGroups(std::move(fusion), std::move(convergence));
        setTransferGroups(transferGroupsOf(data->transfers), transferGroupsOf(data->convergenceTransfers));
        setDustLimitRaw(static_cast<uint8_t>(std::min<uint16_t>(data->dustLevel, 0xff)));
        publish(A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG);
        return std::any{};
    });
    // CIP 0x100acfec0.
    m_hooks.emplace_back("g_game", "onBrowseForgeHistory", [this](LuaObject*, const Args& args) {
        const auto* entries = args.size() < 4 ? nullptr : std::any_cast<std::vector<ForgeHistory>>(&args[3]);
        std::vector<THistoryEntry> history;
        if (entries) {
            history.reserve(entries->size());
            for (const ForgeHistory& entry : *entries)
                history.push_back({ static_cast<int>(entry.createdAt), historyActionCaption(entry.actionType),
                                    QString::fromStdString(entry.description), entry.bonus != 0 });
        }
        setHistory(static_cast<uint16_t>(client::gamecore::argInt(args, 0)), static_cast<uint16_t>(client::gamecore::argInt(args, 1)),
                   std::move(history));
        return std::any{};
    });
    // CIP 0x100ad0510. The engine puts a bonus object (bonuses 4-8) over the
    // left object, which Crystal sends again.
    m_hooks.emplace_back("g_game", "forgeResultData", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeResultData>(&args[0]);
        if (!data || data->bonus > 9)
            return std::any{};
        const bool bonusObject = data->bonus >= 4 && data->bonus <= 8;
        m_lastResult = {
            .isFusion = data->actionType == 0,
            .isConvergence = data->convergence,
            .wasSuccessful = data->success,
            .inputTypeId = data->leftItemId,
            .inputTier = data->leftTier,
            .resultTypeId = data->rightItemId,
            .resultTier = data->rightTier,
            .fusionBonus = data->bonus,
            .coreCount = data->coreCount,
            .bonusTypeId = bonusObject ? data->leftItemId : uint16_t{ 0 },
            .bonusTier = bonusObject ? data->leftTier : uint8_t{ 0 },
        };
        publish(A::SHOW_EXALTATION_RESULT_DIALOG);
        return std::any{};
    });
    // CIP 0x100acdf00.
    m_hooks.emplace_back("g_game", "onCloseForgeCloseWindows", [](LuaObject*, const Args&) {
        publish(A::CLOSE_EXALTATION_FORGE_DIALOG);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onServerError", [](LuaObject*, const Args& args) {
        if (client::gamecore::argInt(args, 0) != ExaltationErrorDialogType)
            return std::any{};
        auto action = input::mutableClone(*input::makeAction(A::SHOW_EXALTATION_ERROR_DIALOG));
        action->text = client::gamecore::argString(args, 1);
        input::publish(action);
        return std::any{};
    });
    // CIP 0x100acd350.
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = args.size() < 3 ? nullptr : std::any_cast<Otc::ResourceTypes_t>(&args[2]);
        if (type && (*type == Otc::RESOURCE_FORGE_DUST || *type == Otc::RESOURCE_FORGE_SLIVER || *type == Otc::RESOURCE_FORGE_CORES))
            Q_EMIT exaltedCurrencyBalanceChanged();
        return std::any{};
    });

    // CIP 0x100acc690.
    input::setActionHandler(A::EXALTATION_FORGE_DIALOG_CLOSED, [this](const input::TGameAction&) {
        reset();
        return true;
    });
    input::setActionHandler(A::EXALTATION_FORGE_FUSION, [](const input::TGameAction& action) {
        g_game.forgeRequest(Otc::ForgeAction_t::FUSION, false, action.exaltationTypeId, action.exaltationTier, action.exaltationSecondTypeId,
                            action.exaltationSuccessIncreased, action.exaltationTierLossImproved);
        return true;
    });
    input::setActionHandler(A::EXALTATION_FORGE_CONVERGENCE_FUSION, [](const input::TGameAction& action) {
        g_game.forgeRequest(Otc::ForgeAction_t::FUSION, true, action.exaltationTypeId, action.exaltationTier, action.exaltationSecondTypeId);
        return true;
    });
    input::setActionHandler(A::EXALTATION_FORGE_TRANSFER, [](const input::TGameAction& action) {
        g_game.forgeRequest(Otc::ForgeAction_t::TRANSFER, action.exaltationConvergence, action.exaltationTypeId, action.exaltationTier,
                            action.exaltationSecondTypeId);
        return true;
    });
    input::setActionHandler(A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE, [](const input::TGameAction&) {
        g_game.sendForgeBrowseHistoryRequest(0);
        return true;
    });
    input::setActionHandler(A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE, [this](const input::TGameAction&) {
        if (m_currentPage + 1 < m_numberOfPages)
            g_game.sendForgeBrowseHistoryRequest(m_currentPage + 1);
        return true;
    });
    input::setActionHandler(A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE, [this](const input::TGameAction&) {
        if (m_currentPage != 0)
            g_game.sendForgeBrowseHistoryRequest(m_currentPage - 1);
        return true;
    });
}

TExaltationStorage::~TExaltationStorage()
{
    for (const A type : { A::EXALTATION_FORGE_DIALOG_CLOSED, A::EXALTATION_FORGE_FUSION, A::EXALTATION_FORGE_CONVERGENCE_FUSION,
                          A::EXALTATION_FORGE_TRANSFER, A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE,
                          A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE, A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE })
        input::setActionHandler(type, nullptr);
}

int TExaltationStorage::currentDustLimit() const
{
    return std::min<int>(m_dustLimitRaw, 225) * 20 + 100;
}

uint64_t TExaltationStorage::fusionPrice(const int classification, const int tier) const
{
    const auto it = m_classPrices.find(classification);
    if (it == m_classPrices.end())
        return 0;
    const auto price = it->second.find(tier);
    return price == it->second.end() ? 0 : price->second;
}

uint64_t TExaltationStorage::convergenceFusionPrice(const int tier) const
{
    const auto it = m_convergenceFusionPrices.find(tier);
    return it == m_convergenceFusionPrices.end() ? 0 : it->second;
}

uint64_t TExaltationStorage::convergenceTransferPrice(const int tier) const
{
    const auto it = m_convergenceTransferPrices.find(tier);
    return it == m_convergenceTransferPrices.end() ? 0 : it->second;
}

uint16_t TExaltationStorage::transferCoreCost(const int tier) const
{
    const auto it = m_transferCoreCosts.find(tier);
    return it == m_transferCoreCosts.end() ? 0 : it->second;
}

uint64_t TExaltationStorage::dustBalance() const { return resourceBalance(Otc::RESOURCE_FORGE_DUST); }
uint64_t TExaltationStorage::sliversBalance() const { return resourceBalance(Otc::RESOURCE_FORGE_SLIVER); }
uint64_t TExaltationStorage::coresBalance() const { return resourceBalance(Otc::RESOURCE_FORGE_CORES); }

void TExaltationStorage::reset()
{
    setFusionGroups({}, {});
    setTransferGroups({}, {});
    m_currentPage = 0;
    m_numberOfPages = 1;
    m_history.clear();
}

void TExaltationStorage::setDustLimitRaw(const uint8_t raw)
{
    if (raw == m_dustLimitRaw)
        return;
    m_dustLimitRaw = raw;
    Q_EMIT currentDustLimitChanged();
}

void TExaltationStorage::setFusionGroups(std::vector<TFusionGroup> groups, std::vector<TFusionGroup> convergence)
{
    if (groups == m_fusionGroups && convergence == m_convergenceFusionGroups)
        return;
    m_fusionGroups = std::move(groups);
    m_convergenceFusionGroups = std::move(convergence);
    Q_EMIT fusionGroupsChanged();
}

void TExaltationStorage::setTransferGroups(std::vector<TTransferGroup> groups, std::vector<TTransferGroup> convergence)
{
    if (groups == m_transferGroups && convergence == m_convergenceTransferGroups)
        return;
    m_transferGroups = std::move(groups);
    m_convergenceTransferGroups = std::move(convergence);
    Q_EMIT transferGroupsChanged();
}

void TExaltationStorage::setHistory(const uint16_t page, const uint16_t pages, std::vector<THistoryEntry> entries)
{
    m_currentPage = page;
    m_numberOfPages = pages;
    m_history = std::move(entries);
    Q_EMIT historyChanged();
}

}
