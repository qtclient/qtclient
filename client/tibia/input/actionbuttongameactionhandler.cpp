#include "input/actionbuttongameactionhandler.h"

#include "config/hotkeyoptions.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "game/cooldownstorage.h"
#include "game/playerdata.h"
#include "input/keyboardshortcuthandler.h"
#include "input/multiactionprioritysolver.h"
#include "spells/spellinfostorage.h"
#include "iclockservice.h"

#include "game/currentgamesession.h"
#include "game/gameservergamesession.h"
#include "network/networkqualityprotocolmessagehandler.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace tibia::input {

namespace {

/// One slot of a multi action as the priority solver asks it: the slot's
/// setting and the spell its text casts (looked up on demand).
class TPrioritySolverAdapter final : public TMultiActionPrioritySolver::IAction
{
public:
    TPrioritySolverAdapter(const spells::TSpellInfoStorage* spellInfoStorage, config::TActionSetting setting)
        : m_spellInfoStorage(spellInfoStorage)
        , m_setting(std::move(setting))
    {
    }

    /// No text, no action, no object, no passive ability and no slots.
    bool isEmpty() const override;
    /// Whether the text casts a spell; remembers the spell.
    bool resolveSpell() override;
    /// The spell resolveSpell found.
    uint16_t spellId() const override;
    bool hasObject() const override;
    config::EUseType useType() const override;
    uint16_t objectId() const override;
    bool hasLiquidType() const override;
    uint32_t liquidType() const override;
    bool hasUpgradeTier() const override;
    uint8_t upgradeTier() const override;

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

}

TActionButtonGameActionHandler::TActionButtonGameActionHandler(spells::TSpellInfoStorage* spellInfoStorage,
                                                               std::shared_ptr<game::TCooldownStorage> cooldownStorage,
                                                               std::shared_ptr<game::TPlayerData> playerData,
                                                               std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> objectInfoStorage,
                                                               std::shared_ptr<shared::IClockService> clockService, QObject* parent)
    : cip::tibia::input::TActionButtonGameActionHandler(parent)
    , m_spellInfoStorage(spellInfoStorage)
    , m_cooldownStorage(std::move(cooldownStorage))
    , m_playerData(std::move(playerData))
    , m_objectInfoStorage(std::move(objectInfoStorage))
    , m_clockService(std::move(clockService))
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
    std::vector<TMultiActionPrioritySolver::IAction*> candidates;
    for (const config::TActionSetting& slot : setting.multiActions) {
        adapters.emplace_back(m_spellInfoStorage, slot);
        candidates.push_back(&adapters.back());
    }
    // The live game server session's round trip; -1 without one.
    const auto* liveSession = dynamic_cast<const tibia::game::TGameserverGameSession*>(tibia::game::currentGameSession());
    const qint64 ping = liveSession ? liveSession->latency().milliseconds : -1;
    const qint64 latency = std::min<qint64>(ping / 2, 400);
    const TMultiActionPrioritySolver solver(m_cooldownStorage.get(), m_playerData.get(), m_objectInfoStorage.get(),
                                            m_clockService->nowMillis(), latency);
    const auto* chosen = static_cast<const TPrioritySolverAdapter*>(solver.solve(candidates));
    return chosen ? chosen->setting() : setting;
}

}
