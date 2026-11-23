#include "input/actionbuttongameactionhandler.h"

#include "appearances/objectappearanceinstancetypeid.h"
#include "config/hotkeyoptions.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "game/cooldownstorage.h"
#include "game/playerdata.h"
#include "input/keyboardshortcuthandler.h"
#include "spells/spellinfostorage.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace tibia::input {

namespace {

using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;

/// One slot of a multi action as the priority solver asks it: the slot's
/// setting and the spell its text casts (looked up on demand).
class TPrioritySolverAdapter
{
public:
    TPrioritySolverAdapter(const spells::TSpellInfoStorage* spellInfoStorage, config::TActionSetting setting)
        : m_spellInfoStorage(spellInfoStorage)
        , m_setting(std::move(setting))
    {
    }

    /// No text, no action, no object, no passive ability and no slots.
    bool isEmpty() const;
    /// Whether the text casts a spell; remembers the spell.
    bool resolveSpell();
    /// The spell resolveSpell found.
    uint16_t spellId() const;
    bool hasObject() const;
    config::EUseType useType() const;
    uint16_t objectId() const;
    bool hasLiquidType() const;
    uint32_t liquidType() const;
    bool hasUpgradeTier() const;
    uint8_t upgradeTier() const;

    const config::TActionSetting& setting() const { return m_setting; }

private:
    const spells::TSpellInfoStorage* m_spellInfoStorage;
    config::TActionSetting m_setting;
    uint16_t m_spellId = 0;
};

bool TPrioritySolverAdapter::isEmpty() const
{
    return m_setting.text.isEmpty() && m_setting.action.isEmpty() && m_setting.objectId == 0 && m_setting.passiveAbility == 0
        && m_setting.multiActions.empty();
}

bool TPrioritySolverAdapter::resolveSpell()
{
    const spells::TSpellInfoStorage::TSpellText spellText = m_spellInfoStorage->parseSpellText(m_setting.text);
    const uint16_t spellId = spellText.spell ? static_cast<uint16_t>(spellText.spell->id) : 0;
    if (spellId != 0)
        m_spellId = spellId;
    return spellId != 0;
}

uint16_t TPrioritySolverAdapter::spellId() const { return m_spellId; }

bool TPrioritySolverAdapter::hasObject() const { return m_setting.objectId != 0; }

config::EUseType TPrioritySolverAdapter::useType() const { return m_setting.useType; }

uint16_t TPrioritySolverAdapter::objectId() const { return static_cast<uint16_t>(m_setting.objectId); }

bool TPrioritySolverAdapter::hasLiquidType() const { return m_setting.liquidType.has_value() && objectId() != 0; }

uint32_t TPrioritySolverAdapter::liquidType() const { return static_cast<uint32_t>(m_setting.liquidType.value_or(0)); }

bool TPrioritySolverAdapter::hasUpgradeTier() const { return m_setting.upgradeTier.has_value() && objectId() != 0; }

uint8_t TPrioritySolverAdapter::upgradeTier() const { return static_cast<uint8_t>(m_setting.upgradeTier.value_or(0)); }

/// Picks the multi action slot a trigger runs.
class TPrioritySolver
{
public:
    TPrioritySolver(const game::TCooldownStorage* cooldownStorage, const game::TPlayerData* playerData,
                    const container::TObjectAppearanceInstanceInfoStorage* objectInfoStorage, const qint64 now, const qint64 latency)
        : m_cooldownStorage(cooldownStorage)
        , m_playerData(playerData)
        , m_objectInfoStorage(objectInfoStorage)
        , m_now(now)
        , m_latency(latency)
    {
    }

    /// The first usable slot that is ready; else the usable slot ready
    /// soonest, a later one only when it is ready sooner by more than the
    /// latency; else the first unusable slot. Empty slots never qualify.
    TPrioritySolverAdapter* solve(const std::vector<TPrioritySolverAdapter*>& candidates) const;
    /// A spell needs to be known, an object to be carried; anything else is
    /// usable.
    bool isUsable(TPrioritySolverAdapter& candidate) const;
    /// How long until the slot can run: the spell's cooldown, a rune's
    /// object cooldown, the multi use cooldown of an object used on something
    /// (yourself, the target, a chosen target, the mouse cursor); 0 otherwise.
    qint64 remainingDelay(TPrioritySolverAdapter& candidate) const;

private:
    /// How many of the slot's object the player carries (its expire
    /// counterpart's count when that is higher).
    int objectCount(TPrioritySolverAdapter& candidate) const;

    const game::TCooldownStorage* m_cooldownStorage;
    const game::TPlayerData* m_playerData;
    const container::TObjectAppearanceInstanceInfoStorage* m_objectInfoStorage;
    qint64 m_now;
    qint64 m_latency;
};

TPrioritySolverAdapter* TPrioritySolver::solve(const std::vector<TPrioritySolverAdapter*>& candidates) const
{
    qint64 best = std::numeric_limits<qint64>::max();
    TPrioritySolverAdapter* chosen = nullptr;
    for (TPrioritySolverAdapter* candidate : candidates) {
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

bool TPrioritySolver::isUsable(TPrioritySolverAdapter& candidate) const
{
    if (candidate.resolveSpell()) {
        const std::unordered_set<uint16_t>& knownSpells = m_playerData->knownSpells();
        return knownSpells.find(candidate.spellId()) != knownSpells.end();
    }
    return !candidate.hasObject() || objectCount(candidate) > 0;
}

qint64 TPrioritySolver::remainingDelay(TPrioritySolverAdapter& candidate) const
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

int TPrioritySolver::objectCount(TPrioritySolverAdapter& candidate) const
{
    if (!candidate.hasObject())
        return 0;
    const uint16_t objectId = candidate.objectId();
    const auto* type = tibia::game::rawThingType(objectId, ThingCategoryItem);
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

TActionButtonGameActionHandler::TActionButtonGameActionHandler(spells::TSpellInfoStorage* spellInfoStorage,
                                                               std::shared_ptr<game::TCooldownStorage> cooldownStorage,
                                                               std::shared_ptr<game::TPlayerData> playerData,
                                                               std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> objectInfoStorage,
                                                               QObject* parent)
    : cip::tibia::input::TActionButtonGameActionHandler(parent)
    , m_spellInfoStorage(spellInfoStorage)
    , m_cooldownStorage(std::move(cooldownStorage))
    , m_playerData(std::move(playerData))
    , m_objectInfoStorage(std::move(objectInfoStorage))
{
}

TActionButtonGameActionHandler::~TActionButtonGameActionHandler() = default;

void TActionButtonGameActionHandler::handleGameAction(const IGameActionPtr& pGameAction)
{
    if (pGameAction->gameActionType() != EGameAction::ACTION_BUTTON_TRIGGERED)
        return;
    handleActionButtonTriggeredGameAction(std::dynamic_pointer_cast<const TGameActionActionButtonTriggered>(pGameAction));
}

void TActionButtonGameActionHandler::handleActionButtonTriggeredGameAction(
    const std::shared_ptr<const TGameActionActionButtonTriggered>& pGameAction)
{
    config::TActionSetting setting = config::THotkeyOptions::instance()->mapping(pGameAction->actionBarId(), pGameAction->actionButtonId());
    // A held key repeats single actions only.
    if (!setting.multiActions.empty() && pGameAction->autoRepeat())
        return;
    setting = actionToExecute(setting);
    IGameActionPtr action = TKeyboardShortcutHandler::createGameAction(setting);
    if (!action)
        return;
    Q_EMIT publishGameAction(action);
    Q_EMIT publishGameAction(std::make_shared<TGameActionCloseAllMultiActionButtons>());
}

config::TActionSetting TActionButtonGameActionHandler::actionToExecute(const config::TActionSetting& setting) const
{
    if (setting.multiActions.empty())
        return setting;
    std::vector<TPrioritySolverAdapter> adapters;
    adapters.reserve(setting.multiActions.size());
    std::vector<TPrioritySolverAdapter*> candidates;
    for (const config::TActionSetting& slot : setting.multiActions) {
        adapters.emplace_back(m_spellInfoStorage, slot);
        candidates.push_back(&adapters.back());
    }
    // Host: the clock and the ping are qtclient statics.
    const qint64 latency = std::min<qint64>(tibia::game::getPing() / 2, 400);
    const TPrioritySolver solver(m_cooldownStorage.get(), m_playerData.get(), m_objectInfoStorage.get(), game::TCooldownStorage::now(),
                                 latency);
    const TPrioritySolverAdapter* chosen = solver.solve(candidates);
    return chosen ? chosen->setting() : setting;
}

}
