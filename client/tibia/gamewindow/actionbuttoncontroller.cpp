#include "actionbuttoncontroller.h"

#include "gamewindow/actionbarcontroller.h"
#include "game/gamesessionbase.h"
#include "input/gameaction.h"
#include "input/gameactioncontext.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using Base = cip::tibia::gamewindow::TActionButtonController;
using Setting = config::TActionSetting;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
using A = input::EGameAction;

bool isMultiUse(const int objectId)
{
    const TAppearanceTypePtr& type = tibia::game::thingType(objectId, ThingCategoryItem);
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

TActionButtonController::TActionButtonController(TActionBarController* bar, const int buttonId, const int multiActionIndex, QObject* parent)
    : Base(parent)
    , m_bar(bar)
{
    m_actionBarID = uint(bar->actionBarId());
    m_actionButtonID = uint(buttonId);
    m_multiActionIndex = multiActionIndex;
    m_isEnabled = true;
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
    m_showsCooldown = false;
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
        auto* child = new TActionButtonController(m_bar, int(m_actionButtonID), i, this);
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
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    m_cooldownStorage = session ? session->cooldownStorage() : nullptr;
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
    if (const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId)) {
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

void TActionButtonController::setPassiveAbility(const int passiveAbility)
{
    if (m_type == PASSIVEABILITY && m_passiveAbility == passiveAbility)
        return;
    reset();
    clearMultiActions();
    m_type = PASSIVEABILITY;
    m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(passiveAbility);
    m_passiveAbility = passiveAbility;
    Q_EMIT changed();
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
        if (const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(m_spellId)) {
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
        // CIP : the ability's name; qtclient has no passive ability data.
        rows += row("actionbar_tooltip_label_passiveability", QString());
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

game::TCooldown TActionButtonController::currentCooldown() const
{
    const game::TCooldownStorage* storage = m_cooldownStorage.get();
    if (!storage)
        return {};
    if (m_type == SPELL)
        return storage->spellCooldown(m_spellId);
    if (m_type == PASSIVEABILITY)
        return storage->passiveCooldown();
    if (m_type == OBJECT) {
        // A rune waits for the multi-use cooldown and its spell's.
        if (spells::TSpellInfoStorage::instance()->runeSpellOfObject(int(m_objectID)))
            return storage->objectCooldown(uint16_t(int(m_objectID)), game::TCooldownStorage::now());
        if (isMultiUse(int(m_objectID)))
            return storage->multiUseCooldown();
    }
    return {};
}

bool TActionButtonController::isUsable() const
{
    if (m_type == SPELL) {
        if (!tibia::game::isOnline())
            return false;
        const std::vector<uint16_t>& learned = tibia::game::localPlayerSpells();
        return std::find(learned.begin(), learned.end(), uint16_t(m_spellId)) != learned.end();
    }
    if (m_type == OBJECT)
        return m_objectCount > 0;
    return true;
}

qint64 TActionButtonController::remainingCooldown(const qint64 now) const
{
    const game::TCooldownStorage* storage = m_cooldownStorage.get();
    if (!storage)
        return 0;
    game::TCooldown cooldown;
    if (m_type == SPELL) {
        cooldown = storage->spellCooldown(m_spellId);
    } else if (m_type == PASSIVEABILITY) {
        cooldown = storage->passiveCooldown();
    } else if (m_type == OBJECT) {
        if (spells::TSpellInfoStorage::instance()->runeSpellOfObject(int(m_objectID))) {
            cooldown = storage->objectCooldown(uint16_t(int(m_objectID)), now);
        } else {
            switch (m_useType) {
            case config::EUseType::UseOnYourself:
            case config::EUseType::UseOnTarget:
            case config::EUseType::SelectUseTarget:
            case config::EUseType::UseAtMouseCursor:
                cooldown = storage->multiUseCooldown();
                break;
            default:
                break;
            }
        }
    }
    return std::max<qint64>(0, cooldown.end - now);
}

TActionButtonController* TActionButtonController::currentMultiAction(const qint64 now) const
{
    const int ping = tibia::game::getPing();
    const qint64 window = std::min<qint64>(std::max(ping, 0) / 2, 400);
    qint64 best = std::numeric_limits<qint64>::max();
    TActionButtonController* candidate = nullptr;
    for (TActionButtonController* child : m_children) {
        if (child->m_type == EMPTY)
            continue;
        if (!child->isUsable()) {
            if (!candidate)
                candidate = child;
            continue;
        }
        const qint64 remaining = child->remainingCooldown(now);
        if (remaining == 0)
            return child;
        if (window + remaining < best) {
            candidate = child;
            best = remaining;
        }
    }
    return candidate;
}

void TActionButtonController::refreshShownSlot(const qint64 now)
{
    TActionButtonController* slot = currentMultiAction(now);
    if (slot == m_shownSlot)
        return;
    m_shownSlot = slot;
    Q_EMIT changed();
    Q_EMIT appearanceChanged();
    Q_EMIT cooldownRemainingPercentChanged();
    Q_EMIT cooldownTextChanged();
}

bool TActionButtonController::refreshCooldown(const qint64 now)
{
    bool running = false;
    for (TActionButtonController* child : m_children)
        running = child->refreshCooldown(now) || running;
    if (!m_children.empty())
        refreshShownSlot(now);

    const game::TCooldown cooldown = currentCooldown();
    const qint64 remaining = cooldown.end - now;
    uint percent = 0;
    QString text;
    if (remaining > 0 && cooldown.duration > 0) {
        const double fraction = std::min(1.0, double(remaining) / double(cooldown.duration));
        percent = uint(std::floor(fraction * 100.0 / 5.0) * 5.0);
        text = cooldownText(remaining);
        running = true;
    }
    if (percent != m_cooldownRemainingPercent) {
        m_cooldownRemainingPercent = percent;
        Q_EMIT cooldownRemainingPercentChanged();
    }
    if (text != m_cooldownText) {
        m_cooldownText = text;
        Q_EMIT cooldownTextChanged();
    }
    return running;
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
