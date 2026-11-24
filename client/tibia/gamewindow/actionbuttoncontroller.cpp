#include "actionbuttoncontroller.h"

#include "appearances/appearanceinstance.h"
#include "gamewindow/actionbarcontroller.h"
#include "game/gamesessionbase.h"
#include "game/passiveabilityinfostorage.h"
#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"
#include "input/multiactionprioritysolver.h"
#include "qmlcomponents/appearanceinstancerenderer.h"
#include "spells/spellinfostorage.h"
#include "iclockservice.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using Base = cip::tibia::gamewindow::TActionButtonController;
using Setting = config::TActionSetting;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
using A = input::EGameAction;

bool isMultiUse(const int objectId)
{
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, objectId);
    return type && type->isMultiUse();
}

/// CIP .
QString row(const char* labelId, const QString& value)
{
    return QStringLiteral("<tr>\n  <td style=\"text-align:right; padding-right:%3px\">%1:</td>\n  <td >%2</td>\n</tr>\n")
        .arg(qtTrId(labelId), value, QString::number(5));
}

/// CIP .
QString durationText(const qint64 seconds)
{
    if (seconds < 60)
        return qtTrId("time_second_short").arg(seconds);
    if (seconds % 60 == 0)
        return qtTrId("time_minutes_short").arg(seconds / 60);
    return qtTrId("time_minutes_and_seconds_short").arg(seconds / 60).arg(seconds % 60);
}

}

TActionButtonController::TActionButtonController(spells::TSpellInfoStorage* SpellInfoStorage,
                                                 game::TPassiveAbilityInfoStorage* PassiveAbilityInfoStorage, const uint8_t ActionBarID,
                                                 const uint ActionButtonID, QObject* parent)
    : Base(parent)
    , m_spellInfoStorage(SpellInfoStorage)
    , m_passiveAbilityInfoStorage(PassiveAbilityInfoStorage)
{
    m_actionBarID = ActionBarID;
    m_actionButtonID = ActionButtonID;
    m_multiActionIndex = -1;
    m_isEnabled = true;
    if (m_spellInfoStorage) {
        connect(m_spellInfoStorage, &spells::TSpellInfoStorage::spellModificationsApplied, this, &TActionButtonController::onSpellDataModified);
        connect(m_spellInfoStorage, &spells::TSpellInfoStorage::activeSpellsChanged, this, &Base::highlightChanged);
    }
    connect(this, &Base::changed, this, &Base::highlightChanged);
}

void TActionButtonController::onSpellDataModified()
{
    if (m_type == SPELL)
        Q_EMIT changed();
}

void TActionButtonController::setCastType(const int castType)
{
    if (m_castType == castType)
        return;
    m_castType = castType;
    Q_EMIT changed();
}

void TActionButtonController::setTutorialMarkerID(const int tutorialMarkerID)
{
    if (m_tutorialMarkerID == tutorialMarkerID)
        return;
    m_tutorialMarkerID = tutorialMarkerID;
    Q_EMIT tutorialMarkerIDChanged();
}

void TActionButtonController::reset()
{
    m_type = EMPTY;
    m_progressType = COOLDOWN;
    m_spellId = 0;
    m_passiveAbility = 0;
    m_text.clear();
    m_spellParameters.clear();
    m_spellIconUrl.clear();
    m_passiveAbilityIconUrl.clear();
    m_sendAutomatically = false;
    m_castType = 0;
    m_objectID = 0;
    m_objectInstance = {};
    m_objectAppearanceInstance.reset();
    m_objectUpgradeTier = 0;
    m_liquidType = Info::ELiquidType::EMPTY;
    m_hookDirection = Info::EHookDirection::NONE;
    m_useType = config::EUseType::SelectUseTarget;
    m_isEquipeAction = false;
    m_objectIsEquiped = false;
    m_isEnabled = true;
    if (m_cooldownRemainingPercent != 0) {
        m_cooldownRemainingPercent = 0;
        Q_EMIT cooldownRemainingPercentChanged();
    }
    if (!m_cooldownText.isEmpty()) {
        m_cooldownText.clear();
        Q_EMIT cooldownTextChanged();
    }
    if (m_isNewlyUnlocked) {
        m_isNewlyUnlocked = false;
        Q_EMIT changed();
    }
    m_objectCount = 0;
    m_cooldownEnd = 0;
    for (TActionButtonController* child : m_children)
        child->reset();
    Q_EMIT changed();
    Q_EMIT appearanceChanged();
}

void TActionButtonController::clearMultiActions()
{
    if (!m_multiActionExpanded && m_children.empty())
        return;
    m_multiActionExpanded = false;
    m_shownSlot = nullptr;
    m_multiActions.clear();
    for (TActionButtonController* child : std::exchange(m_children, {}))
        child->deleteLater();
    Q_EMIT changed();
    Q_EMIT multiActionExpandedChanged();
}

void TActionButtonController::createMultiActions(const int count)
{
    if (!m_children.empty())
        return;
    reset();
    for (int i = 0; i < count; ++i) {
        auto* child = new TActionButtonController(m_spellInfoStorage, m_passiveAbilityInfoStorage, uint8_t(m_actionBarID), m_actionButtonID, this);
        child->m_multiActionIndex = i;
        child->connectToGameSession(m_gameSession);
        connect(child, &Base::publishGameAction, this, &Base::publishGameAction);
        connect(child, &Base::changed, this, &Base::changed);
        connect(child, &Base::cooldownRemainingPercentChanged, this, &Base::cooldownRemainingPercentChanged);
        connect(child, &Base::cooldownTextChanged, this, &Base::cooldownTextChanged);
        connect(child, &Base::appearanceChanged, this, &Base::appearanceChanged);
        connect(child, &TActionButtonController::requestContextMenu, this, &TActionButtonController::requestContextMenu);
        m_children.push_back(child);
        m_multiActions << child;
    }
    Q_EMIT changed();
}

void TActionButtonController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession.reset();
    m_clock.reset();
    m_cooldownStorage.reset();
    m_playerData.reset();
    m_objectInfoStorage.reset();
    m_appearanceInstanceFactory.reset();
    if (GameSession) {
        m_gameSession = GameSession;
        m_clock = m_gameSession->clockService();
        if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
            m_cooldownStorage = session->cooldownStorage();
            m_playerData = session->playerData();
            m_objectInfoStorage = session->objectAppearanceInstanceInfoStorage();
            m_appearanceInstanceFactory = session->appearanceInstanceFactory();
        }
    }
    // Host: the slots show their own cooldowns, so they follow the session too.
    for (TActionButtonController* child : m_children)
        child->connectToGameSession(m_gameSession);
}

void TActionButtonController::setSpell(const int spellId, const QString& parameters, const int castType)
{
    if (m_type == SPELL && m_spellId == spellId && m_spellParameters == parameters && m_castType == castType)
        return;
    reset();
    clearMultiActions();
    m_type = SPELL;
    m_spellId = spellId;
    m_spellParameters = parameters;
    m_castType = castType;
    if (const spells::TSpellInfo* spell = m_spellInfoStorage ? m_spellInfoStorage->spell(spellId) : nullptr) {
        m_spellIconUrl = spells::TSpellInfoStorage::iconUrl(*spell);
        m_text = parameters.isEmpty() ? spell->words : spell->words + QLatin1Char(' ') + parameters;
    }
    m_sendAutomatically = true;
    Q_EMIT changed();
}

void TActionButtonController::setText(const QString& text, const bool sendAutomatically)
{
    if (m_type == TEXT && m_text == text && m_sendAutomatically == sendAutomatically)
        return;
    reset();
    clearMultiActions();
    m_type = TEXT;
    m_text = text;
    m_sendAutomatically = sendAutomatically;
    Q_EMIT changed();
}

void TActionButtonController::setObject(const appearances::TObjectAppearanceInstanceTypeID& objectInstance, const config::EUseType useType,
                                        const bool useEquipSmartMode)
{
    const int upgradeTier = objectInstance.hasTier() ? objectInstance.tier : 0;
    const int liquidType = objectInstance.hasLiquid() ? int(objectInstance.liquid) : 0;
    const bool sameObject = m_type == OBJECT && m_objectInstance.typeId == objectInstance.typeId && m_objectInstance.kind == objectInstance.kind
                            && m_objectInstance.liquid == objectInstance.liquid && m_objectInstance.tier == objectInstance.tier;
    if (sameObject && m_useType == useType && m_useEquipSmartMode == useEquipSmartMode)
        return;
    reset();
    clearMultiActions();
    m_type = OBJECT;
    m_objectInstance = objectInstance;
    m_objectID = uint(objectInstance.typeId);
    m_objectUpgradeTier = uint(upgradeTier);
    m_liquidType = Info::ELiquidType(liquidType);
    m_useType = useType;
    m_useEquipSmartMode = useEquipSmartMode;
    m_isEquipeAction = useType == config::EUseType::Equip;
    Q_EMIT changed();
    Q_EMIT appearanceChanged();
}

void TActionButtonController::setObjectCount(const uint objectCount)
{
    if (m_objectCount == objectCount)
        return;
    m_objectCount = objectCount;
    Q_EMIT changed();
    Q_EMIT appearanceChanged();
}

void TActionButtonController::setObjectAppearanceInstance(const std::shared_ptr<appearances::TAppearanceInstance>& instance)
{
    if (m_objectAppearanceInstance) {
        const bool sameType = instance && m_objectAppearanceInstance->id() == instance->id()
                              && m_objectAppearanceInstance->category() == instance->category();
        if (!sameType || m_objectAppearanceInstance == instance)
            return;
    }
    m_objectAppearanceInstance = instance;
    Q_EMIT appearanceChanged();
}

void TActionButtonController::setObjectInfoName(const QString& objectInfoName)
{
    if (m_type != OBJECT || m_text == objectInfoName)
        return;
    m_text = objectInfoName;
    Q_EMIT changed();
}

QString TActionButtonController::objectTooltipName() const
{
    const TActionButtonController* slot = shown();
    if (slot->m_type != OBJECT || slot->m_text.isEmpty())
        return qtTrId("actionbar_tooltip_object_name_dummy");
    const int tier = slot->m_objectInstance.hasTier() ? slot->m_objectInstance.tier : 0;
    if (tier == 0)
        return slot->m_text;
    return qtTrId("actionbar_tooltip_object_name_with_tier").arg(slot->m_text).arg(tier);
}

/// A passive ability with a cooldown counts it down anchored, one without a
/// counter; an unknown one keeps the cooldown progress.
void TActionButtonController::setPassiveAbility(const int passiveAbility)
{
    if (m_passiveAbility == passiveAbility)
        return;
    reset();
    clearMultiActions();
    m_type = PASSIVEABILITY;
    if (const game::TPassiveAbilityInfoStorage* storage = m_passiveAbilityInfoStorage) {
        if (const std::optional<game::TPassiveAbilityInfo> info = storage->findInfo(static_cast<uint8_t>(passiveAbility))) {
            if (!info->flag)
                m_progressType = ANCHORED_COOLDOWN;
            else
                m_progressType = COUNTER_COOLDOWN;
        }
    }
    // Host: the QML reads the icon from the passive ability icon provider.
    m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(passiveAbility);
    m_passiveAbility = passiveAbility;
    Q_EMIT changed();
}

QString TActionButtonController::passiveAbilityName() const
{
    const TActionButtonController* slot = shown();
    const game::TPassiveAbilityInfoStorage* storage = m_passiveAbilityInfoStorage;
    if (!storage || !game::TPassiveAbilityInfoStorage::isKnownPassiveAbility(static_cast<uint8_t>(slot->m_passiveAbility)))
        return {};
    return storage->info(static_cast<uint8_t>(slot->m_passiveAbility)).name;
}

void TActionButtonController::setHotkeys(const QList<QKeySequence>& hotkeys)
{
    m_hotkeys = hotkeys;
    const QString hotkeyShort = hotkeys.isEmpty() ? QString() : shortHotkeyText(hotkeys.front());
    if (hotkeyShort == m_hotkeyShort)
        return;
    m_hotkeyShort = hotkeyShort;
    Q_EMIT changed();
}

QString TActionButtonController::shortHotkeyText(const QKeySequence& key)
{
    QString text;
    if (key.isEmpty())
        return text;
    const QString portable = key.toString(QKeySequence::PortableText);
    QStringList parts = portable.split(QStringLiteral("+"), Qt::KeepEmptyParts, Qt::CaseSensitive);
    // A key that is itself "+" leaves empty parts behind.
    if (portable.endsWith(QStringLiteral("+"))) {
        if (portable.toUpper().endsWith(QStringLiteral("N+")))
            parts.back() = QStringLiteral("N+");
        else
            parts.append(QStringLiteral("+"));
    }
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (i == parts.size() - 1) {
            text.append(parts[i]);
            break;
        }
        if (!parts[i].isEmpty())
            text.append(parts[i].toUpper().at(0));
    }
    return text;
}

QString TActionButtonController::tooltip() const
{
    QString text;
    if (!isMultiActionChild())
        text = qtTrId("actionbar_button_identifier").arg(m_actionBarID).arg(m_actionButtonID, 2, 10, QLatin1Char('0')) + QStringLiteral("<br>\n");
    text += QStringLiteral("<table>\n");
    text += shown()->tooltipRows();
    text += QStringLiteral("</table>\n");
    return text;
}

QString TActionButtonController::tooltipRows() const
{
    QString rows;
    switch (m_type) {
    case EMPTY:
        rows += row("actionbar_tooltip_label_action", qtTrId("none"));
        break;
    case SPELL:
        if (const spells::TSpellInfo* spell = m_spellInfoStorage ? m_spellInfoStorage->spell(m_spellId) : nullptr) {
            rows += row("actionbar_tooltip_label_action", qtTrId("actionbar_tooltip_spell_action").arg(spell->name));
            QString formula = spell->words;
            if (!m_spellParameters.isEmpty())
                formula += QLatin1Char(' ') + m_spellParameters.toHtmlEscaped();
            rows += row("actionbar_tooltip_label_formula", formula);
            rows += row("cooldown", durationText(spell->cooldownSelf));
            rows += row("mana", spell->mana == 0 ? qtTrId("actionbar_tooltip_mana_variable") : QString::number(spell->mana));
            if (spell->soul != 0)
                rows += row("soul_points", QString::number(spell->soul));
        }
        break;
    case OBJECT: {
        const char* actionId = nullptr;
        switch (m_useType) {
        case config::EUseType::Use:
            actionId = "actionbar_tooltip_use_object";
            break;
        case config::EUseType::UseOnYourself:
            actionId = "actionbar_tooltip_use_object_on_yourself";
            break;
        case config::EUseType::UseOnTarget:
            actionId = "actionbar_tooltip_use_object_on_target";
            break;
        case config::EUseType::SelectUseTarget:
            actionId = "actionbar_tooltip_use_object_with_crosshair";
            break;
        case config::EUseType::Equip:
            actionId = m_objectIsEquiped ? "actionbar_tooltip_unequip_object" : "actionbar_tooltip_equip_object";
            break;
        case config::EUseType::UseAtMouseCursor:
            actionId = "actionbar_tooltip_use_object_at_mousecursor";
            break;
        }
        const QString action = actionId ? qtTrId(actionId).arg(objectTooltipName()) : QString();
        rows += row("actionbar_tooltip_label_action", action);
        rows += row("actionbar_tooltip_label_amount", QString::number(m_objectCount));
        break;
    }
    case TEXT:
        rows += row("actionbar_tooltip_label_action", qtTrId("actionbar_tooltip_text_action").arg(m_text.toHtmlEscaped()));
        rows += row("actionbar_tooltip_label_sendautomatically", qtTrId(m_sendAutomatically ? "yes" : "no"));
        break;
    case PASSIVEABILITY:
        rows += row("actionbar_tooltip_label_passiveability", passiveAbilityName());
        break;
    }
    if (!isMultiActionChild()) {
        // CIP : each key, "<br>\n" between them.
        QStringList keys;
        for (const QKeySequence& key : m_hotkeys)
            keys.push_back(key.toString(QKeySequence::PortableText));
        rows += row("actionbar_tooltip_label_hotkeys", keys.isEmpty() ? qtTrId("none") : keys.join(QStringLiteral("<br>\n")));
    }
    return rows;
}

void TActionButtonController::setIsEnabled(const bool isEnabled)
{
    if (m_isEnabled == isEnabled)
        return;
    m_isEnabled = isEnabled;
    Q_EMIT changed();
}

void TActionButtonController::setObjectIsEquiped(const bool objectIsEquiped)
{
    if (m_objectIsEquiped == objectIsEquiped)
        return;
    m_objectIsEquiped = objectIsEquiped;
    Q_EMIT changed();
}

namespace {

/// A slot of a multi-action button as the multi action priority solver asks
/// it. Every answer reads the action the slot shows: the slot itself, or,
/// while it holds slots of its own, its current slot followed down.
class TPrioritySolverAdapter final : public input::TMultiActionPrioritySolver::IAction
{
public:
    explicit TPrioritySolverAdapter(TActionButtonController* button)
        : m_button(button)
    {
    }

    TActionButtonController* button() const { return m_button; }

    bool isEmpty() const override;
    bool resolveSpell() override;
    uint16_t spellId() const override;
    bool hasObject() const override;
    config::EUseType useType() const override;
    uint16_t objectId() const override;
    /// The object is a fluid container or a splash.
    bool hasLiquidType() const override;
    uint32_t liquidType() const override;
    /// The object has an upgrade classification.
    bool hasUpgradeTier() const override;
    uint8_t upgradeTier() const override;

private:
    const TActionButtonController* action() const;

    TActionButtonController* m_button;
};

const TActionButtonController* TPrioritySolverAdapter::action() const
{
    const TActionButtonController* button = m_button;
    while (button->isMultiAction()) {
        const TActionButtonController* slot = button->currentMultiAction();
        if (!slot)
            break;
        button = slot;
    }
    return button;
}

bool TPrioritySolverAdapter::isEmpty() const { return action()->type() == TActionButtonController::EMPTY; }

bool TPrioritySolverAdapter::resolveSpell() { return action()->type() == TActionButtonController::SPELL; }

uint16_t TPrioritySolverAdapter::spellId() const { return static_cast<uint16_t>(action()->spellId()); }

bool TPrioritySolverAdapter::hasObject() const { return action()->type() == TActionButtonController::OBJECT; }

config::EUseType TPrioritySolverAdapter::useType() const { return action()->useType(); }

uint16_t TPrioritySolverAdapter::objectId() const { return static_cast<uint16_t>(action()->objectID()); }

bool TPrioritySolverAdapter::hasLiquidType() const
{
    if (!hasObject())
        return false;
    const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, objectId());
    return type && (type->isFluidContainer() || type->isSplash());
}

uint32_t TPrioritySolverAdapter::liquidType() const
{
    return qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getLiquidType(action()->liquidType());
}

bool TPrioritySolverAdapter::hasUpgradeTier() const
{
    if (!hasObject())
        return false;
    const TAppearanceType* type = tibia::appearances::TAppearancesManager::instance().findAppearanceType(ThingCategoryItem, objectId());
    return type && type->hasUpgradeClassification();
}

uint8_t TPrioritySolverAdapter::upgradeTier() const { return static_cast<uint8_t>(action()->objectUpgradeTier()); }

}

TActionButtonController* TActionButtonController::currentMultiAction() const
{
    if (!m_gameSession || !m_cooldownStorage || !m_playerData || !m_objectInfoStorage || !m_clock)
        return nullptr;
    std::vector<TPrioritySolverAdapter> adapters;
    adapters.reserve(m_children.size());
    std::vector<input::TMultiActionPrioritySolver::IAction*> candidates;
    for (TActionButtonController* child : m_children) {
        adapters.emplace_back(child);
        candidates.push_back(&adapters.back());
    }
    // Host: the ping is a qtclient static.
    const qint64 latency = std::min<qint64>(tibia::game::getPing() / 2, 400);
    const input::TMultiActionPrioritySolver solver(m_cooldownStorage.get(), m_playerData.get(), m_objectInfoStorage.get(),
                                                   m_clock->nowMillis(), latency);
    const auto* chosen = static_cast<const TPrioritySolverAdapter*>(solver.solve(candidates));
    return chosen ? chosen->button() : nullptr;
}

void TActionButtonController::setCooldown(const qint64 duration, const qint64 end)
{
    if (m_cooldownDuration == duration && m_cooldownEnd == end)
        return;
    m_cooldownDuration = duration;
    m_cooldownEnd = end;
}

void TActionButtonController::setAnchoredCooldown(const game::TPassiveAbilityCooldown& cooldown)
{
    if (m_anchoredCooldown == cooldown)
        return;
    m_anchoredCooldown = cooldown;
    Q_EMIT changed();
}

void TActionButtonController::setCounterCooldown(const game::TPassiveAbilityCounter& counter)
{
    if (m_counterCooldown == counter)
        return;
    m_counterCooldown = counter;
    Q_EMIT changed();
}

void TActionButtonController::setRemainingCooldown(const qint64 remaining)
{
    if (remaining == m_remainingCooldown)
        return;
    m_remainingCooldown = remaining;
    uint percent = 0;
    if (m_cooldownDuration >= 1)
        percent = uint(qint64(std::min(1.0, double(remaining) / double(m_cooldownDuration)) * 100.0));
    setCooldownRemainingPercent(percent);
    setCooldownText(cooldownText(remaining));
}

void TActionButtonController::setCooldownRemainingPercent(const uint percent)
{
    const uint steps = percent / 5 * 5;
    if (m_cooldownRemainingPercent == steps)
        return;
    m_cooldownRemainingPercent = steps;
    Q_EMIT cooldownRemainingPercentChanged();
}

void TActionButtonController::setCooldownText(const QString& text)
{
    if (m_cooldownText == text)
        return;
    m_cooldownText = text;
    Q_EMIT cooldownTextChanged();
}

void TActionButtonController::updateCooldown(const qint64 now)
{
    switch (m_progressType) {
    case COUNTER_COOLDOWN: {
        setCooldownRemainingPercent(uint(qint64(m_counterCooldown.fraction() * 100.0)));
        const int remaining = m_counterCooldown.remaining();
        setCooldownText(remaining < 1 ? QString() : QStringLiteral("%1").arg(remaining));
        break;
    }
    case COOLDOWN:
        setRemainingCooldown(std::max<qint64>(0, m_cooldownEnd - now));
        break;
    case ANCHORED_COOLDOWN:
        setCooldownRemainingPercent(uint(qint64(m_anchoredCooldown.fractionAt(now) * 100.0)));
        setCooldownText(cooldownText(m_anchoredCooldown.remainingAt(now)));
        break;
    }
    // A multi-action button shows the slot that would run now.
    TActionButtonController* const slot = m_children.empty() ? nullptr : currentMultiAction();
    if (slot == m_shownSlot)
        return;
    m_shownSlot = slot;
    Q_EMIT changed();
    Q_EMIT appearanceChanged();
    // Host: the cooldown getters read the shown slot.
    Q_EMIT cooldownRemainingPercentChanged();
    Q_EMIT cooldownTextChanged();
}

bool TActionButtonController::hasHighlight() const
{
    return m_spellInfoStorage && m_spellInfoStorage->isSpellUnlocked(static_cast<uint16_t>(spellId()));
}

QString TActionButtonController::cooldownText(const qint64 remainingMs)
{
    // Seconds and tenths are cut, not rounded; the hours form does not pad the minutes.
    if (remainingMs < 11000)
        return QStringLiteral("%1.%2").arg(remainingMs / 1000).arg(remainingMs / 100 - (remainingMs / 1000) * 10, 1, 10, QLatin1Char('0'));
    if (remainingMs < 60000)
        return QStringLiteral("%1").arg(remainingMs / 1000);
    if (remainingMs < 660000)
        return QStringLiteral("%1:%2").arg(remainingMs / 60000).arg((remainingMs / 1000) % 60, 2, 10, QLatin1Char('0'));
    if (remainingMs < 3660000)
        return QStringLiteral("%1m").arg(remainingMs / 60000);
    if (remainingMs < 36000000)
        return QStringLiteral("%1h%2").arg(remainingMs / 3600000).arg(remainingMs / 60000 - (remainingMs / 3600000) * 60);
    return QStringLiteral("%1h").arg(remainingMs / 3600000);
}

void TActionButtonController::onTriggered()
{
    // A multi-action slot triggers its button, which runs its current slot.
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionActionButtonTriggered>(static_cast<uint8_t>(m_actionBarID), int(m_actionButtonID)));
}

void TActionButtonController::contextMenuRequested()
{
    Q_EMIT requestContextMenu(m_actionButtonID, m_multiActionIndex);
}

void TActionButtonController::onTargetSelected()
{
    // The button picked as a target (assigning a spell from the magical archive).
    const auto button = std::make_shared<input::TLocationActionButtonGameActionContext>(static_cast<uint8_t>(m_actionBarID),
                                                                                          int(m_actionButtonID), multiActionIndex());
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionTargetSelected>(input::TObjectGameActionContext(button, button)));
}

void TActionButtonController::onDragChanged(bool)
{
    // Dragging the button ends its "newly unlocked" marker.
    if (m_isNewlyUnlocked)
        setIsNewlyUnlocked(false);
}

}
