#include "assignactiondialogs.h"

#include "game/gameciptypes.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/smoothfiltering.h"

#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/actionbarmanager.h"
#include "gamewindow/mousecursorcontroller.h"
#include "config/hotkeyoptions.h"
#include "input/eventfiltermanager.h"
#include "input/gameactionhandlers.h"
#include "game/playerdata.h"
#include "game/tgamenotify.h"
#include "spells/spellinfostorage.h"

#include "container/containerciptypes.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <QCoreApplication>
#include <QKeyEvent>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <map>
#include <set>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;

/// "Action Button <bar>.<button>", the button with two digits.
QString actionButtonName(const int actionBarId, const int buttonId)
{
    return qtTrId("actionbar_button_identifier").arg(actionBarId).arg(buttonId, 2, 10, QLatin1Char('0'));
}

TAppearanceTypePtr itemType(const int objectId)
{
    return objectId > 0 ? tibia::game::thingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
}

bool isEquipable(const TAppearanceTypePtr& type)
{
    return type && (type->isCloth() || type->getClothSlot() > 0);
}


}

namespace {

/// A spell group's name, empty for none.
QString spellGroupName(const uint8_t group)
{
    switch (group) {
    case 1: return qtTrId("spell_attack");
    case 2: return qtTrId("spell_healing");
    case 3: return qtTrId("spell_support");
    case 4: return qtTrId("spellcooldown_spell_special");
    case 5: return qtTrId("spellcooldown_spell_conjure");
    case 6: return qtTrId("spellcooldown_spell_crippling");
    case 7: return qtTrId("spellcooldown_spell_focus");
    case 8: return qtTrId("spellcooldown_spell_ultimate_strikes");
    case 9: return qtTrId("spellcooldown_spell_great_beams");
    case 10: return qtTrId("spellcooldown_spell_bursts_of_nature");
    case 11: return qtTrId("spellcooldown_spell_stance");
    default: return {};
    }
}

/// image://<provider>/<index - 1>, empty for index 0.
QString imageUrlOfIndex(const char* provider, const uint8_t index)
{
    if (index == 0)
        return {};
    return QStringLiteral("image://%1/%2").arg(QLatin1String(provider)).arg(int(index) - 1);
}

QString vocationName(const uint8_t vocation)
{
    switch (vocation) {
    case 1: return qtTrId("knight");
    case 2: return qtTrId("paladin");
    case 3: return qtTrId("sorcerer");
    case 4: return qtTrId("druid");
    case 5: return qtTrId("monk");
    default: return qtTrId("none");
    }
}

QString damageTypeName(const uint8_t damageType)
{
    switch (damageType) {
    case 1: return qtTrId("damagetype_fire");
    case 2: return qtTrId("damagetype_earth");
    case 3: return qtTrId("damagetype_energy");
    case 4: return qtTrId("damagetype_ice");
    case 5: return qtTrId("damagetype_holy");
    case 6: return qtTrId("damagetype_death");
    case 7: return qtTrId("damagetype_healing");
    case 8: return qtTrId("damagetype_drowning");
    case 9: return qtTrId("damagetype_lifeleech");
    case 10: return qtTrId("damagetype_manadrain");
    case 11: return qtTrId("damagetype_agony");
    default: return qtTrId("damagetype_physical");
    }
}

/// A damage type, else "variable" for weapon-scaled spells, else "none".
QString damageText(const std::optional<uint8_t>& damageType, const bool weaponScaled)
{
    if (damageType)
        return damageTypeName(*damageType);
    return weaponScaled ? qtTrId("spelllist_label_damage_type_variable") : qtTrId("spelllist_label_damage_type_none");
}

/// "45s", "2min", "1:30min" style cooldown text of milliseconds.
QString cooldownText(const qint64 milliseconds)
{
    const qint64 seconds = milliseconds / 1000;
    if (seconds < 60)
        return qtTrId("time_second_short").arg(seconds);
    if (seconds % 60 == 0)
        return qtTrId("time_minutes_short").arg(seconds / 60);
    return qtTrId("time_minutes_and_seconds_short").arg(seconds / 60).arg(seconds % 60);
}

/// The group names, "none" without a group.
QString groupsText(const spells::TSpellCastParameters& cast)
{
    QStringList groups;
    if (cast.groupPrimary)
        groups << spellGroupName(cast.groupPrimary);
    if (cast.groupSecondary)
        groups << spellGroupName(cast.groupSecondary);
    if (groups.isEmpty())
        groups << qtTrId("none");
    return groups.join(QStringLiteral(", "));
}

/// The cast's own cooldown, then those of its groups.
QString cooldownsText(const spells::TSpellCastParameters& cast)
{
    QStringList cooldowns{ cooldownText(cast.cooldownSelf) };
    if (cast.groupPrimary)
        cooldowns << cooldownText(cast.cooldownPrimaryGroup);
    if (cast.groupSecondary)
        cooldowns << cooldownText(cast.cooldownSecondaryGroup);
    return cooldowns.join(QStringLiteral(" / "));
}

QString groupCooldownsText(const spells::TSpellCastParameters& cast)
{
    QStringList cooldowns;
    if (cast.groupPrimary)
        cooldowns << cooldownText(cast.cooldownPrimaryGroup);
    if (cast.groupSecondary)
        cooldowns << cooldownText(cast.cooldownSecondaryGroup);
    return cooldowns.join(QStringLiteral(" / "));
}

/// "none", "all" for every vocation, else the vocation names druid, knight,
/// paladin, sorcerer, monk in that order.
QString vocationsText(const spells::TSpellCastParameters& cast)
{
    if (cast.vocations.empty())
        return qtTrId("none");
    if (cast.vocations.size() == 5)
        return qtTrId("all");
    QStringList names;
    for (const uint8_t vocation : { 4, 1, 2, 3, 5 }) {
        if (cast.vocations.count(vocation))
            names << vocationName(vocation);
    }
    return names.join(QStringLiteral(", "));
}

/// The usage block the Magical Archive shows for a spell's cast or a rune's use.
QVariantMap usageInfo(const spells::TSpellCastParameters& cast, const bool weaponScaled)
{
    QVariantMap info;
    info.insert(QStringLiteral("groupPrimary"), spellGroupName(cast.groupPrimary));
    info.insert(QStringLiteral("groupSecondary"), spellGroupName(cast.groupSecondary));
    info.insert(QStringLiteral("groups"), groupsText(cast));
    QVariantList vocations;
    for (const uint8_t vocation : cast.vocations) {
        QVariantMap entry;
        entry.insert(QStringLiteral("iconUrl"), imageUrlOfIndex("vocation-icons", vocation));
        entry.insert(QStringLiteral("tooltip"), vocationName(vocation));
        vocations.push_back(entry);
    }
    info.insert(QStringLiteral("allowedVocationInfos"), vocations);
    info.insert(QStringLiteral("cooldown"), cooldownText(cast.cooldownSelf));
    info.insert(QStringLiteral("groupCooldowns"), groupCooldownsText(cast));
    info.insert(QStringLiteral("manaCost"), uint(cast.mana));
    info.insert(QStringLiteral("soulPointsCost"), uint(cast.soul));
    info.insert(QStringLiteral("damagetype"), damageText(cast.damageType, weaponScaled));
    info.insert(QStringLiteral("mean"), uint(cast.mean));
    return info;
}

QString scalingName(const int scaling)
{
    switch (scaling) {
    case spells::TSpellInfo::WeaponSkills: return qtTrId("spell_scaling_weaponskills");
    case spells::TSpellInfo::MagicLevel: return qtTrId("spell_scaling_magiclevel");
    case spells::TSpellInfo::DistanceSkill: return qtTrId("spell_scaling_distanceskill");
    case spells::TSpellInfo::FistSkill: return qtTrId("spell_scaling_fistskill");
    case spells::TSpellInfo::Weapon: return qtTrId("spell_scaling_weapon");
    case spells::TSpellInfo::ShieldingSkill: return qtTrId("spell_scaling_shieldingskill");
    default: return {};
    }
}

bool hasVocation(const spells::TSpellCastParameters& cast, const uint8_t vocation) { return cast.vocations.count(vocation) > 0; }

} // namespace

TSpellListDataModel::TSpellListDataModel(QObject* parent)
    : cip::tibia::gamewindow::TSpellListDataModel(parent)
{
    for (const auto& [id, spell] : spells::TSpellInfoStorage::instance()->spells())
        m_spellIds.push_back(id);
    connect(spells::TSpellInfoStorage::instance(), &spells::TSpellInfoStorage::spellModificationsApplied, this,
            &TSpellListDataModel::onSpellDataModificationsApplied);
}

int TSpellListDataModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : spells::TSpellInfoStorage::instance()->spellCount();
}

QVariant TSpellListDataModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.row() >= int(m_spellIds.size()))
        return {};
    const spells::TSpellInfo spell = spells::TSpellInfoStorage::instance()->spellInfoAt(m_spellIds[size_t(index.row())]);
    const spells::TSpellCastParameters& cast = spell.cast;
    const bool weaponScaled = spell.scalings.count(spells::TSpellInfo::Weapon) > 0;
    switch (role) {
    case EnumIdRole: return uint(spell.id);
    case NameRole: return spell.name;
    case FormulaRawRole: return spell.words;
    case ParameterHintsRole: return spell.parameterHints;
    case GroupsRole: return groupsText(cast);
    case GroupPrimaryRole: return spellGroupName(cast.groupPrimary);
    case GroupPrimaryIconUrlRole: return imageUrlOfIndex("spellgroup-icons-20x20", cast.groupPrimary);
    case GroupSecondaryRole: return spellGroupName(cast.groupSecondary);
    case IconUrlRole: return QStringLiteral("image://spell-icons-32x32/%1").arg(spell.iconIndex);
    case IconUrlSmallRole: return QStringLiteral("image://spell-icons-20x20/%1").arg(spell.iconIndex);
    case IconIndexRole: return uint(spell.iconIndex);
    case TypeRole:
        if (spell.type == spells::TSpellInfo::CreatableRune)
            return qtTrId("spelllist_spell_type_rune");
        if (spell.type == spells::TSpellInfo::Instant)
            return qtTrId("spelllist_spell_type_instant");
        return QString();
    case DamagetypeRole:
        if (spell.rune && spell.rune->cast.damageType)
            return damageTypeName(*spell.rune->cast.damageType);
        return damageText(cast.damageType, weaponScaled);
    case MinimumCasterLevelRole: return uint(cast.minimumLevel);
    case MinimumMagicLevelRole: return spell.rune ? spell.rune->useMagicLevel : 0.0;
    case IsRuneRole: return spell.rune.has_value();
    case RuneAmountRole: return uint(spell.rune ? spell.rune->amount : 0);
    case IsPremiumRole: return spell.premium;
    case PremiumOnlyRole: return spell.premium ? qtTrId("yes") : qtTrId("no");
    case AllowedVocationsRole: return vocationsText(cast);
    case ManaCostRole: return uint(cast.mana);
    case SoulPointsCostRole: return uint(spell.cast.soul);
    case CooldownsRole: return cooldownsText(cast);
    case CooldownRole: return cooldownText(cast.cooldownSelf);
    case CooldownPrimaryGroupRole: return cooldownText(cast.cooldownPrimaryGroup);
    case CooldownSecondaryGroupRole: return cooldownText(cast.cooldownSecondaryGroup);
    case GroupCooldownsRole: return groupCooldownsText(cast);
    case CastPossibleRole: return true;
    case DescriptionRole: return spell.description;
    case ScalingRole: {
        QStringList names;
        for (const int scaling : spell.scalings)
            names << scalingName(scaling);
        return names;
    }
    case SpellUsageInfoRole: return usageInfo(cast, weaponScaled);
    case RuneUsageInfoRole: return spell.rune ? usageInfo(spell.rune->cast, weaponScaled) : QVariantMap();
    case RelevantUsageInfoRole: return usageInfo(spell.rune ? spell.rune->cast : cast, weaponScaled);
    case SourceRole: {
        using ESource = cip::tibia::qmlcomponents::enums::ESpellSource;
        const ESource source = spell.source >= 1 && spell.source <= 3 ? ESource(spell.source) : ESource::SpellSourceUnknown;
        return QVariant::fromValue(source);
    }
    case IsAimAtTargetConfigurableRole: return spell.aimAtTarget;
    case IsCrossHairSpellRole: return spell.crossHairTarget;
    default: return {};
    }
}

QHash<int, QByteArray> TSpellListDataModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[EnumIdRole] = "enumId";
    roles[NameRole] = "name";
    roles[FormulaRawRole] = "formulaRaw";
    roles[ParameterHintsRole] = "parameterHints";
    roles[GroupsRole] = "groups";
    roles[GroupPrimaryRole] = "groupPrimary";
    roles[GroupPrimaryIconUrlRole] = "groupPrimaryIconUrl";
    roles[GroupSecondaryRole] = "groupSecondary";
    roles[IconUrlRole] = "iconUrl";
    roles[IconUrlSmallRole] = "iconUrlSmall";
    roles[IconIndexRole] = "iconIndex";
    roles[TypeRole] = "type";
    roles[DamagetypeRole] = "damagetype";
    roles[MinimumCasterLevelRole] = "minimumCasterLevel";
    roles[MinimumMagicLevelRole] = "minimumMagicLevel";
    roles[IsRuneRole] = "isRune";
    roles[RuneAmountRole] = "runeAmount";
    roles[IsPremiumRole] = "isPremium";
    roles[PremiumOnlyRole] = "premiumOnly";
    roles[AllowedVocationsRole] = "allowedVocations";
    roles[ManaCostRole] = "manaCost";
    roles[SoulPointsCostRole] = "soulPointsCost";
    roles[CooldownsRole] = "cooldowns";
    roles[CooldownRole] = "cooldown";
    roles[CooldownPrimaryGroupRole] = "cooldownPrimaryGroup";
    roles[CooldownSecondaryGroupRole] = "cooldownSecondaryGroup";
    roles[GroupCooldownsRole] = "groupCooldowns";
    roles[CastPossibleRole] = "castPossible";
    roles[DescriptionRole] = "description";
    roles[ScalingRole] = "scaling";
    roles[SpellUsageInfoRole] = "spellUsageInfo";
    roles[RuneUsageInfoRole] = "runeUsageInfo";
    roles[RelevantUsageInfoRole] = "relevantUsageInfo";
    roles[SourceRole] = "source";
    roles[IsAimAtTargetConfigurableRole] = "isAimAtTargetConfigurable";
    roles[IsCrossHairSpellRole] = "isCrossHairSpell";
    return roles;
}

void TSpellListDataModel::onSpellDataModificationsApplied()
{
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { CooldownRole, ManaCostRole });
}

TSpellListDataModelSortFilterProxy::TSpellListDataModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TSpellListDataModelSortFilterProxy(parent)
    , m_model(new TSpellListDataModel(this))
{
    m_showOnlyCurrentVocation = true;
    m_showDruidSpells = m_showKnightSpells = m_showPaladinSpells = m_showSorcererSpells = m_showMonkSpells = true;
    m_showAttackSpellGroup = m_showHealingSpellGroup = m_showSupportSpellGroup = true;
    m_showRuneSpells = m_showInstantSpells = m_showPremiumOnlySpells = m_showFreeSpells = true;
    setSourceModel(m_model);
    setDynamicSortFilter(true);
    sort(0);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TSpellListDataModelSortFilterProxy::onGameSessionChanged);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameSessionChanged, this,
            &TSpellListDataModelSortFilterProxy::onGameSessionChanged);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this,
            &TSpellListDataModelSortFilterProxy::onPlayerDataChanged);
    onGameSessionChanged();
}

void TSpellListDataModelSortFilterProxy::onGameSessionChanged()
{
    if (game::TPlayerData* player = game::TPlayerData::liveInstance())
        connect(player, &game::TPlayerData::playerDataChanged, this, &TSpellListDataModelSortFilterProxy::onPlayerDataChanged,
                Qt::UniqueConnection);
    onPlayerDataChanged();
}

void TSpellListDataModelSortFilterProxy::onPlayerDataChanged()
{
    const game::TPlayerData* player = tibia::game::isOnline() ? game::TPlayerData::liveInstance() : nullptr;
    if (!player) {
        m_knownSpells.clear();
        m_playerVocation = 0;
        m_playerLevel = 0;
        invalidate();
        return;
    }
    bool changed = false;
    const std::vector<uint16_t> spells = player->spells();
    const std::set<uint16_t> known(spells.begin(), spells.end());
    if (known != m_knownSpells) {
        m_knownSpells = known;
        changed = true;
    }
    if (m_playerVocation != player->vocation()) {
        m_playerVocation = player->vocation();
        changed = true;
    }
    if (m_playerLevel != player->level()) {
        m_playerLevel = player->level();
        changed = true;
    }
    if (changed)
        invalidate();
}

void TSpellListDataModelSortFilterProxy::resetFilter()
{
    m_showOnlyCurrentVocation = true;
    m_showOnlyCurrentLevel = false;
    m_showUnkownSpells = true;
    m_showDruidSpells = m_showKnightSpells = m_showPaladinSpells = m_showSorcererSpells = m_showMonkSpells = true;
    m_showAttackSpellGroup = m_showHealingSpellGroup = m_showSupportSpellGroup = true;
    m_showRuneSpells = m_showInstantSpells = m_showPremiumOnlySpells = m_showFreeSpells = true;
    m_filterText = QString();
    invalidate();
    Q_EMIT configChanged();
}

void TSpellListDataModelSortFilterProxy::setFlag(bool& flag, const bool value)
{
    if (flag == value)
        return;
    flag = value;
    invalidate();
    Q_EMIT configChanged();
}

void TSpellListDataModelSortFilterProxy::setShowOnlyCurrentVocation(const bool v) { setFlag(m_showOnlyCurrentVocation, v); }
void TSpellListDataModelSortFilterProxy::setShowOnlyCurrentLevel(const bool v) { setFlag(m_showOnlyCurrentLevel, v); }
void TSpellListDataModelSortFilterProxy::setShowUnkownSpells(const bool v) { setFlag(m_showUnkownSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowDruidSpells(const bool v) { setFlag(m_showDruidSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowKnightSpells(const bool v) { setFlag(m_showKnightSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowPaladinSpells(const bool v) { setFlag(m_showPaladinSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowSorcererSpells(const bool v) { setFlag(m_showSorcererSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowMonkSpells(const bool v) { setFlag(m_showMonkSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowAttackSpellGroup(const bool v) { setFlag(m_showAttackSpellGroup, v); }
void TSpellListDataModelSortFilterProxy::setShowHealingSpellGroup(const bool v) { setFlag(m_showHealingSpellGroup, v); }
void TSpellListDataModelSortFilterProxy::setShowSupportSpellGroup(const bool v) { setFlag(m_showSupportSpellGroup, v); }
void TSpellListDataModelSortFilterProxy::setShowRuneSpells(const bool v) { setFlag(m_showRuneSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowInstantSpells(const bool v) { setFlag(m_showInstantSpells, v); }
void TSpellListDataModelSortFilterProxy::setShowPremiumOnlySpells(const bool v) { setFlag(m_showPremiumOnlySpells, v); }
void TSpellListDataModelSortFilterProxy::setShowFreeSpells(const bool v) { setFlag(m_showFreeSpells, v); }

void TSpellListDataModelSortFilterProxy::setFilterText(const QString& v)
{
    if (m_filterText == v)
        return;
    m_filterText = v;
    invalidate();
    Q_EMIT configChanged();
}

QVariant TSpellListDataModelSortFilterProxy::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const QModelIndex source = mapToSource(index);
    if (role != TSpellListDataModel::CastPossibleRole)
        return source.model() ? source.model()->data(source, role) : QVariant();
    // A spell can be cast when the player knows it and has its vocation and level.
    const int spellId = source.model() ? source.model()->data(source, TSpellListDataModel::EnumIdRole).toInt() : 0;
    const spells::TSpellInfo spell = spells::TSpellInfoStorage::instance()->spellInfoAt(spellId);
    return m_knownSpells.count(uint16_t(spellId)) > 0 && hasVocation(spell.cast, m_playerVocation)
        && spell.cast.minimumLevel <= m_playerLevel;
}

bool TSpellListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* source = sourceModel();
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    const int spellId = source->data(index, TSpellListDataModel::EnumIdRole).toInt();
    const spells::TSpellInfo spell = spells::TSpellInfoStorage::instance()->spellInfoAt(spellId);
    const spells::TSpellCastParameters& cast = spell.cast;

    bool playerOk = !m_showOnlyCurrentVocation || hasVocation(cast, m_playerVocation);
    if (m_showOnlyCurrentLevel)
        playerOk = playerOk && cast.minimumLevel <= m_playerLevel;
    if (!m_showUnkownSpells)
        playerOk = playerOk && m_knownSpells.count(uint16_t(spellId)) > 0;

    const bool vocationOk = (m_showDruidSpells && hasVocation(cast, 4)) || (m_showKnightSpells && hasVocation(cast, 1))
        || (m_showPaladinSpells && hasVocation(cast, 2)) || (m_showSorcererSpells && hasVocation(cast, 3))
        || (m_showMonkSpells && hasVocation(cast, 5));
    const auto inGroup = [&cast](const uint8_t group) { return cast.groupSecondary == group || cast.groupPrimary == group; };
    const bool groupOk = (m_showAttackSpellGroup && inGroup(1)) || (m_showHealingSpellGroup && inGroup(2))
        || (m_showSupportSpellGroup && inGroup(3));
    const bool premiumOk = (m_showPremiumOnlySpells && spell.premium) || (m_showFreeSpells && !spell.premium);
    const bool kindOk = (m_showInstantSpells && !spell.rune) || (m_showRuneSpells && spell.rune);

    bool accepted = premiumOk && groupOk && kindOk && vocationOk && playerOk;
    // Every search word in the formula, or every search word in the name.
    QStringList words = m_filterText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    words.removeIf([](const QString& word) { return word.trimmed().isEmpty(); });
    if (!words.isEmpty()) {
        const auto containsAll = [&words](const QString& text) {
            return std::all_of(words.cbegin(), words.cend(),
                               [&text](const QString& word) { return text.contains(word, Qt::CaseInsensitive); });
        };
        accepted = accepted && (containsAll(spell.words) || containsAll(spell.name));
    }
    return accepted;
}

bool TSpellListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString a = left.model() ? left.model()->data(left, TSpellListDataModel::NameRole).toString().toLower() : QString();
    const QString b = right.model() ? right.model()->data(right, TSpellListDataModel::NameRole).toString().toLower() : QString();
    return QString::compare(a, b, Qt::CaseSensitive) < 0;
}

QVariant TSpellListDataModelSortFilterProxy::getModelDataForRow(const int RowIndex)
{
    const QModelIndex source = mapToSource(index(RowIndex, 0));
    if (!source.isValid())
        return {};
    QVariantMap row;
    const QHash<int, QByteArray> roles = sourceModel()->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        const QString name = QString::fromUtf8(it.value());
        if (!name.isEmpty())
            row.insert(name, sourceModel()->data(source, it.key()));
    }
    return row;
}

QVariant TSpellListDataModelSortFilterProxy::getDataWithRoleName(const int RowIndex, const QByteArray& RoleName)
{
    const QModelIndex source = mapToSource(index(RowIndex, 0));
    const int role = roleNames().key(RoleName, 0);
    return m_model->data(source, role).toString();
}

int TSpellListDataModelSortFilterProxy::spellIdAt(const int row) const
{
    return m_model->data(mapToSource(index(row, 0)), TSpellListDataModel::EnumIdRole).toInt();
}

int TSpellListDataModelSortFilterProxy::rowOfSpell(const int spellId) const
{
    for (int row = 0; row < rowCount(); ++row) {
        if (spellIdAt(row) == spellId)
            return row;
    }
    return -1;
}

void TAssignActionDialogState::bind(const TActionBarController* bar, const uint buttonId, const int multiActionIndex)
{
    this->multiActionIndex = multiActionIndex;
    this->buttonId = buttonId;
    actionBarId = bar ? uint(bar->actionBarId()) : 0;
    buttonName = bar ? actionButtonName(bar->actionBarId(), int(buttonId)) : QString();
}

TAssignSpellActionDialogController::TAssignSpellActionDialogController(TActionBarController* bar, QObject* parent)
    : TDialogController(QStringLiteral("AssignSpellActionDialog.qml"), parent)
    , m_bar(bar)
{
    m_spellListModel = QVariant::fromValue<QObject*>(&m_proxy);
    m_showOnlyKnownSpells = true;
}

void TAssignSpellActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex)
{
    m_proxy.resetFilter();
    m_proxy.setShowUnkownSpells(!m_showOnlyKnownSpells);
    showDialog(ButtonId, MultiActionIndex, m_proxy.spellIdAt(0), QString::fromUtf8(""), ECastType(0));
}

void TAssignSpellActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex, const int SpellId,
                                                    const QString& Parameters, const ECastType CastType)
{
    dialog()->close();
    m_assign.bind(m_bar, ButtonId, MultiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    m_spellId = SpellId;
    m_spellParameters = Parameters;
    m_currentSpellCastType = CastType;
    // Every box ticked and the search words cleared; unknown spells follow
    // the dialog's "only known spells" box.
    m_proxy.resetFilter();
    m_proxy.setShowUnkownSpells(!m_showOnlyKnownSpells);
    dialog()->create();
    dialog()->show();
}

uint TAssignSpellActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignSpellActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignSpellActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignSpellActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignSpellActionDialogController::spellListIndex() const { return uint(m_proxy.rowOfSpell(m_spellId)); }
QString TAssignSpellActionDialogController::spellParameters() const { return m_spellParameters; }
bool TAssignSpellActionDialogController::showOnlyKnownSpells() const { return m_showOnlyKnownSpells; }
ECastType TAssignSpellActionDialogController::currentSpellCastType() const { return m_currentSpellCastType; }
QVariant TAssignSpellActionDialogController::spellListModel() const { return m_spellListModel; }

void TAssignSpellActionDialogController::setShowOnlyKnownSpells(const bool v)
{
    if (::cip::unchanged(m_showOnlyKnownSpells, v))
        return;
    m_showOnlyKnownSpells = v;
    m_proxy.setShowUnkownSpells(!v);
    Q_EMIT selectedSpellChanged();
    Q_EMIT spellFilterChanged();
}

void TAssignSpellActionDialogController::onOkClicked(const QString& Parameters, const ECastType CastType)
{
    onApplyClicked(Parameters, CastType);
    dialog()->close();
}

void TAssignSpellActionDialogController::onApplyClicked(const QString& Parameters, const ECastType CastType)
{
    m_spellParameters = Parameters;
    m_currentSpellCastType = CastType;
    Q_EMIT selectedSpellChanged();
    Q_EMIT requestAssignSpell(m_assign.buttonId, m_assign.multiActionIndex, m_spellId, Parameters, CastType);
}

void TAssignSpellActionDialogController::spellSelected(const uint SpellEnumId)
{
    if (int(SpellEnumId) == m_spellId)
        return;
    m_spellId = int(SpellEnumId);
    m_spellParameters.clear();
    m_currentSpellCastType = ECastType(0);
    Q_EMIT selectedSpellChanged();
}

void TAssignSpellActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignObjectActionDialogController::TAssignObjectActionDialogController(TActionBarController* bar, QObject* parent)
    : TDialogController(QStringLiteral("AssignObjectActionDialog.qml"), parent)
    , m_bar(bar)
{
    m_useObjectType = uint(config::EUseType::SelectUseTarget);
}

void TAssignObjectActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex)
{
    m_assign.bind(m_bar, ButtonId, MultiActionIndex);
    startTargetSelection();
}

void TAssignObjectActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex, const int ObjectId,
                                                     const int UpgradeTier, const int LiquidType, const config::EUseType UseType,
                                                     const bool SmartModeEnabled, const bool assignWithoutDialog)
{
    dialog()->close();
    m_assign.bind(m_bar, ButtonId, MultiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    setObject(ObjectId, UpgradeTier);
    m_liquidType = Info::ELiquidType(LiquidType);
    setUseObjectType(uint(UseType));
    setSmartModeEnabled(SmartModeEnabled);
    if (!assignWithoutDialog) {
        dialog()->create();
        dialog()->show();
        return;
    }
    emitRequestAssignObject();
}

void TAssignObjectActionDialogController::showDialogForObject(const uint ButtonId, const int MultiActionIndex, const int ObjectId,
                                                              const int UpgradeTier, const bool assignWithoutDialog)
{
    if (!itemType(ObjectId)) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionSetStatusLine>(qtTrId("invalid_object")));
        return;
    }
    showDialog(ButtonId, MultiActionIndex, ObjectId, UpgradeTier, 0, TActionBarManager::defaultUseType(ObjectId), false,
               assignWithoutDialog);
}

void TAssignObjectActionDialogController::setClientSettingSmoothFiltering(const bool enabled)
{
    if (m_clientSettingSmoothFiltering == enabled)
        return;
    m_clientSettingSmoothFiltering = enabled;
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::setUseObjectType(const uint useObjectType)
{
    if (m_useObjectType == useObjectType)
        return;
    m_useObjectType = useObjectType;
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::setSmartModeEnabled(const bool enabled)
{
    if (m_smartModeEnabled == enabled)
        return;
    m_smartModeEnabled = enabled;
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::emitRequestAssignObject()
{
    Q_EMIT requestAssignObject(m_assign.buttonId, m_assign.multiActionIndex, int(m_objectID), int(m_upgradeTier), int(m_liquidType),
                               config::EUseType(m_useObjectType), m_smartModeEnabled);
}

uint TAssignObjectActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignObjectActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignObjectActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignObjectActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignObjectActionDialogController::objectID() const { return m_objectID; }
uint TAssignObjectActionDialogController::objectCount() const { return m_objectCount; }
uint TAssignObjectActionDialogController::upgradeTier() const { return m_upgradeTier; }
Info::ELiquidType TAssignObjectActionDialogController::liquidType() const { return m_liquidType; }
Info::EHookDirection TAssignObjectActionDialogController::hookDirection() const { return m_hookDirection; }
uint TAssignObjectActionDialogController::useObjectType() const { return m_useObjectType; }
bool TAssignObjectActionDialogController::canBeEquiped() const { return m_canBeEquiped; }
bool TAssignObjectActionDialogController::canBeMultiUsed() const { return m_canBeMultiUsed; }
bool TAssignObjectActionDialogController::canHaveSmartMode() const { return m_canHaveSmartMode; }
bool TAssignObjectActionDialogController::smartModeEnabled() const { return m_smartModeEnabled; }
bool TAssignObjectActionDialogController::clientSettingSmoothFiltering() const { return smoothFilteringEnabled(); }

void TAssignObjectActionDialogController::setObject(const int objectId, const int upgradeTier)
{
    const TAppearanceTypePtr type = itemType(objectId);
    m_objectID = uint(objectId);
    m_upgradeTier = uint(upgradeTier);
    m_objectCount = 0;
    m_liquidType = Info::ELiquidType::EMPTY;
    m_hookDirection = Info::EHookDirection::NONE;
    m_canBeMultiUsed = type && type->isMultiUse();
    m_canBeEquiped = isEquipable(type);
    m_canHaveSmartMode = m_canBeEquiped;
    Q_EMIT objectAppearanceChanged();
    Q_EMIT changed();
}

void TAssignObjectActionDialogController::onOkClicked(const uint UseObjectTypeEnumId, const bool UseEquipSmartModeEnabled)
{
    onApplyClicked(UseObjectTypeEnumId, UseEquipSmartModeEnabled);
    dialog()->close();
}

void TAssignObjectActionDialogController::onApplyClicked(const uint UseObjectTypeEnumId, const bool UseEquipSmartModeEnabled)
{
    setUseObjectType(UseObjectTypeEnumId);
    setSmartModeEnabled(UseEquipSmartModeEnabled);
    emitRequestAssignObject();
}

void TAssignObjectActionDialogController::startTargetSelection()
{
    m_selectingTarget = true;
    dialog()->hide();
    if (TMouseCursorController* cursor = TMouseCursorController::instance())
        cursor->setTargetSelection(true);
    QPointer<TAssignObjectActionDialogController> self(this);
    input::startTargetSelection([self](const input::TTargetSelectedPtr& target) {
        if (TMouseCursorController* cursor = TMouseCursorController::instance())
            cursor->setTargetSelection(false);
        if (!self || !self->m_selectingTarget)
            return;
        // A picked object reopens the dialog with it.
        if (target && target->objectClientId()) {
            uint8_t tier = 0;
            if (target->position().x == UINT16_MAX) {
                if (target->position().y & 0x40) {
                    if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
                        if (const container::TStoredContainer* row = storage->containerAt(target->position().y & 0x3F))
                            tier = storage->slotAt(row->id, static_cast<int>(row->firstIndex) + target->stackPosition()).tier;
                    }
                } else if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
                    tier = inventory->slotAt(target->position().y).tier;
                }
            }
            self->showDialogForObject(self->m_assign.buttonId, self->m_assign.multiActionIndex, target->objectClientId(), tier, false);
        }
        // The selection ended: a hidden dialog shows again.
        self->m_selectingTarget = false;
        if (self->dialog()->item() && !self->dialog()->isShown())
            self->dialog()->show();
    });
}

void TAssignObjectActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignTextActionDialogController::TAssignTextActionDialogController(TActionBarController* bar, QObject* parent)
    : TDialogController(QStringLiteral("AssignTextActionDialog.qml"), parent)
    , m_bar(bar)
{
    m_sendAutomatically = true;
}

void TAssignTextActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex)
{
    showDialog(ButtonId, MultiActionIndex, QString::fromUtf8(""), true);
}

void TAssignTextActionDialogController::showDialog(const uint ButtonId, const int MultiActionIndex, const QString& Text,
                                                   const bool SendAutomatically)
{
    dialog()->close();
    m_assign.bind(m_bar, ButtonId, MultiActionIndex);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    m_text = Text;
    m_sendAutomatically = SendAutomatically;
    dialog()->create();
    dialog()->show();
}

uint TAssignTextActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignTextActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignTextActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignTextActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
QString TAssignTextActionDialogController::text() const { return m_text; }
bool TAssignTextActionDialogController::sendAutomatically() const { return m_sendAutomatically; }

void TAssignTextActionDialogController::onOkClicked(const QString& Text, const bool SendAutomatically)
{
    onApplyClicked(Text, SendAutomatically);
    dialog()->close();
}

void TAssignTextActionDialogController::onApplyClicked(const QString& Text, const bool SendAutomatically)
{
    m_text = Text;
    m_sendAutomatically = SendAutomatically;
    Q_EMIT changed();
    Q_EMIT requestAssignText(m_assign.buttonId, m_assign.multiActionIndex, Text, SendAutomatically);
}

void TAssignTextActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TAssignPassiveAbilityActionDialogController::TAssignPassiveAbilityActionDialogController(TActionBarController* bar, QObject* parent)
    : TDialogController(QStringLiteral("AssignPassiveAbilityActionDialog.qml"), parent)
    , m_bar(bar)
{
    m_passiveAbilityListIndex = uint(-1);
    m_model = new TPassiveAbilityListDataModel(this);
    m_proxy = new TPassiveAbilityListDataModelSortFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_passiveAbilityListModel = QVariant::fromValue(static_cast<QObject*>(m_proxy));
}

void TAssignPassiveAbilityActionDialogController::reloadAbilities()
{
    game::TPassiveAbilityInfoStorage* storage = game::TPassiveAbilityInfoStorage::instance();
    gamewindow::TPassiveAbilityListDataModel* shared = storage ? storage->listModel() : nullptr;
    if (!shared)
        return;
    std::vector<TPassiveAbilityListDataModel::Entry> copy;
    for (int i = 0; i < shared->rowCount(); ++i) {
        TPassiveAbilityListDataModel::Entry entry;
        entry.id = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::IdRole).toInt();
        entry.name = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::NameRole).toString();
        entry.iconUrl = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::IconUrlRole).toString();
        entry.usableByCurrentVocation = shared->data(shared->index(i, 0), TPassiveAbilityListDataModel::UsableRole).toBool();
        copy.push_back(std::move(entry));
    }
    m_model->setEntries(std::move(copy));
}

void TAssignPassiveAbilityActionDialogController::showDialog(const uint ButtonId)
{
    reloadAbilities();
    const int first = m_proxy->rowCount() > 0 ? m_proxy->index(0, 0).data(TPassiveAbilityListDataModel::IdRole).toInt() : 0;
    showDialog(ButtonId, first);
}

void TAssignPassiveAbilityActionDialogController::showDialog(const uint ButtonId, const int PassiveAbility)
{
    dialog()->close();
    reloadAbilities();
    m_assign.bind(m_bar, ButtonId, -1);
    m_actionBarId = m_assign.actionBarId;
    m_buttonId = m_assign.buttonId;
    m_buttonName = m_assign.buttonName;
    m_showActionBarTarget = m_assign.showActionBarTarget;
    m_passiveAbility = PassiveAbility;
    refreshSelectedPassive();
    dialog()->create();
    dialog()->show();
}

uint TAssignPassiveAbilityActionDialogController::actionBarId() const { return m_assign.actionBarId; }
uint TAssignPassiveAbilityActionDialogController::buttonId() const { return m_assign.buttonId; }
QString TAssignPassiveAbilityActionDialogController::buttonName() const { return m_assign.buttonName; }
bool TAssignPassiveAbilityActionDialogController::showActionBarTarget() const { return m_assign.showActionBarTarget; }
uint TAssignPassiveAbilityActionDialogController::passiveAbilityListIndex() const { return m_passiveAbilityListIndex; }
QString TAssignPassiveAbilityActionDialogController::passiveAbilityName() const { return m_passiveAbilityName; }
QString TAssignPassiveAbilityActionDialogController::passiveAbilityIconUrl() const { return m_passiveAbilityIconUrl; }
QVariant TAssignPassiveAbilityActionDialogController::passiveAbilityListModel() const { return m_passiveAbilityListModel; }

void TAssignPassiveAbilityActionDialogController::refreshSelectedPassive()
{
    m_passiveAbilityListIndex = uint(-1);
    m_passiveAbilityName.clear();
    m_passiveAbilityIconUrl.clear();
    if (!m_proxy || !m_passiveAbility)
        return;
    // The row of the shown (sorted, filtered) list.
    for (int i = 0; i < m_proxy->rowCount(); ++i) {
        const QModelIndex idx = m_proxy->index(i, 0);
        if (idx.data(TPassiveAbilityListDataModel::IdRole).toInt() != m_passiveAbility)
            continue;
        m_passiveAbilityListIndex = uint(i);
        m_passiveAbilityName = idx.data(TPassiveAbilityListDataModel::NameRole).toString();
        m_passiveAbilityIconUrl = idx.data(TPassiveAbilityListDataModel::IconUrlRole).toString();
        if (m_passiveAbilityIconUrl.isEmpty())
            m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(m_passiveAbility);
        break;
    }
    if (m_passiveAbilityIconUrl.isEmpty() && m_passiveAbility)
        m_passiveAbilityIconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(m_passiveAbility);
}

void TAssignPassiveAbilityActionDialogController::onOkClicked()
{
    onApplyClicked();
    dialog()->close();
}

void TAssignPassiveAbilityActionDialogController::onApplyClicked()
{
    Q_EMIT requestAssignPassiveAbility(m_assign.buttonId, m_passiveAbility);
}

void TAssignPassiveAbilityActionDialogController::passiveAbilitySelected(const uint PassiveAbilityEnumId)
{
    if (int(PassiveAbilityEnumId) == m_passiveAbility)
        return;
    m_passiveAbility = int(PassiveAbilityEnumId);
    refreshSelectedPassive();
    Q_EMIT selectedPassiveAbilityChanged();
}

void TAssignPassiveAbilityActionDialogController::onCloseClicked()
{
    dialog()->close();
}

TRecordKeyboardShortcutDialogController::TRecordKeyboardShortcutDialogController(QObject* parent)
    : TDialogController(QStringLiteral("RecordKeyboardShortcutDialog.qml"), parent)
{
    initKeyLists();
}

TRecordKeyboardShortcutDialogController::TRecordKeyboardShortcutDialogController(const QString& description, const QString& actionName,
                                                                                 const bool chatModeOn, const QKeySequence& current,
                                                                                 QObject* parent)
    : TDialogController(QStringLiteral("RecordKeyboardShortcutDialog.qml"), parent)
{
    initKeyLists();
    connect(this, &cip::tibia::gamewindow::TRecordKeyboardShortcutDialogController::dialogClosed, this, &QObject::deleteLater);
    showDialog(chatModeOn, false, current, description, occupiedKeys(actionName, chatModeOn));
}

/// The walk keys and every modifier combination without a key.
void TRecordKeyboardShortcutDialogController::initKeyLists()
{
    for (const char* key : { "Left", "Right", "Up", "Down", "Num+Left", "Num+Right", "Num+Up", "Num+Down", "Num+PgUp", "Num+PgDown",
                             "Num+Home", "Num+End" })
        m_hardcodedKeys.append(QKeySequence(QString::fromLatin1(key)));
    for (const int modifiers : { 0x10000000, 0x2000000, 0x12000000, 0x8000000, 0x18000000, 0xa000000, 0x1a000000, 0x4000000,
                                 0x14000000, 0x6000000, 0x16000000, 0xc000000, 0x1c000000, 0xe000000, 0x1e000000 })
        m_modifierOnlyKeys.append(QKeySequence(QKeyCombination::fromCombined(modifiers)));
}

/// The keys bound to other actions in the chat mode.
std::set<QKeySequence> TRecordKeyboardShortcutDialogController::occupiedKeys(const QString& ownAction, const bool chatModeOn)
{
    using Setting = config::TActionSetting;
    std::set<QKeySequence> keys;
    const config::THotkeySet& set = config::THotkeyOptions::instance()->currentSet();
    for (const config::THotkeyBinding& binding : chatModeOn ? set.chatOn : set.chatOff) {
        if (binding.keys.isEmpty())
            continue;
        if (binding.setting.kind == Setting::ACTION && binding.setting.action == ownAction)
            continue;
        keys.insert(binding.keys);
    }
    return keys;
}

TRecordKeyboardShortcutDialogController::~TRecordKeyboardShortcutDialogController() { stopRecording(); }

QString TRecordKeyboardShortcutDialogController::keySequence() const
{
    return m_keys.toString(QKeySequence::NativeText).replace(QStringLiteral("Backtab"), QStringLiteral("Tab"));
}

bool TRecordKeyboardShortcutDialogController::keySequenceOccupied() const { return m_occupiedKeys.count(m_keys) != 0; }

bool TRecordKeyboardShortcutDialogController::keySequenceHardcoded() const { return m_hardcodedKeys.contains(m_keys); }

QString TRecordKeyboardShortcutDialogController::actionDescription() const { return m_description; }

bool TRecordKeyboardShortcutDialogController::chatModeOn() const { return m_isChatModeOn; }

bool TRecordKeyboardShortcutDialogController::onlySingleKey() const { return m_isOnlySingleKey; }

void TRecordKeyboardShortcutDialogController::showDialog(const bool ChatModeOn, const bool OnlySingleKey, const QKeySequence& CurrentKeySequence,
                                                         const QString& ActionDescription, const std::set<QKeySequence>& OccupiedKeys)
{
    m_isChatModeOn = ChatModeOn;
    m_isOnlySingleKey = OnlySingleKey;
    m_keys = CurrentKeySequence;
    m_selectedKeys = CurrentKeySequence;
    m_description = ActionDescription;
    m_occupiedKeys = OccupiedKeys;
    dialog()->create();
    startRecording();
    dialog()->show();
}

void TRecordKeyboardShortcutDialogController::startRecording()
{
    input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance();
    if (m_recording || !eventFilters)
        return;
    m_recording = true;
    m_recordConnection = connect(eventFilters, &input::TEventFilterManager::keyboardShortcutRecorded, this,
                                 &TRecordKeyboardShortcutDialogController::onKeySequenceEntered);
    eventFilters->startRecordingKeyboardShortcut();
}

void TRecordKeyboardShortcutDialogController::stopRecording()
{
    if (!m_recording)
        return;
    m_recording = false;
    disconnect(m_recordConnection);
    if (input::TEventFilterManager* eventFilters = input::TEventFilterManager::instance())
        eventFilters->stopRecordingKeyboardShortcut();
}

/// A single-key dialog keeps the last key of a chord (keypad modifier kept,
/// others dropped); modifiers alone clear the key.
void TRecordKeyboardShortcutDialogController::onKeySequenceEntered(const QKeySequence& NewKeySequence)
{
    QKeySequence keys = NewKeySequence;
    if (!keys.isEmpty() && m_isOnlySingleKey)
        keys = QKeySequence(QKeyCombination::fromCombined(keys[keys.count() - 1].toCombined() & 0x21ffffff));
    m_keys = m_modifierOnlyKeys.contains(keys) ? QKeySequence() : keys;
    Q_EMIT keySequenceChanged(m_keys);
}

void TRecordKeyboardShortcutDialogController::onOkButtonClicked()
{
    Q_EMIT keySequenceSelected(m_keys);
    stopRecording();
    dialog()->close();
}

void TRecordKeyboardShortcutDialogController::onClearButtonClicked()
{
    Q_EMIT keySequenceSelected(QKeySequence());
    stopRecording();
    dialog()->close();
}

void TRecordKeyboardShortcutDialogController::onCancelButtonClicked()
{
    stopRecording();
    dialog()->close();
}

void TRecordKeyboardShortcutDialogController::requestClose()
{
    stopRecording();
    dialog()->close();
}

}
