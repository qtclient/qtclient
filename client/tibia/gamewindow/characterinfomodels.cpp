#include "characterinfomodels.h"
#include "cyclopedia/staticcontentinformation.h"
#include "colorhelper.h"

#include "analysers/itempricestorage.h"
#include "cyclopedia/characterinfostorage.h"
#include "config/clientoptions.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/contextmenucontroller.h"

#include <tibia/game/gamesession.h>
#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QDateTime>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

QString appearanceName(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP : adds one wire list to the rows, the amount going to
/// `amount` (row +0x24 inventory, +0x28 depot, +0x2c stash, +0x30 inbox,
/// +0x34 store inbox).
void mergeList(std::vector<ItemSummaryEntry>& rows, const std::vector<ItemSummary>& list, uint32_t ItemSummaryEntry::* amount)
{
    analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
    rows.reserve(rows.size() + list.size());
    for (const ItemSummary& item : list) {
        const auto it = std::lower_bound(rows.begin(), rows.end(), item, [](const ItemSummaryEntry& row, const ItemSummary& key) {
            return row.typeId != key.itemId ? row.typeId < key.itemId : row.upgradeTier < key.tier;
        });
        if (it != rows.end() && it->typeId == item.itemId && it->upgradeTier == item.tier) {
            (*it).*amount += item.amount;
            continue;
        }
        ItemSummaryEntry row;
        row.typeId = item.itemId;
        row.upgradeTier = item.tier;
        row.*amount = item.amount;
        row.name = appearanceName(item.itemId);
        row.lootValueLevel = prices->lootValueLevel(item.itemId);
        rows.insert(it, std::move(row));
    }
}

/// CIP.
QString lootValueImage(const int level, const int style)
{
    if (level == 0 || config::TClientOptions::instance()->colorizeLootStyle() != style)
        return QStringLiteral("");
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

}

std::vector<ItemSummaryEntry> mergeItemSummary(const CyclopediaCharacterItemSummary& data)
{
    std::vector<ItemSummaryEntry> rows;
    mergeList(rows, data.inventory, &ItemSummaryEntry::inventoryAmount);
    mergeList(rows, data.stash, &ItemSummaryEntry::stashAmount);
    mergeList(rows, data.depot, &ItemSummaryEntry::depotAmount);
    mergeList(rows, data.inbox, &ItemSummaryEntry::inboxAmount);
    mergeList(rows, data.store, &ItemSummaryEntry::storeInboxAmount);
    return rows;
}

TItemSummaryModel::TItemSummaryModel(std::vector<ItemSummaryEntry> entries, QObject* parent)
    : QAbstractListModel(parent)
    , m_entries(std::move(entries))
{}

int TItemSummaryModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TItemSummaryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const ItemSummaryEntry& entry = m_entries[index.row()];
    switch (role) {
        case AppearanceTypeID: return static_cast<uint>(entry.typeId);
        case UpgradeTier: return static_cast<uint>(entry.upgradeTier);
        case Name: return entry.name;
        case InventoryAmount: return entry.inventoryAmount;
        case StashAmount: return entry.stashAmount;
        case DepotAmount: return entry.depotAmount;
        case InboxAmount: return entry.inboxAmount;
        case StoreInboxAmount: return entry.storeInboxAmount;
        case TotalAmount:
            return entry.inboxAmount + entry.depotAmount + entry.stashAmount + entry.inventoryAmount + entry.storeInboxAmount;
        case LootValueSourceUnder: return lootValueImage(entry.lootValueLevel, 1);
        case LootValueSourceOver: return lootValueImage(entry.lootValueLevel, 2);
        default: return {};
    }
}

QHash<int, QByteArray> TItemSummaryModel::roleNames() const
{
    return { { AppearanceTypeID, "appearanceTypeID" },
             { UpgradeTier, "upgradeTier" },
             { Name, "name" },
             { InventoryAmount, "inventoryAmount" },
             { StashAmount, "stashAmount" },
             { DepotAmount, "depotAmount" },
             { InboxAmount, "inboxAmount" },
             { StoreInboxAmount, "storeInboxAmount" },
             { TotalAmount, "totalAmount" },
             { LootValueSourceUnder, "lootValueSourceUnder" },
             { LootValueSourceOver, "lootValueSourceOver" } };
}

TItemSummaryModelSortFilterProxy::TItemSummaryModelSortFilterProxy(std::vector<ItemSummaryEntry> entries, QObject* parent)
    : cip::tibia::gamewindow::TItemSummaryModelSortFilterProxy(parent)
    , m_model(new TItemSummaryModel(std::move(entries), this))
{
    m_showInventory = true;
    setSourceModel(m_model);
    sort(0);
}

void TItemSummaryModelSortFilterProxy::setFlag(bool& flag, const bool value)
{
    if (flag == value)
        return;
    beginFilterChange();
    flag = value;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
    Q_EMIT filterChanged();
}

void TItemSummaryModelSortFilterProxy::setShowInventory(const bool v) { setFlag(m_showInventory, v); }
void TItemSummaryModelSortFilterProxy::setShowStash(const bool v) { setFlag(m_showStash, v); }
void TItemSummaryModelSortFilterProxy::setShowDepot(const bool v) { setFlag(m_showDepot, v); }
void TItemSummaryModelSortFilterProxy::setShowInbox(const bool v) { setFlag(m_showInbox, v); }
void TItemSummaryModelSortFilterProxy::setShowStoreInbox(const bool v) { setFlag(m_showStoreInbox, v); }

void TItemSummaryModelSortFilterProxy::updateNameFilter(const QString& NameFilter)
{
    if (m_nameFilter == NameFilter)
        return;
    beginFilterChange();
    m_nameFilter = NameFilter;
    endFilterChange(QSortFilterProxyModel::Direction::Both);
}

bool TItemSummaryModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_model->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    const auto has = [&](const bool shown, const int role) { return shown && index.data(role).toInt() > 0; };
    using R = TItemSummaryModel::Role;
    if (!has(m_showInventory, R::InventoryAmount) && !has(m_showStash, R::StashAmount) && !has(m_showDepot, R::DepotAmount)
        && !has(m_showInbox, R::InboxAmount) && !has(m_showStoreInbox, R::StoreInboxAmount))
        return false;
    return m_nameFilter.isEmpty() || index.data(R::Name).toString().contains(m_nameFilter, Qt::CaseInsensitive);
}

bool TItemSummaryModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    using R = TItemSummaryModel::Role;
    const int order = QString::compare(left.data(R::Name).toString(), right.data(R::Name).toString(), Qt::CaseInsensitive);
    if (order != 0)
        return order < 0;
    return left.data(R::UpgradeTier).toInt() < right.data(R::UpgradeTier).toInt();
}

TTitlesModel::TTitlesModel(std::vector<TitleEntry> entries, QObject* parent)
    : QAbstractListModel(parent)
    , m_entries(std::move(entries))
{}

int TTitlesModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_entries.size()); }

QVariant TTitlesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const TitleEntry& entry = m_entries[index.row()];
    switch (role) {
        case TitleId: return static_cast<uint>(entry.id);
        case Name: return entry.name;
        case Description: return entry.description;
        case IsPermanent: return entry.permanent;
        case IsUnlocked: return entry.unlocked;
        case CanBeChosen: return entry.unlocked || !entry.permanent;
        default: return {};
    }
}

QHash<int, QByteArray> TTitlesModel::roleNames() const
{
    return { { TitleId, "titleId" },         { Name, "name" },          { Description, "description" },
             { IsPermanent, "isPermanent" }, { IsUnlocked, "isUnlocked" }, { CanBeChosen, "canBeChosen" } };
}

TTitlesSortProxyModel::TTitlesSortProxyModel(std::vector<TitleEntry> entries, QObject* parent)
    : cip::tibia::gamewindow::TTitlesSortProxyModel(parent)
{
    setSourceModel(new TTitlesModel(std::move(entries), this));
    sort(0);
    // CIP Q_PROPERTY WRITE is FINAL: QML may call the base setter, which updates
    // m_titleFilter / m_nameFilter but does not invalidate the proxy.
    connect(this, &TTitlesSortProxyModel::filterChanged, this, [this] {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    });
}

void TTitlesSortProxyModel::setNameFilter(const QString& v)
{
    if (m_nameFilter == v)
        return;
    beginFilterChange();
    m_nameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TTitlesSortProxyModel::setTitleFilter(const EQmlTitleFilter v)
{
    if (m_titleFilter == v)
        return;
    beginFilterChange();
    m_titleFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

bool TTitlesSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    using R = TTitlesModel::Role;
    if (!m_nameFilter.isEmpty() && !index.data(R::Name).toString().contains(m_nameFilter, Qt::CaseInsensitive))
        return false;
    switch (m_titleFilter) {
        case ShowPermanentTitles: return index.data(R::IsPermanent).toBool();
        case ShowTemporaryTitles: return !index.data(R::IsPermanent).toBool();
        case ShowUnlockedTitles: return index.data(R::IsUnlocked).toBool();
        case ShowLockedTitles: return !index.data(R::IsUnlocked).toBool();
        default: return true;
    }
}

bool TTitlesSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    using R = TTitlesModel::Role;
    // The names compared in lower case.
    return QString::compare(left.data(R::Name).toString().toLower(), right.data(R::Name).toString().toLower(), Qt::CaseSensitive) < 0;
}

TCharacterTitlesInfoQmlData::TCharacterTitlesInfoQmlData(const uint8_t currentTitleId, std::vector<TitleEntry> entries, QObject* parent)
    : cip::tibia::gamewindow::TCharacterTitlesInfoQmlData(parent)
{
    m_currentTitle = qtTrId("charinfo_character_title_no_title");
    m_isCurrentTitleActive = true;
    const auto current =
        std::find_if(entries.begin(), entries.end(), [currentTitleId](const TitleEntry& entry) { return entry.id == currentTitleId; });
    if (current != entries.end() && current->id != 0) {
        m_currentTitle = current->name;
        m_isCurrentTitleActive = current->unlocked;
        m_currentTitleId = current->id;
    }
    m_hasCurrentTitle = m_currentTitleId != 0;
    m_titlesModel = new TTitlesSortProxyModel(std::move(entries), this);
}

TOutfitsMountsSummonSkinsModel::TOutfitsMountsSummonSkinsModel(std::vector<AppearanceEntry> entries, QObject* parent)
    : cip::tibia::gamewindow::TOutfitsMountsSummonSkinsModel(parent)
    , m_entries(std::move(entries))
{}

int TOutfitsMountsSummonSkinsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TOutfitsMountsSummonSkinsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const AppearanceEntry& entry = m_entries[index.row()];
    const bool outfit = entry.kind == AppearanceEntry::Outfit;
    switch (role) {
        case OutfitID: return static_cast<int>(entry.id);
        case Name: return entry.name;
        case Source: return entry.source;
        case OfferID: return entry.offerId;
        case HasFirstAddon: return outfit && (entry.addons & 1) != 0;
        case HasSecondAddon: return outfit && (entry.addons & 2) != 0;
        case IsOutfit: return outfit;
        case IsMount: return entry.kind == AppearanceEntry::Mount;
        case IsSummonSkin: return entry.kind == AppearanceEntry::SummonSkin;
        default: return {};
    }
}

QHash<int, QByteArray> TOutfitsMountsSummonSkinsModel::roleNames() const
{
    return { { OutfitID, "outfitID" },           { Name, "name" },
             { Source, "source" },               { OfferID, "offerID" },
             { HasFirstAddon, "hasFirstAddon" }, { HasSecondAddon, "hasSecondAddon" },
             { IsOutfit, "isOutfit" },           { IsMount, "isMount" },
             { IsSummonSkin, "isSummonSkin" } };
}

TOutfitMountSummonSkinsListDataModelSortFilterProxy::TOutfitMountSummonSkinsListDataModelSortFilterProxy(std::vector<AppearanceEntry> entries,
                                                                                                         const Colors& colors, QObject* parent)
    : cip::tibia::gamewindow::TOutfitMountSummonSkinsListDataModelSortFilterProxy(parent)
    , m_model(new TOutfitsMountsSummonSkinsModel(std::move(entries), this))
    , m_colors(colors)
{
    setSourceModel(m_model);
}

void TOutfitMountSummonSkinsListDataModelSortFilterProxy::setTypeFilter(const EQmlOutfitMountTypeFilter v)
{
    if (m_typeFilter == v)
        return;
    m_typeFilter = v;
    invalidate();
    Q_EMIT typeFilterChanged();
}

void TOutfitMountSummonSkinsListDataModelSortFilterProxy::setSourceFilter(const EQmlOutfitMountSourceFilter v)
{
    if (m_sourceFilter == v || (v == Standard && m_typeFilter == Mounts) || (v == Store && m_typeFilter == SummonSkins))
        return;
    m_sourceFilter = v;
    invalidate();
    Q_EMIT sourceFilterChanged();
}

QColor TOutfitMountSummonSkinsListDataModelSortFilterProxy::color(const int part) const
{
    if (m_typeFilter != Outfits && m_typeFilter != Mounts)
        return QColor(Qt::black);
    const QColor c = tibia::shared::outfitColor(m_colors[(m_typeFilter == Mounts ? 4 : 0) + part]);
    return QColor(c.red(), c.green(), c.blue());
}

bool TOutfitMountSummonSkinsListDataModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = m_model->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    using R = TOutfitsMountsSummonSkinsModel::Role;
    const int source = index.data(R::Source).toInt();
    bool sourceShown = false;
    switch (m_sourceFilter) {
        case All: sourceShown = true; break;
        case Standard: sourceShown = source == TOutfitsMountsSummonSkinsModel::Standard; break;
        case Quest: sourceShown = source == TOutfitsMountsSummonSkinsModel::Quest; break;
        case Store: sourceShown = source == TOutfitsMountsSummonSkinsModel::Store; break;
    }
    switch (m_typeFilter) {
        case Outfits: return sourceShown && index.data(R::IsOutfit).toBool();
        case Mounts: return sourceShown && index.data(R::IsMount).toBool();
        case SummonSkins: return sourceShown && index.data(R::IsSummonSkin).toBool();
    }
    return false;
}

TAccountBadgesModel::TAccountBadgesModel(std::vector<AccountBadgeEntry> entries, QObject* parent)
    : QAbstractListModel(parent)
    , m_entries(std::move(entries))
{}

int TAccountBadgesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TAccountBadgesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const AccountBadgeEntry& entry = m_entries[index.row()];
    switch (role) {
        case BadgeID: return static_cast<uint>(entry.id);
        case BadgeName: return entry.name;
        // The icon atlas counts from 0, badge ids from 1.
        case BadgeIcon: return QStringLiteral("image://friendbadge-icons/%1").arg(quint64(entry.id) - 1);
        default: return {};
    }
}

QHash<int, QByteArray> TAccountBadgesModel::roleNames() const
{
    return { { BadgeID, "badgeID" }, { BadgeName, "badgeName" }, { BadgeIcon, "badgeIcon" } };
}

TQmlAchievement::TQmlAchievement(const AchievementEntry& entry, QObject* parent)
    : cip::tibia::gamewindow::TQmlAchievement(parent)
{
    m_name = entry.name;
    m_description = entry.description;
    m_grade = entry.grade;
    m_secret = entry.secret;
    // CIP .
    m_unlockDateString = entry.timestamp == 0
                             ? QStringLiteral("")
                             : QDateTime::fromSecsSinceEpoch(entry.timestamp).toString(QStringLiteral("yyyy-MM-dd"));
}

namespace {

/// CIP : the static achievements (staticdata).
const std::vector<cyclopedia::TStaticAchievement>& staticAchievements() { return cyclopedia::staticAchievements(); }

}

TAchievementInfoController::TAchievementInfoController(QObject* parent)
    : cip::tibia::gamewindow::TAchievementInfoController(parent)
{
    m_filterCriterion = Discovered;
}

void TAchievementInfoController::setData(const int points, const int totalSecret, std::vector<AchievementEntry> unlocked)
{
    m_unlocked = std::move(unlocked);
    m_achievementPointsSum = points;
    m_totalSecretAchievements = totalSecret;
    updateCounts();
    rebuildList();
    Q_EMIT achievementsChanged();
}

void TAchievementInfoController::reset()
{
    const QList<QObject*> old = std::exchange(m_achievementList, {});
    Q_EMIT achievementListChanged();
    for (QObject* object : old)
        object->deleteLater();
    m_sortingCriterion = Name;
    Q_EMIT sortingCriterionChanged();
    m_filterCriterion = Discovered;
    Q_EMIT filterCriterionChanged();
}

void TAchievementInfoController::setSortingCriterion(const EAchievmentSortingCriteria v)
{
    if (m_sortingCriterion == v)
        return;
    m_sortingCriterion = v;
    rebuildList();
    Q_EMIT sortingCriterionChanged();
}

void TAchievementInfoController::setFilterCriterion(const EAchievmentFilterCriteria v)
{
    if (m_filterCriterion == v)
        return;
    m_filterCriterion = v;
    rebuildList();
    Q_EMIT filterCriterionChanged();
}

void TAchievementInfoController::updateCounts()
{
    int grades[5] = {};
    int regular = 0;
    int secret = 0;
    for (const AchievementEntry& entry : m_unlocked) {
        if (entry.grade >= 1 && entry.grade <= 4)
            ++grades[entry.grade];
        (entry.secret ? secret : regular) += 1;
    }
    m_totalRegularAchievements = static_cast<int>(staticAchievements().size());
    Q_EMIT totalRegularAchievementsChanged();
    m_unlockedRegularAchievements = regular;
    Q_EMIT unlockedRegularAchievementsChanged();
    m_unlockedSecretAchievements = secret;
    Q_EMIT unlockedSecretAchievementsChanged();
    m_unlockedGrade1Achievements = grades[1];
    Q_EMIT unlockedGrade1AchievementsChanged();
    m_unlockedGrade2Achievements = grades[2];
    Q_EMIT unlockedGrade2AchievementsChanged();
    m_unlockedGrade3Achievements = grades[3];
    Q_EMIT unlockedGrade3AchievementsChanged();
    m_unlockedGrade4Achievements = grades[4];
    Q_EMIT unlockedGrade4AchievementsChanged();
}

void TAchievementInfoController::rebuildList()
{
    std::vector<AchievementEntry> rows;
    if (m_filterCriterion != Locked)
        rows = m_unlocked;
    if (m_filterCriterion != Discovered) {
        for (const cyclopedia::TStaticAchievement& known : staticAchievements()) {
            const bool unlocked =
                std::any_of(m_unlocked.begin(), m_unlocked.end(), [&known](const AchievementEntry& entry) { return entry.id == known.id; });
            if (!unlocked)
                rows.push_back({ known.id, 0, false, known.name, known.description, known.grade });
        }
    }

    // CIP (name), (newest first), (highest grade, then name).
    switch (m_sortingCriterion) {
        case Name:
            std::sort(rows.begin(), rows.end(), [](const AchievementEntry& a, const AchievementEntry& b) {
                return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
            });
            break;
        case UnlockDate:
            std::sort(rows.begin(), rows.end(), [](const AchievementEntry& a, const AchievementEntry& b) { return a.timestamp > b.timestamp; });
            break;
        case Grade:
            std::sort(rows.begin(), rows.end(), [](const AchievementEntry& a, const AchievementEntry& b) {
                if (a.grade != b.grade)
                    return a.grade > b.grade;
                return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
            });
            break;
    }

    const QList<QObject*> old = std::exchange(m_achievementList, {});
    m_achievementList.reserve(static_cast<qsizetype>(rows.size()));
    for (const AchievementEntry& entry : rows)
        m_achievementList.append(new TQmlAchievement(entry, this));
    Q_EMIT achievementListChanged();
    for (QObject* object : old)
        object->deleteLater();
}

namespace {

void replaceObject(QObject*& member, QObject* object)
{
    if (member)
        member->deleteLater();
    member = object;
}

/// CIP : the wire's 1 is a quest, 2 a store appearance.

}

/// CIP: a new proxy (inventory only) per answer, sorted.
void TCharacterInfoDialogController::updateItemSummary()
{
    if (!characterInfoStorage())
        return;
    const CyclopediaCharacterItemSummary summary = characterInfoStorage()->itemSummary();
    replaceObject(m_itemSummary, new TItemSummaryModelSortFilterProxy(mergeItemSummary(summary), this));
    Q_EMIT dataModelChanged();
}

/// CIP: a new proxy over the outfits, mounts and summon skins, with the
/// character's eight colors.
void TCharacterInfoDialogController::updateOutfitsMounts()
{
    const auto appearances = characterInfoStorage() ? characterInfoStorage()->outfitsMountsSummonSkins() : nullptr;
    if (!appearances)
        return;
    const auto& outfitColors = appearances->outfitColors;
    const auto& mountColors = appearances->mountColors;
    std::vector<AppearanceEntry> entries;
    entries.reserve(appearances->outfits.size() + appearances->mounts.size() + appearances->summonSkins.size());
    for (const auto& o : appearances->outfits)
        entries.push_back({ AppearanceEntry::Outfit, o.lookType, o.name, static_cast<int>(o.source), static_cast<int>(o.current),
                            static_cast<uint8_t>(o.addons) });
    for (const auto& m : appearances->mounts)
        entries.push_back({ AppearanceEntry::Mount, m.lookType, m.name, static_cast<int>(m.source), static_cast<int>(m.current), 0 });
    for (const auto& f : appearances->summonSkins)
        entries.push_back({ AppearanceEntry::SummonSkin, f.lookType, f.name, static_cast<int>(f.source), static_cast<int>(f.current), 0 });
    const TOutfitMountSummonSkinsListDataModelSortFilterProxy::Colors indexes{ outfitColors.head, outfitColors.body, outfitColors.legs,
                                                                               outfitColors.feet, mountColors.head, mountColors.body,
                                                                               mountColors.legs,  mountColors.feet };
    replaceObject(m_outfitMountModel, new TOutfitMountSummonSkinsListDataModelSortFilterProxy(std::move(entries), indexes, this));
    Q_EMIT dataModelChanged();
}

/// CIP: a new titles object with the current title.
void TCharacterInfoDialogController::updateTitles()
{
    const auto info = characterInfoStorage() ? characterInfoStorage()->titlesInfo() : nullptr;
    if (!info)
        return;
    std::vector<TitleEntry> entries;
    entries.reserve(info->titles.size());
    for (const cyclopedia::TCharacterTitle& title : info->titles)
        entries.push_back({ title.id, title.name, title.description, title.permanent, title.unlocked });
    replaceObject(m_titlesInfo, new TCharacterTitlesInfoQmlData(info->currentTitle, std::move(entries), this));
    Q_EMIT dataModelChanged();
}

/// The achievements controller from the record: points, secret total and the
/// unlocked achievements.
void TCharacterInfoDialogController::updateAchievements()
{
    if (!characterInfoStorage())
        return;
    const cyclopedia::TAchievementStorage& storage = characterInfoStorage()->achievementStorage();
    std::vector<AchievementEntry> unlocked;
    unlocked.reserve(storage.achievements.size());
    for (const cyclopedia::TAchievementStorage::TAchievement& achievement : storage.achievements)
        unlocked.push_back({ achievement.id, achievement.timestamp, achievement.secret, achievement.name, achievement.description, achievement.grade });
    if (auto* controller = dynamic_cast<TAchievementInfoController*>(m_achievementsController))
        controller->setData(storage.achievementPoints, storage.secretAchievements, std::move(unlocked));
}

/// CIP .
void TCharacterInfoDialogController::resetCharacterTitle() { tibia::game::requestSetCharacterTitle(0); }

void TCharacterInfoDialogController::setOrToggleCharacterTitle(const uint characterTitleId)
{
    const auto* titles = static_cast<cip::tibia::gamewindow::TCharacterTitlesInfoQmlData*>(m_titlesInfo);
    if (!titles || characterTitleId == 0)
        return;
    if (characterTitleId == titles->currentTitleId())
        tibia::game::requestSetCharacterTitle(0);
    else
        tibia::game::requestSetCharacterTitle(static_cast<uint8_t>(characterTitleId));
}

/// CIP .
void TCharacterInfoDialogController::showContextMenuForCharacterTitleID(const uint CharacerTitleId)
{
    const auto* titles = static_cast<cip::tibia::gamewindow::TCharacterTitlesInfoQmlData*>(m_titlesInfo);
    TContextMenuController* menu = TContextMenuController::instance();
    if (!titles || !menu)
        return;
    menu->beginCustomMenu();
    if (CharacerTitleId == titles->currentTitleId()) {
        connect(menu->addCustomEntry(qtTrId("charinfo_character_title_context_rest_title")), &TContextMenuEntryItem::triggered, this,
                [] { tibia::game::requestSetCharacterTitle(0); });
    } else {
        connect(menu->addCustomEntry(qtTrId("charinfo_character_title_context_set_title")), &TContextMenuEntryItem::triggered, this,
                [CharacerTitleId] { tibia::game::requestSetCharacterTitle(static_cast<uint8_t>(CharacerTitleId)); });
    }
    menu->showCustomMenu();
}

}
