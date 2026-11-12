#include "storeandresourcebalancehelpercontroller.h"

#include "store/qmlstoreobjects.h"

#include <QLocale>

#include <algorithm>
#include <climits>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/playerdata.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP keeps resource 0x49 as the proficiency storage's lunar
/// ascension orb count (+0x94), which mirrors here.
constexpr auto LunarAscensionOrbsResource = static_cast<tibia::client::ResourceTypes_t>(0x49);

/// CIP .
QString englishNumber(const qulonglong value)
{
    return QLocale(QLocale::English).toString(value);
}

uint64_t balance(const tibia::client::ResourceTypes_t type)
{
    return tibia::game::localPlayerResourceBalance(type);
}

int intBalance(const tibia::client::ResourceTypes_t type)
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
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() {
        updateGold();
        updateResources();
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        updateGold();
        updateResources();
    });
    updateGold();
    updateResources();
}

// b1/9049bbf1e52d7203
void TStoreAndResourceBalanceHelperController::setExaltationStorage(exaltation::TExaltationStorage* storage)
{
    m_exaltation = storage;
    connect(storage, &exaltation::TExaltationStorage::currentDustLimitChanged, this, &TStoreAndResourceBalanceHelperController::updateResources);
    connect(storage, &exaltation::TExaltationStorage::exaltedCurrencyBalanceChanged, this,
            &TStoreAndResourceBalanceHelperController::updateResources);
    updateResources();
}

// b1/5492252506b7b4d7
bool TStoreAndResourceBalanceHelperController::canAffordWithTotalGoldBalance(const int Price)
{
    return static_cast<qulonglong>(std::max(0, Price)) <= m_bankGoldBalance + m_inventoryGoldBalance;
}

// b1/b72c5982fcc3894a
void TStoreAndResourceBalanceHelperController::updateGold()
{
    // CIP : bank / inventory / guild from TPlayerData (+0xe8 / +0xf0 / +0xf8)
    // and hasGuild at +0x100 (set by ResourceBalance type 0x02 → setGuild).
    qulonglong bank = 0;
    qulonglong inventory = 0;
    qulonglong guild = 0;
    bool hasGuild = false;
    if (tibia::game::TPlayerData* data = tibia::game::TPlayerData::liveInstance()) {
        bank = data->v72();
        inventory = data->resourceBalance(tibia::client::RESOURCE_GOLD_EQUIPPED);
        guild = data->v77();
        hasGuild = data->v78();
    } else {
        bank = balance(tibia::client::RESOURCE_BANK_BALANCE);
        inventory = balance(tibia::client::RESOURCE_GOLD_EQUIPPED);
        guild = balance(tibia::client::RESOURCE_GUILD_BANK_BALANCE);
        hasGuild = tibia::game::localPlayerHasResourceBalance(tibia::client::RESOURCE_GUILD_BANK_BALANCE);
    }
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

// b1/d376c6f3310ab534
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

// b1/1d2e89c080f54bf6
void TStoreAndResourceBalanceHelperController::updateResources()
{
    const int wildcards = intBalance(tibia::client::RESOURCE_PREY_WILDCARDS);
    if (wildcards != m_preyWildcards) {
        m_preyWildcards = wildcards;
        Q_EMIT preyWildcardsChanged();
    }
    const int tokens = intBalance(tibia::client::RESOURCE_TASK_HUNTING);
    if (tokens != m_preyHuntingTaskTokens) {
        m_preyHuntingTaskTokens = tokens;
        Q_EMIT preyHuntingTaskTokensChanged();
    }
    const int dust = intBalance(tibia::client::RESOURCE_FORGE_DUST);
    const int dustMaximum = m_exaltation ? m_exaltation->currentDustLimit() : 100;
    const int slivers = intBalance(tibia::client::RESOURCE_FORGE_SLIVER);
    const int cores = intBalance(tibia::client::RESOURCE_FORGE_CORES);
    if (dust != m_exaltedDustBalance || dustMaximum != m_exaltedDustMaximum || slivers != m_exaltedSliversCount
        || cores != m_exaltedCoreCount) {
        m_exaltedDustBalance = dust;
        m_exaltedDustMaximum = dustMaximum;
        m_exaltedSliversCount = slivers;
        m_exaltedCoreCount = cores;
        Q_EMIT exaltedCurrencyBalanceChanged();
    }
    const int lesser = intBalance(tibia::client::RESOURCE_LESSER_FRAGMENT);
    const int greater = intBalance(tibia::client::RESOURCE_GREATER_FRAGMENT);
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
    const auto cosmetic = static_cast<uint>(intBalance(tibia::client::RESOURCE_COSMETIC_TOKENS));
    if (cosmetic != m_cosmeticTokenCount) {
        m_cosmeticTokenCount = cosmetic;
        Q_EMIT cosmeticTokenCountChanged();
    }
}

}
