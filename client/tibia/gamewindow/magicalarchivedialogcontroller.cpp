#include "magicalarchivedialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QFile>
#include <QJsonDocument>
#include <QJSEngine>
#include <QLocale>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

constexpr int CipKnight = 0;
constexpr int CipPaladin = 2;
constexpr int CipSorcerer = 3;
constexpr int CipDruid = 4;
constexpr int CipMonk = 5;

int cipVocationBase(const int vocation)
{
    switch (vocation) {
        case 1:
        case 5: return CipSorcerer;
        case 2:
        case 6: return CipDruid;
        case 3:
        case 7: return CipPaladin;
        case 4:
        case 8: return CipKnight;
        case 9:
        case 10: return CipMonk;
        default: return -1;
    }
}

int cipVocationOfName(const QString& name)
{
    if (name.contains(QLatin1String("Knight"), Qt::CaseInsensitive))
        return CipKnight;
    if (name.contains(QLatin1String("Paladin"), Qt::CaseInsensitive))
        return CipPaladin;
    if (name.contains(QLatin1String("Sorcerer"), Qt::CaseInsensitive))
        return CipSorcerer;
    if (name.contains(QLatin1String("Druid"), Qt::CaseInsensitive))
        return CipDruid;
    if (name.contains(QLatin1String("Monk"), Qt::CaseInsensitive))
        return CipMonk;
    return -1;
}

QString groupLabel(const int group)
{
    switch (group) {
        case 1: return qtTrId("spell_attack");
        case 2: return qtTrId("spell_healing");
        case 3: return qtTrId("spell_support");
        default: return {};
    }
}

QString cooldownText(const int seconds)
{
    if (seconds <= 0)
        return QStringLiteral("-");
    return QLocale(QLocale::English).toString(seconds) + QLatin1Char('s');
}

QString damageLabel(const QString& damagetype)
{
    if (damagetype.isEmpty() || damagetype == QLatin1String("None"))
        return qtTrId("spelllist_label_damage_type_none");
    if (damagetype.contains(QLatin1String("VARIABLE"), Qt::CaseInsensitive))
        return qtTrId("spelllist_label_damage_type_variable");
    QString name = damagetype;
    name.remove(QLatin1String("DAMAGE_"));
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return name.toLower();
}

QVariantList vocationInfos(const QStringList& vocations)
{
    QVariantList list;
    for (const QString& name : vocations) {
        const int id = cipVocationOfName(name);
        if (id < 0)
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("iconUrl"), QStringLiteral("image://vocation-icons/%1").arg(id));
        entry.insert(QStringLiteral("tooltip"), name);
        list.push_back(entry);
    }
    return list;
}

QVariantMap usageInfo(const spells::TSpellInfo& spell, const bool runeUse)
{
    QVariantMap info;
    info.insert(QStringLiteral("manaCost"), spell.mana);
    info.insert(QStringLiteral("soulPointsCost"), spell.soul);
    const QString primary = groupLabel(spell.groupPrimary);
    const QString secondary = groupLabel(spell.groupSecondary);
    info.insert(QStringLiteral("groups"), secondary.isEmpty() ? primary : primary + QLatin1String(" / ") + secondary);
    info.insert(QStringLiteral("mean"), spell.mean);
    info.insert(QStringLiteral("cooldown"), cooldownText(spell.cooldownSelf));
    QString groups = cooldownText(spell.cooldownPrimaryGroup);
    if (spell.cooldownSecondaryGroup > 0)
        groups += QLatin1String(" / ") + cooldownText(spell.cooldownSecondaryGroup);
    info.insert(QStringLiteral("groupCooldowns"), groups);
    info.insert(QStringLiteral("damagetype"), damageLabel(spell.damagetype));
    info.insert(QStringLiteral("allowedVocationInfos"), vocationInfos(runeUse && !spell.runeVocations.isEmpty() ? spell.runeVocations : spell.vocations));
    return info;
}

bool spellHasVocation(const spells::TSpellInfo& spell, const int cipVoc)
{
    for (const QString& name : spell.vocations) {
        if (cipVocationOfName(name) == cipVoc)
            return true;
    }
    return false;
}

}

TMagicalArchiveSpellListModel::TMagicalArchiveSpellListModel(QObject* parent)
    : cip::tibia::gamewindow::TSpellListDataModel(parent)
{
    rebuildRows();
}

void TMagicalArchiveSpellListModel::rebuildRows()
{
    m_rows.clear();
    for (const auto& [id, spell] : spells::TSpellInfoStorage::instance()->spells()) {
        if (!spell.name.isEmpty())
            m_rows.push_back(id);
    }
    std::sort(m_rows.begin(), m_rows.end(), [](const int a, const int b) {
        const auto* storage = spells::TSpellInfoStorage::instance();
        return QString::localeAwareCompare(storage->spell(a)->name, storage->spell(b)->name) < 0;
    });
}

void TMagicalArchiveSpellListModel::reload()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
}

int TMagicalArchiveSpellListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant TMagicalArchiveSpellListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(m_rows[size_t(index.row())]);
    if (!spell)
        return {};
    switch (role) {
        case EnumIdRole: return spell->id;
        case Qt::DisplayRole:
        case NameRole: return spell->name;
        case FormulaRawRole: return spell->words;
        case ParameterHintsRole: return spell->parameterHints;
        case IconUrlRole: return spells::TSpellInfoStorage::iconUrl(*spell);
        case IconUrlSmallRole: return QStringLiteral("image://spell-icons-20x20/%1").arg(spell->iconIndex);
        case IconIndexRole: return spell->iconIndex;
        case GroupPrimaryRole: return spell->groupPrimary;
        case GroupPrimaryIconUrlRole: return QStringLiteral("image://spellgroup-icons-20x20/%1").arg(std::max(0, spell->groupPrimary - 1));
        case GroupSecondaryRole: return spell->groupSecondary;
        case DamagetypeRole: return damageLabel(spell->damagetype);
        case MinimumCasterLevelRole: return spell->minimumLevel;
        case DescriptionRole: return spell->description;
        case SourceStringRole: return spell->source;
        case MinimumMagicLevelRole: return spell->minimumMagicLevel;
        case MeanRole: return spell->isRune ? 0 : spell->mean;
        case IsRuneRole: return spell->isRune;
        case RuneAmountRole: return spell->runeAmount;
        case IsPremiumRole:
        case PremiumOnlyRole: return spell->premium;
        case GroupsRole: {
            const QString primary = groupLabel(spell->groupPrimary);
            const QString secondary = groupLabel(spell->groupSecondary);
            return secondary.isEmpty() ? primary : primary + QLatin1String(" / ") + secondary;
        }
        case ManaCostRole: return spell->mana;
        case SoulPointsCostRole: return spell->soul;
        case CooldownSelfRole: return cooldownText(spell->cooldownSelf);
        case CooldownPrimaryGroupRole: return cooldownText(spell->cooldownPrimaryGroup);
        case CooldownSecondaryGroupRole: return cooldownText(spell->cooldownSecondaryGroup);
        case GroupCooldownsRole: {
            QString text = cooldownText(spell->cooldownPrimaryGroup);
            if (spell->cooldownSecondaryGroup > 0)
                text += QLatin1String(" / ") + cooldownText(spell->cooldownSecondaryGroup);
            return text;
        }
        case CastPossibleRole: {
            if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
                for (const uint16_t id : player->getSpells()) {
                    if (id == spell->id)
                        return true;
                }
                return false;
            }
            return true;
        }
        case ScalingRole: return QVariant::fromValue(spell->scaling);
        case SpellUsageInfoRole: return usageInfo(*spell, spell->isRune);
        case RuneUsageInfoRole: return spell->isRune ? usageInfo(*spell, true) : QVariantMap{};
        case RelevantUsageInfoRole: return usageInfo(*spell, false);
        case SourceRole: return spell->source;
        case IsAimAtTargetConfigurableRole: return spell->crossHairTarget;
        case IsCrossHairSpellRole: return spell->crossHairTarget;
        case AllowedVocationsRole: return spell->vocations;
        default: return {};
    }
}

QHash<int, QByteArray> TMagicalArchiveSpellListModel::roleNames() const
{
    return {
        { EnumIdRole, "enumId" },
        { NameRole, "name" },
        { FormulaRawRole, "formulaRaw" },
        { ParameterHintsRole, "parameterHints" },
        { IconUrlRole, "iconUrl" },
        { IconUrlSmallRole, "iconUrlSmall" },
        { IconIndexRole, "iconIndex" },
        { GroupPrimaryRole, "groupPrimary" },
        { GroupPrimaryIconUrlRole, "groupPrimaryIconUrl" },
        { GroupSecondaryRole, "groupSecondary" },
        { DamagetypeRole, "damagetype" },
        { MinimumCasterLevelRole, "minimumCasterLevel" },
        { DescriptionRole, "description" },
        { MinimumMagicLevelRole, "minimumMagicLevel" },
        { MeanRole, "mean" },
        { IsRuneRole, "isRune" },
        { RuneAmountRole, "runeAmount" },
        { IsPremiumRole, "isPremium" },
        { PremiumOnlyRole, "premiumOnly" },
        { GroupsRole, "groups" },
        { ManaCostRole, "manaCost" },
        { SoulPointsCostRole, "soulPointsCost" },
        { CooldownSelfRole, "cooldowns" },
        { CooldownPrimaryGroupRole, "cooldownPrimaryGroup" },
        { CooldownSecondaryGroupRole, "cooldownSecondaryGroup" },
        { GroupCooldownsRole, "groupCooldowns" },
        { CastPossibleRole, "castPossible" },
        { ScalingRole, "scaling" },
        { SpellUsageInfoRole, "spellUsageInfo" },
        { RuneUsageInfoRole, "runeUsageInfo" },
        { RelevantUsageInfoRole, "relevantUsageInfo" },
        { SourceRole, "source" },
        { IsAimAtTargetConfigurableRole, "isAimAtTargetConfigurable" },
        { IsCrossHairSpellRole, "isCrossHairSpell" },
        { AllowedVocationsRole, "allowedVocations" },
    };
}

TSpellListDataModelSortFilterProxy::TSpellListDataModelSortFilterProxy(QObject* parent)
    : cip::tibia::gamewindow::TSpellListDataModelSortFilterProxy(parent)
{
    setDynamicSortFilter(true);
    connect(this, &TSpellListDataModelSortFilterProxy::configChanged, this, [this] {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    });
    resetFilter();
}

void TSpellListDataModelSortFilterProxy::resetFilter()
{
    // CIP 0x100c98d90 defaults. Prefer showing the full vocation set until the
    // LocalPlayer vocation is confirmed — otherwise the list can render empty.
    setShowOnlyCurrentVocation(false);
    setShowOnlyCurrentLevel(false);
    setShowUnkownSpells(true);
    setShowDruidSpells(true);
    setShowKnightSpells(true);
    setShowPaladinSpells(true);
    setShowSorcererSpells(true);
    setShowMonkSpells(true);
    setShowAttackSpellGroup(true);
    setShowHealingSpellGroup(true);
    setShowSupportSpellGroup(true);
    setShowRuneSpells(true);
    setShowInstantSpells(true);
    setShowPremiumOnlySpells(true);
    setShowFreeSpells(true);
    setFilterText({});
    syncPlayer();
}

void TSpellListDataModelSortFilterProxy::syncPlayer()
{
    m_playerLevel = 0;
    m_playerVocationBase = -1;
    m_knownSpells.clear();
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
        m_playerLevel = player->getLevel();
        m_playerVocationBase = cipVocationBase(player->getVocation());
        for (const uint16_t id : player->getSpells())
            m_knownSpells.insert(id);
    }
}

void TSpellListDataModelSortFilterProxy::onPlayerDataChanged()
{
    beginFilterChange();
    syncPlayer();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

QVariant TSpellListDataModelSortFilterProxy::getModelDataForRow(const int RowIndex)
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return {};
    const QModelIndex index = this->index(RowIndex, 0);
    if (!index.isValid())
        return {};
    const QModelIndex src = mapToSource(index);
    QVariantMap row;
    const auto roles = source->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row.insert(QString::fromUtf8(it.value()), source->data(src, it.key()));
    return row;
}

QVariant TSpellListDataModelSortFilterProxy::getDataWithRoleName(const int RowIndex, const QByteArray& RoleName)
{
    const QModelIndex idx = index(RowIndex, 0);
    if (!idx.isValid())
        return {};
    const int role = roleNames().key(RoleName, -1);
    if (role < 0)
        return {};
    return data(idx, role);
}

bool TSpellListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* source = sourceModel();
    if (!source)
        return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    const int spellId = source->data(index, TMagicalArchiveSpellListModel::EnumIdRole).toInt();
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
    if (!spell)
        return false;

    bool vocationOk = true;
    // When vocation is unknown (no LocalPlayer / unmapped id), keep rows visible —
    // otherwise showOnlyCurrentVocation would reject every spell.
    if (m_showOnlyCurrentVocation && m_playerVocationBase >= 0)
        vocationOk = spellHasVocation(*spell, m_playerVocationBase);
    if (m_showOnlyCurrentLevel)
        vocationOk = vocationOk && spell->minimumLevel <= m_playerLevel;
    if (!m_showUnkownSpells)
        vocationOk = vocationOk && m_knownSpells.count(spellId) > 0;

    const bool vocMatch = spell->vocations.isEmpty()
        || (m_showDruidSpells && spellHasVocation(*spell, CipDruid))
        || (m_showKnightSpells && spellHasVocation(*spell, CipKnight))
        || (m_showPaladinSpells && spellHasVocation(*spell, CipPaladin))
        || (m_showSorcererSpells && spellHasVocation(*spell, CipSorcerer))
        || (m_showMonkSpells && spellHasVocation(*spell, CipMonk));

    const bool groupMatch = (m_showAttackSpellGroup && spell->groupPrimary == 1)
        || (m_showHealingSpellGroup && spell->groupPrimary == 2)
        || (m_showSupportSpellGroup && spell->groupPrimary == 3)
        || (spell->groupPrimary == 0 && (m_showAttackSpellGroup || m_showHealingSpellGroup || m_showSupportSpellGroup));

    const bool kindMatch = (m_showRuneSpells && spell->isRune) || (m_showInstantSpells && !spell->isRune);
    const bool premiumMatch = (m_showPremiumOnlySpells && spell->premium) || (m_showFreeSpells && !spell->premium)
        || (m_showPremiumOnlySpells && m_showFreeSpells);

    if (!(vocationOk && vocMatch && groupMatch && kindMatch && premiumMatch))
        return false;

    const QString filter = m_filterText.trimmed();
    if (filter.isEmpty())
        return true;
    for (const QString& token : filter.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (!spell->name.contains(token, Qt::CaseInsensitive) && !spell->words.contains(token, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

bool TSpellListDataModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString a = sourceModel()->data(left, TMagicalArchiveSpellListModel::NameRole).toString();
    const QString b = sourceModel()->data(right, TMagicalArchiveSpellListModel::NameRole).toString();
    return QString::localeAwareCompare(a, b) < 0;
}

TMagicalArchiveDialogController::TMagicalArchiveDialogController(QObject* parent)
    : cip::tibia::gamewindow::TMagicalArchiveDialogController(parent)
    , m_model(new TMagicalArchiveSpellListModel(this))
    , m_proxy(new TSpellListDataModelSortFilterProxy(this))
{
    m_proxy->setSourceModel(m_model);
    m_proxy->sort(0);
    m_spellListModel = m_proxy;

    QFile file(QStringLiteral(":/spells/spells-previews.json"));
    if (file.open(QIODevice::ReadOnly))
        m_previewsVariant = QJsonDocument::fromJson(file.readAll()).toVariant();
}

QJSValue TMagicalArchiveDialogController::spellsPreviewsJson() const
{
    if (QJSEngine* engine = qjsEngine(const_cast<TMagicalArchiveDialogController*>(this))) {
        if (m_previewsJs.isUndefined() || m_previewsJs.isNull())
            m_previewsJs = engine->toScriptValue(m_previewsVariant);
        return m_previewsJs;
    }
    return {};
}

void TMagicalArchiveDialogController::setSelectedSpellID(const int v)
{
    if (m_selectedSpellID == v)
        return;
    m_selectedSpellID = v;
    refreshSelectionFlags();
    Q_EMIT selectedSpellIDChanged();
}

void TMagicalArchiveDialogController::refreshSelectionFlags()
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(m_selectedSpellID);
    m_isValidSpellSelected = spell != nullptr;
    m_isAimAtTargetSpellSelected = spell && spell->crossHairTarget;
    m_isAimAtTargetSpellSelectedAndActivated = false;
}

void TMagicalArchiveDialogController::showDialogTab()
{
    m_model->reload();
    m_proxy->resetFilter();
    m_proxy->onPlayerDataChanged();
    // Re-convert previews now that this controller is in a QML tree (CONSTANT
    // property was often empty on the first read before qjsEngine was set).
    m_previewsJs = QJSValue();
    if (QJSEngine* engine = qjsEngine(this))
        m_previewsJs = engine->toScriptValue(m_previewsVariant);
    if (m_initialSpellID == 0 && m_proxy->rowCount() > 0) {
        const QVariant row = m_proxy->getModelDataForRow(0);
        m_initialSpellID = row.toMap().value(QStringLiteral("enumId")).toInt();
    }
    setSelectedSpellID(m_initialSpellID);
    Q_EMIT dataChanged();
    Q_EMIT selectedSpellIDChanged();
}

void TMagicalArchiveDialogController::hideDialogTab() {}

void TMagicalArchiveDialogController::requestAssignSpellToActionButton()
{
    if (m_selectedSpellID <= 0)
        return;
    auto action = input::mutableClone(*input::makeAction(A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON));
    action->parameter = m_selectedSpellID;
    action->actionBarId = 1;
    action->actionButtonId = 0;
    input::publish(action);
}

void TMagicalArchiveDialogController::requestAimAtTargetOptionChange(const bool Value)
{
    if (m_selectedSpellID <= 0)
        return;
    g_game.sendSelectSpellAim({ static_cast<uint16_t>(m_selectedSpellID) }, Value);
    m_isAimAtTargetSpellSelectedAndActivated = Value;
    Q_EMIT selectedSpellIDChanged();
}

}
