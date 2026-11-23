#include "skillwheelmodels.h"

#include <numeric>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <tibia/game/thingsession_forward.h>

namespace tibia::skillwheel {

namespace {

/// A row as a map from role name to data: empty for a negative row or column or an index without
/// a model.
QVariantMap modelRowData(const QAbstractItemModel& model, const QModelIndex& index)
{
    QVariantMap row;
    if (index.row() < 0 || index.column() < 0 || !index.model())
        return row;
    const QHash<int, QByteArray> roles = model.roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row[QString::fromUtf8(it.value())] = model.data(index, it.key());
    return row;
}

bool socketedIn(const TWheel& wheel, const int gemId) { return vesselOfGem(wheel, gemId).has_value(); }

QString itemName(const int typeId)
{
    if (typeId <= 0)
        return {};
    const TAppearanceTypePtr& type = tibia::game::thingType(static_cast<uint16_t>(typeId), ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// A supreme mod's effect; augmentations and revelation masteries show their name above it.
QString supremeModText(const int mod, const int grade)
{
    const QString effect = supremeModEffect(mod, grade);
    if (showsModName(mod))
        return supremeModName(mod, false, false) + u'\n' + effect;
    return effect;
}

int gradeOf(const TGrades& grades, const int mod)
{
    const auto it = grades.find(mod);
    return it != grades.end() ? it->second : 0;
}

/// The revealed gem object type of `quality` for the vocation (1 knight .. 5 monk); 0 for none.
TAppearanceTypePtr gemObjectType(const int quality, const int vocation)
{
    for (const auto& type : tibia::appearances::TAppearancesManager::instance().getSkillWheelGemObjectTypes()) {
        if (type && static_cast<int>(type->getSkillWheelGemQualityId()) == quality
            && static_cast<int>(type->getSkillWheelGemVocationId()) == vocation)
            return type;
    }
    return nullptr;
}

}

TPaginationProxyModel::TPaginationProxyModel(const int entriesPerPage, QObject* parent)
    : shared::TPaginationProxyModel(quint8(entriesPerPage))
{
    setParent(parent);
}

QModelIndex TPaginationProxyModel::showSourceRow(const int sourceRow)
{
    if (sourceRow < 0)
        return {};
    setCurrentPage(pageOfRow(sourceRow));
    return mapFromSource(sourceModel()->index(sourceRow, 0));
}

TInventoryGemListModel::TInventoryGemListModel(QObject* parent)
    : cip::tibia::skillwheel::TInventoryGemListModel(parent)
{}

int TInventoryGemListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_gems.size()); }

QHash<int, QByteArray> TInventoryGemListModel::roleNames() const
{
    return {
        { GemImageSource, "gemImageSource" },     { GemName, "gemName" },
        { Quality, "quality" },                   { Mod1Id, "mod1Id" },
        { Mod1ImageSource, "mod1ImageSource" },   { Mod1EffectString, "mod1EffectString" },
        { Mod1Grade, "mod1Grade" },               { Mod2Id, "mod2Id" },
        { Mod2ImageSource, "mod2ImageSource" },   { Mod2EffectString, "mod2EffectString" },
        { Mod2Grade, "mod2Grade" },               { Mod3Id, "mod3Id" },
        { Mod3ImageSource, "mod3ImageSource" },   { Mod3EffectString, "mod3EffectString" },
        { Mod3Grade, "mod3Grade" },               { IsSocketed, "isSocketed" },
        { GemId, "gemId" },                       { CanBeDismantled, "canBeDismantled" },
        { IsLocked, "isLocked" },
    };
}

void TInventoryGemListModel::setGems(const std::vector<TGem>& gems)
{
    beginResetModel();
    m_gems = gems;
    endResetModel();
}

void TInventoryGemListModel::setConfiguration(const std::optional<TWheel>& configuration, const std::optional<TWheel>& serverConfiguration,
                                              const TGrades& basicGrades, const TGrades& supremeGrades)
{
    m_configuration = configuration;
    m_serverConfiguration = serverConfiguration;
    m_basicGrades = basicGrades;
    m_supremeGrades = supremeGrades;
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

int TInventoryGemListModel::vocationIndex() const { return m_configuration ? m_configuration->vocation - 1 : 0; }
int TInventoryGemListModel::basicGrade(const int mod) const { return gradeOf(m_basicGrades, mod); }
int TInventoryGemListModel::supremeGrade(const int mod) const { return gradeOf(m_supremeGrades, mod); }

QVariant TInventoryGemListModel::data(const QModelIndex& index, const int role) const
{
    // Nothing is shown without a configuration.
    const TGem* g = gem(index.row());
    if (!index.isValid() || !g || !m_configuration || role < GemImageSource || role > IsLocked)
        return {};
    const bool secondMod = g->quality >= 1;
    const bool supremeMod = g->quality >= 2;
    const int quality = secondMod ? (supremeMod ? 2 : 1) : 0;
    const int voc = vocationIndex();
    switch (role) {
        case GemImageSource: {
            const TAppearanceTypePtr type = gemObjectType(quality, m_configuration->vocation);
            return static_cast<uint>(type ? type->getId() : 0);
        }
        case GemName: {
            const TAppearanceTypePtr type = gemObjectType(quality, m_configuration->vocation);
            return type ? QString::fromStdString(type->getName()) : QString();
        }
        case Quality: return quality;
        case Mod1Id: return g->basic1;
        case Mod1ImageSource: return basicModImage(g->basic1);
        case Mod1EffectString: return basicModEffect(g->basic1, voc, basicGrade(g->basic1));
        case Mod1Grade: return basicGrade(g->basic1);
        case Mod2Id: return secondMod ? g->basic2 : 0;
        case Mod2ImageSource: return secondMod ? basicModImage(g->basic2) : QUrl();
        case Mod2EffectString: return secondMod ? basicModEffect(g->basic2, voc, basicGrade(g->basic2)) : QString();
        case Mod2Grade: return secondMod ? basicGrade(g->basic2) : 0;
        case Mod3Id: return supremeMod ? g->supreme : 0;
        case Mod3ImageSource: return supremeMod ? supremeModImage(g->supreme) : QUrl();
        case Mod3EffectString: return supremeMod ? supremeModText(g->supreme, supremeGrade(g->supreme)) : QString();
        case Mod3Grade: return supremeMod ? supremeGrade(g->supreme) : 0;
        case IsSocketed: return socketedIn(*m_configuration, g->id);
        case GemId: return g->id;
        case CanBeDismantled: return !(m_serverConfiguration && socketedIn(*m_serverConfiguration, g->id));
        case IsLocked: return g->locked;
        default: return {};
    }
}

const TGem* TInventoryGemListModel::gem(const int row) const
{
    return row >= 0 && row < static_cast<int>(m_gems.size()) ? &m_gems[static_cast<size_t>(row)] : nullptr;
}

int TInventoryGemListModel::rowOfGem(const int gemId) const
{
    const auto it = std::ranges::find_if(m_gems, [gemId](const TGem& g) { return g.id == gemId; });
    return it != m_gems.end() ? static_cast<int>(it - m_gems.begin()) : -1;
}

QVariantMap TInventoryGemListModel::rowData(const QModelIndex& Index) { return modelRowData(*this, Index); }

QVariantMap TInventoryGemListModel::dataForId(const int gemId) const
{
    const int row = rowOfGem(gemId);
    if (row < 0)
        return {};
    return modelRowData(*this, index(row, 0));
}

TInventoryGemSortFilterProxyModel::TInventoryGemSortFilterProxyModel(TInventoryGemListModel* source, QObject* parent)
    : cip::tibia::skillwheel::TInventoryGemSortFilterProxyModel(parent)
    , m_source(source)
{
    beginFilterChange();
    m_filterModName = QString();
    m_filterGemQuality = -1;
    m_showLockedOnly = false;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
    setSourceModel(source);
}

void TInventoryGemSortFilterProxyModel::refilter()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
}

QModelIndex TInventoryGemSortFilterProxyModel::indexOfGem(const int gemId) const
{
    const QVariant id(static_cast<int>(static_cast<uint16_t>(gemId)));
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex index = this->index(row, 0);
        if (data(index, TInventoryGemListModel::GemId) == id)
            return index;
    }
    return {};
}

void TInventoryGemSortFilterProxyModel::setFilterModName(const QString& v)
{
    if (v == m_filterModName)
        return;
    m_filterModName = v;
    refilter();
}

void TInventoryGemSortFilterProxyModel::setFilterGemQuality(const int v)
{
    if (v == m_filterGemQuality)
        return;
    m_filterGemQuality = v;
    refilter();
}

void TInventoryGemSortFilterProxyModel::setShowLockedOnly(const bool v)
{
    if (v == m_showLockedOnly)
        return;
    m_showLockedOnly = v;
    refilter();
}

bool TInventoryGemSortFilterProxyModel::accepts(const TGem& gem) const
{
    const bool secondMod = gem.quality >= 1;
    const bool supremeMod = gem.quality >= 2;
    if (!m_filterModName.isEmpty()) {
        // The gem's mod texts at the configuration's grades, joined on one line.
        const int voc = m_source->vocationIndex();
        QStringList texts{ basicModEffect(gem.basic1, voc, m_source->basicGrade(gem.basic1)) };
        if (secondMod)
            texts << basicModEffect(gem.basic2, voc, m_source->basicGrade(gem.basic2));
        if (supremeMod) {
            if (const QString name = supremeModName(gem.supreme, false, false); !name.isEmpty())
                texts << name;
            texts << supremeModEffect(gem.supreme, m_source->supremeGrade(gem.supreme));
        }
        const QString joined = texts.join(u' ').replace(u'\n', u' ');
        const bool matches = joined.contains(m_filterModName, Qt::CaseInsensitive)
                             || std::ranges::any_of(texts, [&](const QString& t) { return t.contains(m_filterModName, Qt::CaseInsensitive); });
        if (!matches)
            return false;
    }
    if (m_filterGemQuality != -1 && m_filterGemQuality != (secondMod ? (supremeMod ? 2 : 1) : 0))
        return false;
    return gem.locked || !m_showLockedOnly;
}

bool TInventoryGemSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const TGem* gem = m_source->gem(sourceRow);
    return gem && accepts(*gem);
}

QVariantMap TInventoryGemSortFilterProxyModel::rowData(const QModelIndex& Index) { return modelRowData(*this, Index); }

TModListModel::TModListModel(QObject* parent)
    : cip::tibia::skillwheel::TModListModel(parent)
{
}

int TModListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_mods.size()); }

QHash<int, QByteArray> TModListModel::roleNames() const
{
    return {
        { ModImageSource, "modImageSource" }, { ModId, "modID" }, { IsBasicMod, "isBasicMod" }, { IsSocketed, "isSocketed" },
        { Grade, "grade" }, { AmountOnGemsInAtelier, "amountOnGemsInAtelier" }, { ModName, "modName" }, { AugmentName, "augmentName" },
        { Grade1Bonus, "grade1Bonus" }, { Grade2Bonus, "grade2Bonus" }, { Grade3Bonus, "grade3Bonus" }, { Grade4Bonus, "grade4Bonus" },
    };
}

void TModListModel::setMods(const std::vector<TModListEntry>& mods)
{
    beginResetModel();
    m_mods = mods;
    endResetModel();
}

void TModListModel::setConfiguration(const TWheel& configuration, const TGrades& basicGrades, const TGrades& supremeGrades)
{
    m_configuration = configuration;
    m_basicGrades = basicGrades;
    m_supremeGrades = supremeGrades;
}

int TModListModel::vocationIndex() const { return m_configuration ? m_configuration->vocation - 1 : 0; }

int TModListModel::basicGrade(const int mod) const
{
    const auto it = m_basicGrades.find(mod);
    return it != m_basicGrades.end() ? it->second : 0;
}

int TModListModel::supremeGrade(const int mod) const
{
    const auto it = m_supremeGrades.find(mod);
    return it != m_supremeGrades.end() ? it->second : 0;
}

QVariant TModListModel::data(const QModelIndex& index, const int role) const
{
    const TModListEntry* e = entry(index.row());
    if (!index.isValid() || !e)
        return {};
    // Basic mods read their values for the configuration's vocation; supreme mods name
    // themselves only when their kind (augmentations, revelation masteries) needs it.
    const auto bonus = [&](const int grade) {
        return e->basic ? basicModEffect(e->mod, vocationIndex(), grade) : supremeModEffect(e->mod, grade);
    };
    switch (role) {
        case ModImageSource: return e->basic ? basicModImage(e->mod) : supremeModImage(e->mod);
        case ModId: return e->mod;
        case IsBasicMod: return e->basic;
        case IsSocketed: return e->active;
        case Grade: return e->grade;
        case AmountOnGemsInAtelier: return e->gemCount;
        case ModName: {
            if (e->basic)
                return basicModEffect(e->mod, vocationIndex(), e->grade);
            const QString effect = supremeModEffect(e->mod, e->grade);
            if (showsModName(e->mod))
                return supremeModName(e->mod, false, false) + u'\n' + effect;
            return effect;
        }
        case AugmentName: return e->basic ? QString() : supremeModName(e->mod, false, false);
        case Grade1Bonus: return bonus(0);
        case Grade2Bonus: return bonus(1);
        case Grade3Bonus: return bonus(2);
        case Grade4Bonus: return bonus(3);
        default: return {};
    }
}

const TModListEntry* TModListModel::entry(const int row) const
{
    return row >= 0 && row < static_cast<int>(m_mods.size()) ? &m_mods[static_cast<size_t>(row)] : nullptr;
}

QVariantMap TModListModel::rowData(const QModelIndex& Index) { return modelRowData(*this, Index); }


QModelIndex TModListSortFilterProxyModel::indexOfSupremeMod(const int modId) const
{
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex index = this->index(row, 0);
        if (data(index, TModListModel::ModId) == QVariant(modId) && !data(index, TModListModel::IsBasicMod).toBool())
            return index;
    }
    return {};
}

QModelIndex TModListSortFilterProxyModel::indexOfBasicMod(const int modId) const
{
    for (int row = 0; row < rowCount(); ++row) {
        const QModelIndex index = this->index(row, 0);
        if (data(index, TModListModel::ModId) == QVariant(modId) && data(index, TModListModel::IsBasicMod).toBool())
            return index;
    }
    return {};
}

TModListSortFilterProxyModel::TModListSortFilterProxyModel(TModListModel* source, QObject* parent)
    : cip::tibia::skillwheel::TModListSortFilterProxyModel(parent)
    , m_source(source)
{
    beginFilterChange();
    m_filterModName = QString();
    m_filterMods = 0;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
    setSourceModel(source);
}

void TModListSortFilterProxyModel::setFilterModName(const QString& v)
{
    if (v == m_filterModName)
        return;
    beginFilterChange();
    m_filterModName = v;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
}

void TModListSortFilterProxyModel::setFilterMods(const int v)
{
    if (v == m_filterMods)
        return;
    beginFilterChange();
    m_filterMods = v;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
}

QVariantMap TModListSortFilterProxyModel::rowData(const QModelIndex& Index) { return modelRowData(*this, Index); }

bool TModListSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const TModListEntry* e = m_source->entry(sourceRow);
    if (!e)
        return false;
    if (!m_filterModName.isEmpty()) {
        // The mod's texts at the configuration's grade, joined on one line.
        QStringList texts;
        if (e->basic) {
            texts << basicModEffect(e->mod, m_source->vocationIndex(), m_source->basicGrade(e->mod));
        } else {
            if (const QString name = supremeModName(e->mod, false, false); !name.isEmpty())
                texts << name;
            texts << supremeModEffect(e->mod, m_source->supremeGrade(e->mod));
        }
        const QString joined = texts.join(u' ').replace(u'\n', u' ');
        if (!joined.contains(m_filterModName, Qt::CaseInsensitive)
            && !std::ranges::any_of(texts, [&](const QString& t) { return t.contains(m_filterModName, Qt::CaseInsensitive); }))
            return false;
    }
    if (m_filterMods >= 4 && m_filterMods <= 7 && e->grade != m_filterMods - 4)
        return false;
    if (m_filterMods == 1 && !e->basic)
        return false;
    if (m_filterMods == 2 && e->basic)
        return false;
    return e->active || m_filterMods != 3;
}

QHash<int, QByteArray> TPerkInfoModel::roleNames() const
{
    return { { Label, "label" }, { Effect, "effekt" }, { AdditionalInfo, "additionalInfo" }, { IsSeparator, "isSeparator" }, { ImageSource, "imageSource" } };
}

QVariant TPerkInfoModel::itemData(const TPerkInfo& item, const int role) const
{
    switch (role) {
        case Label: return item.label;
        case Effect: return item.effect;
        case AdditionalInfo: return item.additionalInfo;
        case IsSeparator: return item.isSeparator;
        case ImageSource: return item.imageSource;
        default: return {};
    }
}

TPresetInfoModel::TPresetInfoModel(QObject* parent) : TSimpleVectorAbstractListModel(parent) { m_nameEnumValue = Name; }

int TPresetInfoModel::nameEnumValue() const { return m_nameEnumValue; }

QHash<int, QByteArray> TPresetInfoModel::roleNames() const { return { { Name, "name" }, { AvailablePoints, "availablePoints" } }; }

void TPresetInfoModel::setTotalPoints(const int points)
{
    if (m_totalPoints == points)
        return;
    m_totalPoints = points;
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

QVariant TPresetInfoModel::itemData(const TPresetInfo& item, const int role) const
{
    if (role == AvailablePoints)
        return m_totalPoints - std::accumulate(item.wheel.points.begin(), item.wheel.points.end(), 0);
    if (role == Name)
        return item.name;
    return {};
}

}
