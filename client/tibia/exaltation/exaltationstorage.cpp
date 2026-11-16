#include "exaltationstorage.h"

#include "input/gameactionhandlers.h"

#include <client/staticdata.h>

#include <algorithm>
#include <set>
#include <utility>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::exaltation {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;

/// The ShowMessageDialog type the forge's errors use.
constexpr int ExaltationErrorDialogType = 50;

/// The history entry's action caption by action type.
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

}

namespace {
TExaltationStorage* s_instance = nullptr;
}

TExaltationStorage* TExaltationStorage::instance() { return s_instance; }

TExaltationStorage::TExaltationStorage(QObject* parent)
    : cip::tibia::exaltation::TExaltationStorage(parent)
{
    s_instance = this;
    // CIP TExaltationProtocolMessageHandler::handleExaltationBaseDataMessage:
    // the prices, then the dust limit.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "forgeData", [this](GameObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeConfigData>(&args[0]);
        if (!data)
            return std::any{};
        TExaltationBaseData base;
        for (const ForgeClassTierPrices& classPrices : data->classPrices)
            for (const ForgeTierPrice& tier : classPrices.tiers)
                base.classPrices.push_back({ classPrices.classId, tier.tier, tier.price });
        for (const ForgeTierPrice& price : data->convergenceFusionPrices)
            base.convergenceFusionPrices.push_back({ price.tier, price.price });
        for (const ForgeTierPrice& price : data->convergenceTransferPrices)
            base.convergenceTransferPrices.push_back({ price.tier, price.price });
        for (const ForgeGradeData& grade : data->fusionGrades)
            base.transferCores.push_back({ grade.tier, grade.exaltedCores });
        setBaseData(base);
        // The protocol handler reports the raw dust limit byte as 100 + raw.
        if (data->maxDustLevel >= 100)
            setDustLimitRaw(static_cast<uint8_t>(data->maxDustLevel - 100));
        return std::any{};
    });
    // CIP handleExaltationDialogRefreshMessage: every group with all its
    // objects, the dust limit, then OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOpenForge", [this](GameObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeOpenData>(&args[0]);
        if (!data)
            return std::any{};
        const auto* fusionItems = args.size() < 2 ? nullptr : std::any_cast<std::vector<std::vector<ForgeItemInfo>>>(&args[1]);
        std::vector<TFusionGroup> fusion;
        if (fusionItems) {
            fusion.reserve(fusionItems->size());
            for (const std::vector<ForgeItemInfo>& items : *fusionItems) {
                TFusionGroup& group = fusion.emplace_back();
                for (const ForgeItemInfo& item : items)
                    group.push_back(objectOf(item));
            }
        }
        std::vector<TFusionGroup> convergence;
        convergence.reserve(data->convergenceFusion.size());
        for (const std::vector<ForgeItemInfo>& items : data->convergenceFusion) {
            TFusionGroup& group = convergence.emplace_back();
            for (const ForgeItemInfo& item : items)
                group.push_back(objectOf(item));
        }
        setFusionGroups(std::move(fusion));
        setConvergenceFusionGroups(std::move(convergence));
        setTransferGroups(transferGroupsOf(data->transfers));
        setConvergenceTransferGroups(transferGroupsOf(data->convergenceTransfers));
        setDustLimitRaw(static_cast<uint8_t>(data->dustLevel));
        publish(A::OPEN_OR_UPDATE_EXALTATION_FORGE_DIALOG);
        return std::any{};
    });
    // CIP handleExaltationHistoryMessage: the action caption by type.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBrowseForgeHistory", [this](GameObject*, const Args& args) {
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
    // CIP handleExaltationResultMessage: bonuses 0-9 map one to one onto
    // EFusionBonusEffect (CIP throws on any other value; the message is dropped).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "forgeResultData", [this](GameObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeResultData>(&args[0]);
        if (!data || data->bonus > 9)
            return std::any{};
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
            .bonusTypeId = static_cast<uint16_t>(client::gamecore::argInt(args, 1)),
            .bonusTier = static_cast<uint8_t>(client::gamecore::argInt(args, 2)),
        };
        publish(A::SHOW_EXALTATION_RESULT_DIALOG);
        return std::any{};
    });
    // CIP handleCloseExaltationDialogMessage.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCloseForgeCloseWindows", [](GameObject*, const Args&) {
        publish(A::CLOSE_EXALTATION_FORGE_DIALOG);
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::serverError, this,
            [](const int type, const QString& message) { handleShowMessageDialogMessage(type, message); });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](const qint64 balance, qint64, const int type) { handleResourceBalanceMessage(type, balance); });

    for (const A type : { A::EXALTATION_FORGE_DIALOG_CLOSED, A::EXALTATION_FORGE_FUSION, A::EXALTATION_FORGE_CONVERGENCE_FUSION,
                          A::EXALTATION_FORGE_TRANSFER, A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE,
                          A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE, A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE })
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleGameAction(action); });
}

TExaltationStorage::~TExaltationStorage()
{
    if (s_instance == this)
        s_instance = nullptr;
    for (const A type : { A::EXALTATION_FORGE_DIALOG_CLOSED, A::EXALTATION_FORGE_FUSION, A::EXALTATION_FORGE_CONVERGENCE_FUSION,
                          A::EXALTATION_FORGE_TRANSFER, A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE,
                          A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE, A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE })
        input::setActionHandler(type, nullptr);
}

/// Dust, slivers and cores (resource types 70 to 72), as 16 bits.
void TExaltationStorage::handleResourceBalanceMessage(const int type, const qint64 balance)
{
    switch (type) {
    case tibia::client::RESOURCE_FORGE_DUST: setDustBalance(static_cast<uint16_t>(balance)); break;
    case tibia::client::RESOURCE_FORGE_SLIVER: setSliversBalance(static_cast<uint16_t>(balance)); break;
    case tibia::client::RESOURCE_FORGE_CORES: setCoresBalance(static_cast<uint16_t>(balance)); break;
    default: break;
    }
}

/// Only the forge's message dialog type becomes SHOW_EXALTATION_ERROR_DIALOG.
void TExaltationStorage::handleShowMessageDialogMessage(const int type, const QString& message)
{
    if (type != ExaltationErrorDialogType)
        return;
    auto action = input::mutableClone(*input::makeAction(A::SHOW_EXALTATION_ERROR_DIALOG));
    action->text = message;
    input::publish(action);
}

bool TExaltationStorage::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::EXALTATION_FORGE_DIALOG_CLOSED: handleExaltationForgeDialogClosedGameAction(); break;
    case A::EXALTATION_FORGE_FUSION: handleExaltationForgeFusionGameAction(action); break;
    case A::EXALTATION_FORGE_CONVERGENCE_FUSION: handleExaltationForgeConvergenceFusionGameAction(action); break;
    case A::EXALTATION_FORGE_TRANSFER: handleExaltationForgeTransferGameAction(action); break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_FIRST_PAGE: tibia::game::sendForgeBrowseHistoryRequest(0); break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_NEXT_PAGE: handleExaltationForgeHistoryNextPageGameAction(); break;
    case A::REQUEST_EXALTATION_FORGE_HISTORY_PREVIOUS_PAGE: handleExaltationForgeHistoryPreviousPageGameAction(); break;
    default: return false;
    }
    return true;
}

void TExaltationStorage::handleExaltationForgeDialogClosedGameAction()
{
    reset();
}

void TExaltationStorage::handleExaltationForgeFusionGameAction(const input::TGameAction& action)
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::FUSION, false, action.exaltationTypeId, action.exaltationTier,
                              action.exaltationSecondTypeId, action.exaltationSuccessIncreased, action.exaltationTierLossImproved);
}

void TExaltationStorage::handleExaltationForgeConvergenceFusionGameAction(const input::TGameAction& action)
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::FUSION, true, action.exaltationTypeId, action.exaltationTier,
                              action.exaltationSecondTypeId);
}

void TExaltationStorage::handleExaltationForgeTransferGameAction(const input::TGameAction& action)
{
    tibia::game::forgeRequest(tibia::client::ForgeAction_t::TRANSFER, action.exaltationConvergence, action.exaltationTypeId,
                              action.exaltationTier, action.exaltationSecondTypeId);
}

void TExaltationStorage::handleExaltationForgeHistoryNextPageGameAction()
{
    const uint16_t next = static_cast<uint16_t>(m_currentPage + 1);
    if (next < m_numberOfPages)
        tibia::game::sendForgeBrowseHistoryRequest(next);
}

void TExaltationStorage::handleExaltationForgeHistoryPreviousPageGameAction()
{
    if (m_currentPage != 0)
        tibia::game::sendForgeBrowseHistoryRequest(static_cast<uint16_t>(m_currentPage - 1));
}

void TExaltationStorage::setBaseData(const TExaltationBaseData& data)
{
    for (const TExaltationBaseData::TClassTierPrice& price : data.classPrices)
        m_classPrices[price.classification][price.tier] = price.price;
    for (const TExaltationBaseData::TTierPrice& price : data.convergenceFusionPrices)
        m_convergenceFusionPrices[price.tier] = price.price;
    for (const TExaltationBaseData::TTierPrice& price : data.convergenceTransferPrices)
        m_convergenceTransferPrices[price.tier] = price.price;
    for (const TExaltationBaseData::TTierCores& cores : data.transferCores)
        m_transferCoreCosts[cores.tier] = cores.cores;
    Q_EMIT baseDataChanged();
}

void TExaltationStorage::setDustLimitRaw(const uint8_t raw)
{
    if (raw == m_dustLimitRaw)
        return;
    m_dustLimitRaw = raw;
    Q_EMIT currentDustLimitChanged();
}

int TExaltationStorage::currentDustLimit() const
{
    return std::min<int>(m_dustLimitRaw, 225) * 20 + 100;
}

uint64_t TExaltationStorage::fusionPrice(const int classification, const int tier) const
{
    const auto it = m_classPrices.find(static_cast<uint8_t>(classification));
    if (it == m_classPrices.end())
        return 0;
    const auto price = it->second.find(static_cast<uint8_t>(tier));
    return price == it->second.end() ? 0 : price->second;
}

uint64_t TExaltationStorage::convergenceFusionPrice(const int tier) const
{
    const auto it = m_convergenceFusionPrices.find(static_cast<uint8_t>(tier));
    return it == m_convergenceFusionPrices.end() ? 0 : it->second;
}

uint64_t TExaltationStorage::convergenceTransferPrice(const int tier) const
{
    const auto it = m_convergenceTransferPrices.find(static_cast<uint8_t>(tier));
    return it == m_convergenceTransferPrices.end() ? 0 : it->second;
}

uint16_t TExaltationStorage::transferCoreCost(const int tier) const
{
    const auto it = m_transferCoreCosts.find(static_cast<uint8_t>(tier));
    return it == m_transferCoreCosts.end() ? 0 : it->second;
}

std::vector<uint8_t> TExaltationStorage::classifications() const
{
    std::vector<uint8_t> result;
    result.reserve(m_classPrices.size());
    for (const auto& [classification, prices] : m_classPrices)
        result.push_back(classification);
    return result;
}

std::vector<uint8_t> TExaltationStorage::tiers(const uint8_t classification) const
{
    std::vector<uint8_t> result;
    const auto it = m_classPrices.find(classification);
    if (it == m_classPrices.end())
        return result;
    result.reserve(it->second.size() + 1);
    for (const auto& [tier, price] : it->second)
        result.push_back(tier);
    if (!result.empty())
        result.push_back(static_cast<uint8_t>(result.back() + 1));
    return result;
}

std::vector<uint8_t> TExaltationStorage::allTiers() const
{
    std::set<uint8_t> unique;
    for (const uint8_t classification : classifications())
        for (const uint8_t tier : tiers(classification))
            unique.insert(tier);
    std::vector<uint8_t> result(unique.begin(), unique.end());
    std::ranges::sort(result);
    return result;
}

void TExaltationStorage::setFusionGroups(std::vector<TFusionGroup> groups)
{
    if (groups == m_fusionGroups)
        return;
    m_fusionGroups = std::move(groups);
    Q_EMIT fusionGroupsChanged();
}

void TExaltationStorage::setConvergenceFusionGroups(std::vector<TFusionGroup> groups)
{
    if (groups == m_convergenceFusionGroups)
        return;
    m_convergenceFusionGroups = std::move(groups);
    Q_EMIT fusionGroupsChanged();
}

void TExaltationStorage::setTransferGroups(std::vector<TTransferGroup> groups)
{
    if (groups == m_transferGroups)
        return;
    m_transferGroups = std::move(groups);
    Q_EMIT transferGroupsChanged();
}

void TExaltationStorage::setConvergenceTransferGroups(std::vector<TTransferGroup> groups)
{
    if (groups == m_convergenceTransferGroups)
        return;
    m_convergenceTransferGroups = std::move(groups);
    Q_EMIT transferGroupsChanged();
}

void TExaltationStorage::setHistory(const uint16_t page, const uint16_t pages, std::vector<THistoryEntry> entries)
{
    if (page == m_currentPage && pages == m_numberOfPages && entries == m_history)
        return;
    m_currentPage = page;
    m_numberOfPages = pages;
    m_history = std::move(entries);
    Q_EMIT historyChanged();
}

/// The convergence groups stay; the history goes without a signal.
void TExaltationStorage::reset()
{
    setFusionGroups({});
    setTransferGroups({});
    m_currentPage = 0;
    m_numberOfPages = 1;
    m_history.clear();
}

void TExaltationStorage::setDustBalance(const uint16_t balance)
{
    if (balance == m_dustBalance)
        return;
    m_dustBalance = balance;
    Q_EMIT exaltedCurrencyBalanceChanged();
}

void TExaltationStorage::setSliversBalance(const uint16_t balance)
{
    if (balance == m_sliversBalance)
        return;
    m_sliversBalance = balance;
    Q_EMIT exaltedCurrencyBalanceChanged();
}

void TExaltationStorage::setCoresBalance(const uint16_t balance)
{
    if (balance == m_coresBalance)
        return;
    m_coresBalance = balance;
    Q_EMIT exaltedCurrencyBalanceChanged();
}

}
