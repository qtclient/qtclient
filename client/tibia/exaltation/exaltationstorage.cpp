#include "exaltationstorage.h"

#include <algorithm>
#include <set>
#include <utility>

namespace tibia::exaltation {

TExaltationStorage::TExaltationStorage(QObject* parent)
    : cip::tibia::exaltation::TExaltationStorage(parent)
{
}

TExaltationStorage::~TExaltationStorage() = default;

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
    return m_classPrices.at(static_cast<uint8_t>(classification)).at(static_cast<uint8_t>(tier));
}

uint64_t TExaltationStorage::convergenceFusionPrice(const int tier) const
{
    return m_convergenceFusionPrices.at(static_cast<uint8_t>(tier));
}

uint64_t TExaltationStorage::transferPrice(const int classification, const int tier) const
{
    return m_classPrices.at(static_cast<uint8_t>(classification)).at(static_cast<uint8_t>(tier - 1));
}

uint64_t TExaltationStorage::convergenceTransferPrice(const int tier) const
{
    return m_convergenceTransferPrices.at(static_cast<uint8_t>(tier));
}

uint16_t TExaltationStorage::transferCoreCost(const int tier) const
{
    return m_transferCoreCosts.at(static_cast<uint8_t>(tier));
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
