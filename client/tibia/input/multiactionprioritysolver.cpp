#include "input/multiactionprioritysolver.h"

#include "appearances/objectappearanceinstancetypeid.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "game/cooldownstorage.h"
#include "game/playerdata.h"
#include "spells/spellinfostorage.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_set>

namespace tibia::input {

namespace {

using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;

}

TMultiActionPrioritySolver::TMultiActionPrioritySolver(const game::TCooldownStorage* cooldownStorage, const game::TPlayerData* playerData,
                                                       const container::TObjectAppearanceInstanceInfoStorage* objectInfoStorage,
                                                       const qint64 now, const qint64 latency)
    : m_cooldownStorage(cooldownStorage)
    , m_playerData(playerData)
    , m_objectInfoStorage(objectInfoStorage)
    , m_now(now)
    , m_latency(latency)
{
}

TMultiActionPrioritySolver::IAction* TMultiActionPrioritySolver::solve(const std::vector<IAction*>& candidates) const
{
    qint64 best = std::numeric_limits<qint64>::max();
    IAction* chosen = nullptr;
    for (IAction* candidate : candidates) {
        if (candidate->isEmpty())
            continue;
        if (!isUsable(*candidate)) {
            if (!chosen)
                chosen = candidate;
            continue;
        }
        const qint64 remaining = remainingDelay(*candidate);
        if (remaining == 0)
            return candidate;
        if (m_latency + remaining < best) {
            chosen = candidate;
            best = remaining;
        }
    }
    return chosen;
}

bool TMultiActionPrioritySolver::isUsable(IAction& candidate) const
{
    if (candidate.resolveSpell()) {
        const std::unordered_set<uint16_t>& knownSpells = m_playerData->knownSpells();
        return knownSpells.find(candidate.spellId()) != knownSpells.end();
    }
    return !candidate.hasObject() || objectCount(candidate) > 0;
}

qint64 TMultiActionPrioritySolver::remainingDelay(IAction& candidate) const
{
    if (candidate.resolveSpell())
        return m_cooldownStorage->spellCooldown(candidate.spellId(), m_now).remaining;
    if (!candidate.hasObject())
        return 0;
    const uint16_t objectId = candidate.objectId();
    const config::EUseType useType = candidate.useType();
    if (spells::TSpellInfoStorage::instance()->runeSpellOfObject(objectId))
        return m_cooldownStorage->objectCooldown(objectId, m_now).remaining;
    switch (useType) {
    case config::EUseType::UseOnYourself:
    case config::EUseType::UseOnTarget:
    case config::EUseType::SelectUseTarget:
    case config::EUseType::UseAtMouseCursor:
        return m_cooldownStorage->multiUseCooldown(m_now).remaining;
    default:
        return 0;
    }
}

int TMultiActionPrioritySolver::objectCount(IAction& candidate) const
{
    if (!candidate.hasObject())
        return 0;
    const uint16_t objectId = candidate.objectId();
    const auto* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, objectId);
    TObjectAppearanceInstanceTypeID instance = TObjectAppearanceInstanceTypeID::plain(objectId);
    if (candidate.hasLiquidType())
        instance = TObjectAppearanceInstanceTypeID::withLiquid(objectId, candidate.liquidType());
    else if (type && type->hasUpgradeClassification())
        instance = TObjectAppearanceInstanceTypeID::withTier(objectId, candidate.upgradeTier());
    // CipSoft flags the key of some other objects, but the count storage compares
    // keys by type, liquid and tier only: such a key finds the plain count.
    int count = m_objectInfoStorage->count(instance);
    if (const std::optional<uint16_t> counterpart = type ? type->expireCounterpart() : std::nullopt)
        count = std::max(count, m_objectInfoStorage->count(TObjectAppearanceInstanceTypeID::plain(*counterpart)));
    return count;
}

}
