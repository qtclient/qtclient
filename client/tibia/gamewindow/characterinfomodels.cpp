#include "characterinfomodels.h"

#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/contextmenucontroller.h"

#include <tibia/world/game.h>
#include <client/outfit.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QDateTime>

#include <algorithm>
#include <tuple>

namespace tibia::gamewindow {

namespace {

QString appearanceName(const uint16_t typeId)
{
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    return type ? QString::fromStdString(type->getName()) : QString();
}

/// CIP 0x100a546f0: adds one wire list to the rows, the amount going to
/// `amount` (row +0x24 inventory, +0x28 depot, +0x2c stash, +0x30 inbox,
/// +0x34 store inbox).
void mergeList(std::vector<ItemSummaryEntry>& rows, const std::vector<ItemSummary>& list, uint32_t ItemSummaryEntry::* amount)
{
    const cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
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
        row.lootValueLevel = prices ? prices->lootValueLevel(item.itemId) : 0;
        rows.insert(it, std::move(row));
    }
}

/// CIP FUN_100927c50 / FUN_100927d10.
QString lootValueImage(const int level, const int style)
{
    const cyclopedia::TItemInfoStorage* prices = cyclopedia::TItemInfoStorage::instance();
    if (level == 0 || !prices || prices->colorizeLootStyle() != style)
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
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
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
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
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
    return QString::compare(left.data(R::Name).toString(), right.data(R::Name).toString(), Qt::CaseSensitive) < 0;
}

TCharacterTitlesInfoQmlData::TCharacterTitlesInfoQmlData(const uint8_t currentTitleId, std::vector<TitleEntry> entries, QObject* parent)
    : cip::tibia::gamewindow::TCharacterTitlesInfoQmlData(parent)
{
    m_currentTitle = qtTrId("charinfo_character_title_no_title");
    m_isCurrentTitleActive = true;
    const auto current =
        std::find_if(entries.begin(), entries.end(), [currentTitleId](const TitleEntry& entry) { return entry.id == currentTitleId; });
    if (current != entries.end()) {
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
    const Color c = Outfit::getColor(m_colors[(m_typeFilter == Mounts ? 4 : 0) + part]);
    return QColor(c.r(), c.g(), c.b());
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
        // CIP 0x100927820: image://friendbadge-icons/<id-1>.
        case BadgeIcon: return entry.id == 0 ? QString()
                                            : QStringLiteral("image://friendbadge-icons/%1").arg(entry.id - 1);
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
    // CIP 0x10095cc80.
    m_unlockDateString = entry.timestamp == 0
                             ? QStringLiteral("")
                             : QDateTime::fromSecsSinceEpoch(entry.timestamp).toString(QStringLiteral("yyyy-MM-dd"));
}

namespace {

/// CIP 0x100aaee40: the static achievements (staticdata).
const std::vector<CyclopediaAchievementType>& staticAchievements() { return g_things.getCyclopediaAchievements(); }

/// CIP 0x100aaee50.
const CyclopediaAchievementType* staticAchievement(const uint16_t id)
{
    const auto& all = staticAchievements();
    const auto it = std::find_if(all.begin(), all.end(), [id](const CyclopediaAchievementType& a) { return a.id == id; });
    return it != all.end() ? &*it : nullptr;
}

}

void TAchievementInfoController::setData(const int points, const int totalSecret, std::vector<AchievementEntry> unlocked)
{
    for (AchievementEntry& entry : unlocked) {
        if (entry.secret)
            continue;
        // CIP 0x100a8da90: a regular one takes its texts and grade from the static data.
        if (const CyclopediaAchievementType* known = staticAchievement(entry.id)) {
            entry.name = QString::fromStdString(known->name);
            entry.description = QString::fromStdString(known->description);
            entry.grade = known->grade;
        }
    }
    m_unlocked = std::move(unlocked);
    m_achievementPointsSum = points;
    m_totalSecretAchievements = totalSecret;
    updateCounts();
    rebuildList();
    Q_EMIT achievementsChanged();
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
        for (const CyclopediaAchievementType& known : staticAchievements()) {
            const bool unlocked =
                std::any_of(m_unlocked.begin(), m_unlocked.end(), [&known](const AchievementEntry& entry) { return entry.id == known.id; });
            if (!unlocked)
                rows.push_back({ known.id, 0, false, QString::fromStdString(known.name), QString::fromStdString(known.description), known.grade });
        }
    }

    // CIP 0x100d1a5d0 (name), 0x100d1fc40 (newest first), 0x100d23330 (highest grade, then name).
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

/// CIP 0x100a90860: the wire's 1 is a quest, 2 a store appearance.
int appearanceSource(const uint8_t type) { return type == 1 ? 1 : type == 2 ? 2 : 0; }

}

void TCharacterInfoDialogController::hookModelEvents()
{
    // CIP 0x100d13520: a new proxy (inventory only) per answer, sorted.
    m_hooks.emplace_back("g_game", "onUpdateCyclopediaCharacterItemSummary", [this](LuaObject*, const Args& args) {
        if (!m_shown)
            return std::any{};
        if (const auto* data = args.empty() ? nullptr : std::any_cast<CyclopediaCharacterItemSummary>(&args[0]))
            replaceObject(m_itemSummary, new TItemSummaryModelSortFilterProxy(mergeItemSummary(*data), this));
        received(ItemSummary);
        return std::any{};
    });

    // CIP 0x100d136f0: a new proxy (outfits, all sources) per answer.
    m_hooks.emplace_back("g_game", "onParseCyclopediaCharacterAppearances", [this](LuaObject*, const Args& args) {
        if (!m_shown || args.size() < 4)
            return std::any{};
        const auto* colors = std::any_cast<OutfitColorStruct>(&args[0]);
        const auto* outfits = std::any_cast<std::vector<CharacterInfoOutfits>>(&args[1]);
        const auto* mounts = std::any_cast<std::vector<CharacterInfoMounts>>(&args[2]);
        const auto* familiars = std::any_cast<std::vector<CharacterInfoFamiliar>>(&args[3]);
        if (!colors || !outfits || !mounts || !familiars)
            return std::any{};
        std::vector<AppearanceEntry> entries;
        entries.reserve(outfits->size() + mounts->size() + familiars->size());
        for (const CharacterInfoOutfits& o : *outfits)
            entries.push_back({ AppearanceEntry::Outfit, o.lookType, QString::fromStdString(o.name), appearanceSource(o.type),
                                static_cast<int>(o.isCurrent), o.addons });
        for (const CharacterInfoMounts& m : *mounts)
            entries.push_back(
                { AppearanceEntry::Mount, m.mountId, QString::fromStdString(m.name), appearanceSource(m.type), static_cast<int>(m.isCurrent), 0 });
        for (const CharacterInfoFamiliar& f : *familiars)
            entries.push_back({ AppearanceEntry::SummonSkin, f.lookType, QString::fromStdString(f.name), appearanceSource(f.type),
                                static_cast<int>(f.isCurrent), 0 });
        const TOutfitMountSummonSkinsListDataModelSortFilterProxy::Colors indexes{ colors->lookHead,      colors->lookBody,
                                                                                   colors->lookLegs,      colors->lookFeet,
                                                                                   colors->lookMountHead, colors->lookMountBody,
                                                                                   colors->lookMountLegs, colors->lookMountFeet };
        replaceObject(m_outfitMountModel, new TOutfitMountSummonSkinsListDataModelSortFilterProxy(std::move(entries), indexes, this));
        received(OutfitsMounts);
        return std::any{};
    });

    // CIP 0x100d14150.
    m_hooks.emplace_back("g_game", "onCyclopediaCharacterTitles", [this](LuaObject*, const Args& args) {
        using Wire = std::vector<std::tuple<uint8_t, std::string, std::string, bool, bool>>;
        if (!m_shown || args.size() < 2)
            return std::any{};
        const int* current = std::any_cast<int>(&args[0]);
        const auto* titles = std::any_cast<Wire>(&args[1]);
        if (!current || !titles)
            return std::any{};
        std::vector<TitleEntry> entries;
        entries.reserve(titles->size());
        for (const auto& [id, name, description, permanent, unlocked] : *titles)
            entries.push_back({ id, QString::fromStdString(name), QString::fromStdString(description), permanent, unlocked });
        replaceObject(m_titlesInfo, new TCharacterTitlesInfoQmlData(static_cast<uint8_t>(*current), std::move(entries), this));
        received(Titles);
        return std::any{};
    });

    m_hooks.emplace_back("g_game", "onCyclopediaCharacterAchievements", [this](LuaObject*, const Args& args) {
        using Wire = std::vector<std::tuple<uint16_t, uint32_t, bool, std::string, std::string, uint8_t>>;
        if (!m_shown || args.size() < 3)
            return std::any{};
        const int* points = std::any_cast<int>(&args[0]);
        const int* totalSecret = std::any_cast<int>(&args[1]);
        const auto* achievements = std::any_cast<Wire>(&args[2]);
        if (!points || !totalSecret || !achievements)
            return std::any{};
        std::vector<AchievementEntry> unlocked;
        unlocked.reserve(achievements->size());
        for (const auto& [id, timestamp, secret, name, description, grade] : *achievements)
            unlocked.push_back({ id, timestamp, secret, QString::fromStdString(name), QString::fromStdString(description), grade });
        if (auto* controller = dynamic_cast<TAchievementInfoController*>(m_achievementsController))
            controller->setData(*points, *totalSecret, std::move(unlocked));
        received(Achievements);
        return std::any{};
    });
}

/// CIP 0x100d108d0.
void TCharacterInfoDialogController::resetCharacterTitle() { g_game.requestSetCharacterTitle(0); }

void TCharacterInfoDialogController::setOrToggleCharacterTitle(const uint characterTitleId)
{
    const auto* titles = static_cast<cip::tibia::gamewindow::TCharacterTitlesInfoQmlData*>(m_titlesInfo);
    if (!titles || characterTitleId == 0)
        return;
    if (characterTitleId == titles->currentTitleId())
        g_game.requestSetCharacterTitle(0);
    else
        g_game.requestSetCharacterTitle(static_cast<uint8_t>(characterTitleId));
}

/// CIP 0x100d10950.
void TCharacterInfoDialogController::showContextMenuForCharacterTitleID(const uint CharacerTitleId)
{
    const auto* titles = static_cast<cip::tibia::gamewindow::TCharacterTitlesInfoQmlData*>(m_titlesInfo);
    TContextMenuController* menu = TContextMenuController::instance();
    if (!titles || !menu)
        return;
    menu->beginCustomMenu();
    if (CharacerTitleId == titles->currentTitleId()) {
        connect(menu->addCustomEntry(qtTrId("charinfo_character_title_context_rest_title")), &TContextMenuEntryItem::triggered, this,
                [] { g_game.requestSetCharacterTitle(0); });
    } else {
        connect(menu->addCustomEntry(qtTrId("charinfo_character_title_context_set_title")), &TContextMenuEntryItem::triggered, this,
                [CharacerTitleId] { g_game.requestSetCharacterTitle(static_cast<uint8_t>(CharacerTitleId)); });
    }
    menu->showCustomMenu();
}

}
