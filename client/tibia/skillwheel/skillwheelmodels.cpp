#include "skillwheelmodels.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <algorithm>

namespace tibia::skillwheel {

namespace {

bool socketedIn(const TWheel& wheel, const int gemId) { return std::ranges::find(wheel.gems, gemId) != wheel.gems.end(); }

QString itemName(const int typeId)
{
    if (typeId <= 0)
        return {};
    const ThingTypePtr& type = g_things.getThingType(static_cast<uint16_t>(typeId), ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP 0x100f9b620 role 0x112: augmentations show their name above the effect.
QString supremeEffectText(const int mod, const int grade)
{
    const QString effect = supremeModEffect(mod, grade);
    return isAugmentationMod(mod) ? supremeModName(mod, false, false) + u'\n' + effect : effect;
}

}

TPaginationProxyModel::TPaginationProxyModel(const int entriesPerPage, QObject* parent) : cip::shared::TPaginationProxyModel(parent)
{
    m_entriesPerPage = std::max(1, entriesPerPage);
    m_currentPage = 1;
    m_numberOfPages = 1;
}

void TPaginationProxyModel::setSourceModel(QAbstractItemModel* source)
{
    if (sourceModel())
        disconnect(sourceModel(), nullptr, this, nullptr);
    cip::shared::TPaginationProxyModel::setSourceModel(source);
    if (source) {
        for (auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved })
            connect(source, signal, this, &TPaginationProxyModel::updatePages);
        connect(source, &QAbstractItemModel::modelReset, this, &TPaginationProxyModel::updatePages);
        connect(source, &QAbstractItemModel::layoutChanged, this, &TPaginationProxyModel::updatePages);
    }
    updatePages();
}

void TPaginationProxyModel::setEntriesPerPage(const int v)
{
    if (v < 1 || v == m_entriesPerPage)
        return;
    m_entriesPerPage = v;
    updatePages();
}

void TPaginationProxyModel::setCurrentPage(const int v)
{
    const int page = std::clamp(v, 1, std::max(1, m_numberOfPages));
    if (page == m_currentPage)
        return;
    m_currentPage = page;
    updatePages();
}

void TPaginationProxyModel::updatePages()
{
    const int rows = sourceModel() ? sourceModel()->rowCount() : 0;
    m_numberOfPages = std::max(1, (rows + m_entriesPerPage - 1) / m_entriesPerPage);
    m_currentPage = std::clamp(m_currentPage, 1, m_numberOfPages);
    m_hasPreviousPage = m_currentPage > 1;
    m_hasNextPage = m_currentPage < m_numberOfPages;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT paginationDataChanged();
}

bool TPaginationProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const int first = (m_currentPage - 1) * m_entriesPerPage;
    return sourceRow >= first && sourceRow < first + m_entriesPerPage;
}

QVariantMap TPaginationProxyModel::rowData(const QModelIndex& Index)
{
    QVariantMap row;
    if (!Index.isValid() || Index.model() != this)
        return row;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row.insert(QString::fromUtf8(it.value()), data(Index, it.key()));
    return row;
}

QModelIndex TPaginationProxyModel::showSourceRow(const int sourceRow)
{
    if (sourceRow < 0)
        return {};
    setCurrentPage(sourceRow / m_entriesPerPage + 1);
    return mapFromSource(sourceModel()->index(sourceRow, 0));
}

TInventoryGemListModel::TInventoryGemListModel(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::skillwheel::TInventoryGemListModel(parent)
    , m_storage(storage)
{}

int TInventoryGemListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_storage->gems().size()); }

QHash<int, QByteArray> TInventoryGemListModel::roleNames() const
{
    return {
        { GemImageSource, "gemImageSource" },       { DomainAffinitySource, "domainAffinitySource" },
        { GemName, "gemName" },                     { Mod1Id, "mod1Id" },
        { Mod1ImageSource, "mod1ImageSource" },     { Mod1EffectString, "mod1EffectString" },
        { Mod1Active, "mod1Active" },               { Mod1Grade, "mod1Grade" },
        { Mod1PotentialGrade, "mod1PotentialGrade" }, { Mod2Id, "mod2Id" },
        { Mod2ImageSource, "mod2ImageSource" },     { Mod2EffectString, "mod2EffectString" },
        { Mod2Active, "mod2Active" },               { Mod2Grade, "mod2Grade" },
        { Mod2PotentialGrade, "mod2PotentialGrade" }, { Mod3Id, "mod3Id" },
        { Mod3ImageSource, "mod3ImageSource" },     { Mod3EffectString, "mod3EffectString" },
        { Mod3Active, "mod3Active" },               { Mod3Grade, "mod3Grade" },
        { Mod3PotentialGrade, "mod3PotentialGrade" }, { IsSocketed, "isSocketed" },
        { GemId, "gemId" },                         { CanBeDismantled, "canBeDismantled" },
        { IsLastOfDomainAffinity, "isLastOfDomainAffinity" }, { IsLocked, "isLocked" },
    };
}

QVariant TInventoryGemListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const TGem& gem = m_storage->gems()[index.row()];
    const int voc = m_storage->vocationIndex();
    const TGrades& basic = m_storage->basicGrades();
    const TGrades& supreme = m_storage->supremeGrades();
    const int count = modCount(gem);
    const int res = resonance(m_storage->currentWheel().points, gem.domain);
    switch (role) {
        case GemImageSource: return gemImage(gem, voc);
        case DomainAffinitySource: return domainAffinityImage(gem.domain);
        case GemName: return itemName(unrevealedGemTypeId(voc, gem.quality));
        case Mod1Id: return gem.basic1;
        case Mod1ImageSource: return basicModImage(gem.basic1);
        case Mod1EffectString: return basicModEffect(gem.basic1, voc, effectiveGrade(gem, basic, supreme, 0));
        case Mod1Active: return res > 0;
        case Mod1Grade:
        case Mod1PotentialGrade: return effectiveGrade(gem, basic, supreme, 0);
        case Mod2Id: return count > 1 ? gem.basic2 : 0;
        case Mod2ImageSource: return count > 1 ? basicModImage(gem.basic2) : QUrl();
        case Mod2EffectString: return count > 1 ? basicModEffect(gem.basic2, voc, effectiveGrade(gem, basic, supreme, 1)) : QString();
        case Mod2Active: return res > 1;
        case Mod2Grade: return count > 1 ? effectiveGrade(gem, basic, supreme, 1) : 0;
        case Mod2PotentialGrade: return count > 1 ? gemGrade(gem, basic, supreme, 1) : 0;
        case Mod3Id: return count > 2 ? gem.supreme : 0;
        case Mod3ImageSource: return count > 2 ? supremeModImage(gem.supreme) : QUrl();
        case Mod3EffectString: return count > 2 ? supremeEffectText(gem.supreme, effectiveGrade(gem, basic, supreme, 2)) : QString();
        case Mod3Active: return res > 2;
        case Mod3Grade: return count > 2 ? effectiveGrade(gem, basic, supreme, 2) : 0;
        case Mod3PotentialGrade: return count > 2 ? gemGrade(gem, basic, supreme, 2) : 0;
        case IsSocketed: return socketedIn(m_storage->currentWheel(), gem.id);
        case GemId: return gem.id;
        case CanBeDismantled: return !socketedIn(m_storage->serverWheel(), gem.id);
        case IsLastOfDomainAffinity:
            return std::ranges::count_if(m_storage->gems(), [&](const TGem& g) { return g.domain == gem.domain; }) <= 1;
        case IsLocked: return gem.locked;
        default: return {};
    }
}

void TInventoryGemListModel::reset()
{
    beginResetModel();
    endResetModel();
}

void TInventoryGemListModel::refresh()
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

int TInventoryGemListModel::rowOfGem(const int gemId) const
{
    const auto& gems = m_storage->gems();
    const auto it = std::ranges::find_if(gems, [gemId](const TGem& g) { return g.id == gemId; });
    return it != gems.end() ? static_cast<int>(it - gems.begin()) : -1;
}

TInventoryGemSortFilterProxyModel::TInventoryGemSortFilterProxyModel(TSkillWheelStorage* storage, QObject* parent)
    : cip::tibia::skillwheel::TInventoryGemSortFilterProxyModel(parent)
    , m_storage(storage)
{
    m_filterDomainAffinity = -1;
    m_filterGemQuality = -1;
}

void TInventoryGemSortFilterProxyModel::refilter()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TInventoryGemSortFilterProxyModel::setFilterModName(const QString& v)
{
    if (v == m_filterModName)
        return;
    m_filterModName = v;
    refilter();
}

void TInventoryGemSortFilterProxyModel::setFilterDomainAffinity(const int v)
{
    if (v == m_filterDomainAffinity)
        return;
    m_filterDomainAffinity = v;
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
    if (!m_filterModName.isEmpty()) {
        const int voc = m_storage->vocationIndex();
        const TGrades& basic = m_storage->basicGrades();
        const TGrades& supreme = m_storage->supremeGrades();
        QStringList texts{ basicModEffect(gem.basic1, voc, effectiveGrade(gem, basic, supreme, 0)) };
        if (gem.quality >= 1)
            texts << basicModEffect(gem.basic2, voc, effectiveGrade(gem, basic, supreme, 1));
        if (gem.quality >= 2) {
            if (const QString name = supremeModName(gem.supreme, false, false); !name.isEmpty())
                texts << name;
            texts << supremeModEffect(gem.supreme, effectiveGrade(gem, basic, supreme, 2));
        }
        const QString joined = texts.join(u' ').replace(u'\n', u' ');
        const bool matches = joined.contains(m_filterModName, Qt::CaseInsensitive)
                             || std::ranges::any_of(texts, [&](const QString& t) { return t.contains(m_filterModName, Qt::CaseInsensitive); });
        if (!matches)
            return false;
    }
    if (m_filterDomainAffinity != -1 && gem.domain != m_filterDomainAffinity)
        return false;
    if (m_filterGemQuality != -1 && gem.quality != m_filterGemQuality)
        return false;
    return gem.locked || !m_showLockedOnly;
}

bool TInventoryGemSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const auto& gems = m_storage->gems();
    return sourceRow >= 0 && sourceRow < static_cast<int>(gems.size()) && accepts(gems[sourceRow]);
}

TModListModel::TModListModel(TSkillWheelStorage* storage, QObject* parent) : QAbstractListModel(parent), m_storage(storage) {}

int TModListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QHash<int, QByteArray> TModListModel::roleNames() const
{
    return {
        { ModImageSource, "modImageSource" }, { ModId, "modID" }, { IsBasicMod, "isBasicMod" }, { IsSocketed, "isSocketed" },
        { Grade, "grade" }, { AmountOnGemsInAtelier, "amountOnGemsInAtelier" }, { ModName, "modName" }, { AugmentName, "augmentName" },
        { Grade1Bonus, "grade1Bonus" }, { Grade2Bonus, "grade2Bonus" }, { Grade3Bonus, "grade3Bonus" }, { Grade4Bonus, "grade4Bonus" },
    };
}

QString TModListModel::modName(const TModEntry& entry) const
{
    if (entry.isBasic)
        return basicModEffect(entry.modId, m_storage->vocationIndex(), entry.grade);
    if (isAugmentationMod(entry.modId) || revelationMasteryPerk(entry.modId) >= 0)
        return supremeModName(entry.modId, false, false) + u'\n' + supremeModEffect(entry.modId, entry.grade);
    return supremeModEffect(entry.modId, entry.grade);
}

QVariant TModListModel::data(const QModelIndex& index, const int role) const
{
    const TModEntry* e = entry(index.row());
    if (!index.isValid() || !e)
        return {};
    const auto bonus = [&](const int grade) {
        return e->isBasic ? basicModEffect(e->modId, m_storage->vocationIndex(), grade) : supremeModEffect(e->modId, grade);
    };
    switch (role) {
        case ModImageSource: return e->isBasic ? basicModImage(e->modId) : supremeModImage(e->modId);
        case ModId: return e->modId;
        case IsBasicMod: return e->isBasic;
        case IsSocketed: return e->isSocketed;
        case Grade: return e->grade;
        case AmountOnGemsInAtelier: return e->amount;
        case ModName: return modName(*e);
        case AugmentName: return e->isBasic ? QString() : supremeModName(e->modId, false, false);
        case Grade1Bonus: return bonus(0);
        case Grade2Bonus: return bonus(1);
        case Grade3Bonus: return bonus(2);
        case Grade4Bonus: return bonus(3);
        default: return {};
    }
}

const TModEntry* TModListModel::entry(const int row) const { return row >= 0 && row < m_entries.size() ? &m_entries[row] : nullptr; }

int TModListModel::rowOfMod(const bool isBasic, const int modId) const
{
    for (int row = 0; row < m_entries.size(); ++row)
        if (m_entries[row].isBasic == isBasic && m_entries[row].modId == modId)
            return row;
    return -1;
}

/// CIP mod list build: the vocation's supreme mods (0..93), then the basic
/// mods; the count on revealed gems and whether a socketed gem has the mod.
void TModListModel::reset()
{
    beginResetModel();
    m_entries.clear();
    const int voc = m_storage->vocationIndex();
    const auto& gems = m_storage->gems();
    const TWheel& wheel = m_storage->currentWheel();
    const auto grade = [](const TGrades& grades, const int mod) {
        const auto it = grades.find(mod);
        return it != grades.end() ? it->second : 0;
    };
    for (int mod = 0; mod < supremeModCount(); ++mod) {
        if (m_storage->hasData() && !supremeModForVocation(mod, voc))
            continue;
        TModEntry e{ false, false, mod, 0, grade(m_storage->supremeGrades(), mod) };
        for (const TGem& g : gems)
            if (g.quality >= 2 && g.supreme == mod) {
                ++e.amount;
                e.isSocketed = e.isSocketed || socketedIn(wheel, g.id);
            }
        m_entries.append(e);
    }
    for (int mod = 0; mod < basicModCount(); ++mod) {
        if (!hasBasicMod(mod))
            continue;
        TModEntry e{ true, false, mod, 0, grade(m_storage->basicGrades(), mod) };
        for (const TGem& g : gems) {
            const int hits = (g.basic1 == mod ? 1 : 0) + (g.quality >= 1 && g.basic2 == mod ? 1 : 0);
            e.amount += hits;
            e.isSocketed = e.isSocketed || (hits > 0 && socketedIn(wheel, g.id));
        }
        m_entries.append(e);
    }
    endResetModel();
}

TModFilterProxyModel::TModFilterProxyModel(TModListModel* source, QObject* parent) : QSortFilterProxyModel(parent), m_source(source)
{
    setSourceModel(source);
}

void TModFilterProxyModel::setFilterModName(const QString& v)
{
    if (v == m_filterModName)
        return;
    beginFilterChange();
    m_filterModName = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TModFilterProxyModel::setFilterMods(const int v)
{
    if (v == m_filterMods)
        return;
    beginFilterChange();
    m_filterMods = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

bool TModFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    const TModEntry* e = m_source->entry(sourceRow);
    if (!e)
        return false;
    if (!m_filterModName.isEmpty()) {
        QStringList texts;
        if (e->isBasic)
            texts << basicModEffect(e->modId, m_source->vocationIndex(), e->grade);
        else
            texts << supremeModName(e->modId, false, false) << supremeModEffect(e->modId, e->grade);
        const QString joined = texts.join(u' ').replace(u'\n', u' ');
        if (!joined.contains(m_filterModName, Qt::CaseInsensitive)
            && !std::ranges::any_of(texts, [&](const QString& t) { return t.contains(m_filterModName, Qt::CaseInsensitive); }))
            return false;
    }
    switch (m_filterMods) {
        case 1: return e->isBasic;
        case 2: return !e->isBasic;
        case 3: return e->isSocketed;
        case 4:
        case 5:
        case 6:
        case 7: return e->grade == m_filterMods - 4;
        default: return true;
    }
}

int TSummaryModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QHash<int, QByteArray> TSummaryModel::roleNames() const
{
    return { { Label, "lable" }, { Effect, "effekt" }, { AdditionalInfo, "additionalInfo" }, { ImageSource, "imageSource" }, { IsSeparator, "isSeparator" } };
}

QVariant TSummaryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const TSummaryEntry& e = m_entries[index.row()];
    switch (role) {
        case Label: return e.label;
        case Effect: return e.effect;
        case AdditionalInfo: return e.additionalInfo;
        case ImageSource: return e.image;
        case IsSeparator: return e.separator;
        default: return {};
    }
}

void TSummaryModel::setEntries(QList<TSummaryEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TPresetInfoModel::TPresetInfoModel(TSkillWheelStorage* storage, QObject* parent) : cip::TPresetInfoModel(parent), m_storage(storage)
{
    m_nameEnumValue = Name;
}

int TPresetInfoModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_storage->presets().size()); }

QHash<int, QByteArray> TPresetInfoModel::roleNames() const { return { { Name, "name" }, { AvailablePoints, "availablePoints" } }; }

QVariant TPresetInfoModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const TPreset& preset = m_storage->presets()[index.row()];
    switch (role) {
        case Name: return preset.name;
        case AvailablePoints: {
            int spent = 0;
            for (const int p : preset.wheel.points)
                spent += p;
            return m_storage->totalPoints() - spent;
        }
        default: return {};
    }
}

void TPresetInfoModel::reset()
{
    beginResetModel();
    endResetModel();
}

}
