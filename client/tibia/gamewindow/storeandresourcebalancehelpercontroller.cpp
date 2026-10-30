#include "storeandresourcebalancehelpercontroller.h"

#include "store/qmlstoreobjects.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QLocale>

#include <algorithm>
#include <climits>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP FUN_100eebf50 keeps resource 0x49 as the proficiency storage's lunar
/// ascension orb count (+0x94), which FUN_100d62fd0 mirrors here.
constexpr auto LunarAscensionOrbsResource = static_cast<Otc::ResourceTypes_t>(0x49);

/// CIP 0x100d630e0.
QString englishNumber(const qulonglong value)
{
    return QLocale(QLocale::English).toString(value);
}

uint64_t balance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

int intBalance(const Otc::ResourceTypes_t type)
{
    return static_cast<int>(std::min<uint64_t>(balance(type), INT_MAX));
}

}

TStoreAndResourceBalanceHelperController::TStoreAndResourceBalanceHelperController(store::TStoreStorage* storage, QObject* parent)
    : cip::tibia::gamewindow::TStoreAndResourceBalanceHelperController(parent)
    , m_storage(storage)
{
    m_exaltedDustMaximum = 100;
    m_storeBalance = new store::TQmlStoreBalance(*storage, this);
    connect(storage, &store::TStoreStorage::balanceChanged, this, &TStoreAndResourceBalanceHelperController::updateStoreBalance);
    connect(storage, &store::TStoreStorage::balanceUpdatingChanged, this, &TStoreAndResourceBalanceHelperController::updateStoreBalance);
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args&) {
        updateGold();
        updateResources();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        updateGold();
        updateResources();
        return std::any{};
    });
    updateGold();
    updateResources();
}

void TStoreAndResourceBalanceHelperController::setExaltationStorage(exaltation::TExaltationStorage* storage)
{
    m_exaltation = storage;
    connect(storage, &exaltation::TExaltationStorage::currentDustLimitChanged, this, &TStoreAndResourceBalanceHelperController::updateResources);
    connect(storage, &exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged, this,
            &TStoreAndResourceBalanceHelperController::updateResources);
    updateResources();
}

bool TStoreAndResourceBalanceHelperController::canAffordWithTotalGoldBalance(const int Price)
{
    return static_cast<qulonglong>(std::max(0, Price)) <= m_bankGoldBalance + m_inventoryGoldBalance;
}

void TStoreAndResourceBalanceHelperController::updateGold()
{
    // CIP 0x100d627b0: bank / inventory / guild from TPlayerData (+0xe8 / +0xf0 / +0xf8)
    // and hasGuild at +0x100 (set by ResourceBalance type 0x02 → setGuild 0x100ae2ce0).
    const qulonglong bank = balance(Otc::RESOURCE_BANK_BALANCE);
    const qulonglong inventory = balance(Otc::RESOURCE_GOLD_EQUIPPED);
    const qulonglong guild = balance(Otc::RESOURCE_GUILD_BANK_BALANCE);
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const bool hasGuild = player && player->hasResourceBalance(Otc::RESOURCE_GUILD_BANK_BALANCE);
    if (bank == m_bankGoldBalance && inventory == m_inventoryGoldBalance && guild == m_guildBankGoldBalance
        && hasGuild == m_hasGuildBankGoldBalance && !m_totalGoldBalanceString.isEmpty())
        return;
    m_bankGoldBalance = bank;
    m_inventoryGoldBalance = inventory;
    m_guildBankGoldBalance = guild;
    m_hasGuildBankGoldBalance = hasGuild;
    m_totalGoldBalance = bank + inventory;
    m_totalGoldBalanceString = englishNumber(m_totalGoldBalance);
    m_bankGoldBalanceString = englishNumber(bank);
    m_inventoryGoldBalanceString = englishNumber(inventory);
    Q_EMIT goldBalanceChanged();
}

void TStoreAndResourceBalanceHelperController::updateStoreBalance()
{
    if (!m_storage)
        return;
    QObject* previous = m_storeBalance;
    m_storeBalance = new store::TQmlStoreBalance(*m_storage, this);
    Q_EMIT storeBalanceChanged();
    if (previous)
        previous->deleteLater();
}

void TStoreAndResourceBalanceHelperController::updateResources()
{
    const int wildcards = intBalance(Otc::RESOURCE_PREY_WILDCARDS);
    if (wildcards != m_preyWildcards) {
        m_preyWildcards = wildcards;
        Q_EMIT preyWildcardsChanged();
    }
    const int tokens = intBalance(Otc::RESOURCE_TASK_HUNTING);
    if (tokens != m_preyHuntingTaskTokens) {
        m_preyHuntingTaskTokens = tokens;
        Q_EMIT preyHuntingTaskTokensChanged();
    }
    const int dust = intBalance(Otc::RESOURCE_FORGE_DUST);
    const int dustMaximum = m_exaltation ? m_exaltation->currentDustLimit() : 100;
    const int slivers = intBalance(Otc::RESOURCE_FORGE_SLIVER);
    const int cores = intBalance(Otc::RESOURCE_FORGE_CORES);
    if (dust != m_exaltedDustBalance || dustMaximum != m_exaltedDustMaximum || slivers != m_exaltedSliversCount
        || cores != m_exaltedCoreCount) {
        m_exaltedDustBalance = dust;
        m_exaltedDustMaximum = dustMaximum;
        m_exaltedSliversCount = slivers;
        m_exaltedCoreCount = cores;
        Q_EMIT exaltedCurrencyBalanceChanged();
    }
    const int lesser = intBalance(Otc::RESOURCE_LESSER_FRAGMENT);
    const int greater = intBalance(Otc::RESOURCE_GREATER_FRAGMENT);
    if (lesser != m_lesserFragmentCount || greater != m_greaterFragmentCount) {
        m_lesserFragmentCount = lesser;
        m_greaterFragmentCount = greater;
        Q_EMIT fragmentBalanceChanged();
    }
    const auto orbs = static_cast<uint>(intBalance(LunarAscensionOrbsResource));
    if (orbs != m_lunarAscensionOrbsAmount) {
        m_lunarAscensionOrbsAmount = orbs;
        Q_EMIT lunarAscensionOrbsAmountChanged();
    }
}

}
