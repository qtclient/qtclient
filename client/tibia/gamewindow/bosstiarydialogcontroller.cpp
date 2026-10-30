#include "bosstiarydialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"

#include <tibia/world/game.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP FUN_1007d6090: eight bosses a page.
constexpr int BossesPerPage = 8;

int pageCount(const int bosses) { return std::max(1, (bosses - 1) / BossesPerPage + 1); }

}

bool TBosstiaryDialogController::Context::operator==(const Context& other) const
{
    return search == other.search && std::equal(std::begin(filters), std::end(filters), std::begin(other.filters)) && page == other.page;
}

TBosstiaryBossModel::TBosstiaryBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : QAbstractListModel(parent)
    , m_storage(storage)
{}

int TBosstiaryBossModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TBosstiaryBossModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const cyclopedia::TBosstiaryEntry& entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case BossRaceId: return entry.raceId;
        case Name: return entry.name;
        case Kills: return entry.kills;
        case KillsForProwess: return m_storage.killsForStage(entry.category, 1);
        case KillsForExpertise: return m_storage.killsForStage(entry.category, 2);
        case KillsForMastery: return m_storage.killsForStage(entry.category, 3);
        case BossRaritySource: return cyclopedia::bossRaritySource(entry.category);
        case BossRarityTooltip: return m_storage.rarityTooltip(entry.category);
        case Tracked: return entry.tracked;
        default: return {};
    }
}

QHash<int, QByteArray> TBosstiaryBossModel::roleNames() const
{
    return {
        { BossRaceId, "bossRaceId" },
        { Name, "name" },
        { Kills, "kills" },
        { KillsForProwess, "killsForProwess" },
        { KillsForExpertise, "killsForExpertise" },
        { KillsForMastery, "killsForMastery" },
        { BossRaritySource, "bossRaritySource" },
        { BossRarityTooltip, "bossRarityTooltip" },
        { Tracked, "tracked" },
    };
}

void TBosstiaryBossModel::setEntries(std::vector<cyclopedia::TBosstiaryEntry> entries)
{
    const int oldCount = rowCount();
    const int newCount = static_cast<int>(entries.size());
    if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_entries = std::move(entries);
        endRemoveRows();
    } else if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_entries = std::move(entries);
        endInsertRows();
    } else {
        m_entries = std::move(entries);
    }
    refreshAll();
}

void TBosstiaryBossModel::refreshAll()
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1));
}

TBosstiaryDialogController::TBosstiaryDialogController(cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TBosstiaryDialogController(parent)
    , m_storage(storage)
    , m_model(new TBosstiaryBossModel(storage, this))
{
    m_bosses = m_model;
    m_currentPage = 1;
    m_numberOfPages = 1;
    applyContext({});
    // CIP 0x100dd1f50: the storage's list / static data / tracker signals.
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::entriesChanged, this, &TBosstiaryDialogController::rebuild);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::killStagesChanged, m_model, &TBosstiaryBossModel::refreshAll);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::pointsChanged, m_model, &TBosstiaryBossModel::refreshAll);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::trackerChanged, this,
            &TBosstiaryDialogController::canTrackMoreBossesChanged);
}

TBosstiaryDialogController::~TBosstiaryDialogController() = default;

TBosstiaryDialogController::Context TBosstiaryDialogController::captureContext() const
{
    Context context;
    context.search = m_searchString;
    const bool filters[8] = { m_showBaneBosses,     m_showArchfoeBosses,  m_showNemesisBosses,   m_showNoKillBosses,
                              m_showFewKillsBosses, m_showProwessBosses, m_showExpertiseBosses, m_showMasteryBosses };
    std::copy(std::begin(filters), std::end(filters), std::begin(context.filters));
    context.page = m_currentPage;
    return context;
}

void TBosstiaryDialogController::applyContext(const Context& context)
{
    m_searchString = context.search;
    m_showBaneBosses = context.filters[0];
    m_showArchfoeBosses = context.filters[1];
    m_showNemesisBosses = context.filters[2];
    m_showNoKillBosses = context.filters[3];
    m_showFewKillsBosses = context.filters[4];
    m_showProwessBosses = context.filters[5];
    m_showExpertiseBosses = context.filters[6];
    m_showMasteryBosses = context.filters[7];
    m_currentPage = std::max(1, context.page);
    Q_EMIT filterChanged();
}

void TBosstiaryDialogController::showRace(const int raceId)
{
    Context context;
    context.search = QString::fromStdString(g_things.getRaceData(static_cast<uint32_t>(raceId)).name);
    applyContext(context);
    rebuild();
}

QString TBosstiaryDialogController::baneTooltip() const { return m_storage.rarityTooltip(0); }
QString TBosstiaryDialogController::archfoeTooltip() const { return m_storage.rarityTooltip(1); }
QString TBosstiaryDialogController::nemesisTooltip() const { return m_storage.rarityTooltip(2); }
bool TBosstiaryDialogController::canTrackMoreBosses() const { return m_storage.canTrackMoreBosses(); }

#define BOSSTIARY_FILTER_SETTER(setter, member, type)                                                                  \
    void TBosstiaryDialogController::setter(type v)                                                                    \
    {                                                                                                                  \
        if (member == v)                                                                                               \
            return;                                                                                                    \
        cip::tibia::gamewindow::TBosstiaryDialogController::setter(v);                                                 \
        rebuild();                                                                                                     \
    }

BOSSTIARY_FILTER_SETTER(setSearchString, m_searchString, const QString&)
BOSSTIARY_FILTER_SETTER(setShowBaneBosses, m_showBaneBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowArchfoeBosses, m_showArchfoeBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowNemesisBosses, m_showNemesisBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowNoKillBosses, m_showNoKillBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowFewKillsBosses, m_showFewKillsBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowProwessBosses, m_showProwessBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowExpertiseBosses, m_showExpertiseBosses, bool)
BOSSTIARY_FILTER_SETTER(setShowMasteryBosses, m_showMasteryBosses, bool)

#undef BOSSTIARY_FILTER_SETTER

void TBosstiaryDialogController::showDialogTab()
{
    if (m_pendingContext) {
        applyContext(*m_pendingContext);
        m_pendingContext.reset();
    }
    rebuild();
    g_game.requestBosstiaryInfo();
}

void TBosstiaryDialogController::hideDialogTab()
{
    // CIP 0x100dd2e50: page 1, empty model, default filter, no list.
    m_model->setEntries({});
    applyContext({});
    m_storage.clearEntries();
}

bool TBosstiaryDialogController::accepts(const cyclopedia::TBosstiaryEntry& entry) const
{
    const qint64 kills = entry.kills;
    if (!m_searchString.isEmpty() && (kills == 0 || !entry.name.contains(m_searchString, Qt::CaseInsensitive)))
        return false;
    if ((!m_showBaneBosses && entry.category == 0) || (!m_showArchfoeBosses && entry.category == 1)
        || (!m_showNemesisBosses && entry.category == 2) || (!m_showNoKillBosses && kills == 0))
        return false;
    const int prowess = m_storage.killsForStage(entry.category, 1);
    const int expertise = m_storage.killsForStage(entry.category, 2);
    const int mastery = m_storage.killsForStage(entry.category, 3);
    if (!m_showFewKillsBosses && kills >= 1 && kills < prowess)
        return false;
    if (!m_showProwessBosses && kills >= prowess && kills < expertise)
        return false;
    if (!m_showExpertiseBosses && kills >= expertise && kills < mastery)
        return false;
    return m_showMasteryBosses || kills < mastery;
}

void TBosstiaryDialogController::rebuild()
{
    std::vector<cyclopedia::TBosstiaryEntry> filtered;
    for (const cyclopedia::TBosstiaryEntry& entry : m_storage.entries()) {
        if (accepts(entry))
            filtered.push_back(entry);
    }
    const int pages = pageCount(static_cast<int>(filtered.size()));
    if (m_currentPage > pages)
        m_currentPage = 1;
    m_numberOfPages = pages;
    const size_t first = static_cast<size_t>(m_currentPage - 1) * BossesPerPage;
    std::vector<cyclopedia::TBosstiaryEntry> page;
    for (size_t i = first; i < filtered.size() && i < first + BossesPerPage; ++i)
        page.push_back(filtered[i]);
    m_model->setEntries(std::move(page));
    Q_EMIT pagesChanged();
}

void TBosstiaryDialogController::requestPreviousPage()
{
    m_currentPage = std::max(1, m_currentPage - 1);
    rebuild();
}

void TBosstiaryDialogController::requestNextPage()
{
    m_currentPage = std::min(m_numberOfPages, m_currentPage + 1);
    rebuild();
}

void TBosstiaryDialogController::requestToggleBosstiaryTracking(const int RaceID, const bool ShouldTrack)
{
    g_game.sendStatusTrackerBestiary(static_cast<uint16_t>(RaceID), ShouldTrack);
}

void TBossSlotInformation::update(const cyclopedia::TBossSlot& slot, const cyclopedia::TBosstiaryStorage& storage)
{
    m_raceID = slot.raceId;
    m_name = slot.name;
    m_unlocked = slot.unlocked;
    m_numberOfKills = slot.hasBoss ? static_cast<int>(slot.kills) : 0;
    m_currentLootBonus = slot.hasBoss ? slot.lootBonus : 0;
    m_killBonus = slot.hasBoss ? slot.killBonus : 0;
    m_priceForReset = slot.hasBoss ? QLocale(QLocale::English).toString(static_cast<qlonglong>(slot.removePrice)) : QStringLiteral("0");
    m_resetPriceNumber = slot.hasBoss ? static_cast<int>(slot.removePrice) : 0;
    m_bossRaritySource = slot.hasBoss ? cyclopedia::bossRaritySource(slot.category) : QString();
    m_bossRarityTooltip = slot.hasBoss ? storage.rarityTooltip(slot.category) : QString();
    m_killsForProwess = slot.hasBoss ? storage.killsForStage(slot.category, 1) : 0;
    m_killsForExpertise = slot.hasBoss ? storage.killsForStage(slot.category, 2) : 0;
    m_killsForMastery = slot.hasBoss ? storage.killsForStage(slot.category, 3) : 0;
    m_unlockBosspoints = slot.unlockBosspoints;
    m_isInBoostedSlot = slot.hasBoss && slot.inactive;
    Q_EMIT dataChanged();
}

TAssignableBossModel::TAssignableBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : QAbstractListModel(parent)
    , m_storage(storage)
{}

int TAssignableBossModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_bosses.size());
}

QVariant TAssignableBossModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const cyclopedia::TAssignableBoss& boss = m_bosses[static_cast<size_t>(index.row())];
    switch (role) {
        case BossRaceId: return boss.raceId;
        case Name: return boss.name;
        case BossRaritySource: return cyclopedia::bossRaritySource(boss.category);
        case BossRarityTooltip: return m_storage.rarityTooltip(boss.category);
        default: return {};
    }
}

QHash<int, QByteArray> TAssignableBossModel::roleNames() const
{
    return {
        { BossRaceId, "bossRaceId" },
        { Name, "name" },
        { BossRaritySource, "bossRaritySource" },
        { BossRarityTooltip, "bossRarityTooltip" },
    };
}

void TAssignableBossModel::setBosses(const std::vector<cyclopedia::TAssignableBoss>& bosses)
{
    beginResetModel();
    m_bosses = bosses;
    endResetModel();
}

void TAssignableBossModel::refreshAll()
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1));
}

TAssignableBossSortProxyModel::TAssignableBossSortProxyModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::TAssignableBossSortProxyModel(parent)
    , m_source(new TAssignableBossModel(storage, this))
{
    m_raceIDRoleEnumValue = TAssignableBossModel::BossRaceId;
    setSourceModel(m_source);
    sort(0, Qt::AscendingOrder);
}

void TAssignableBossSortProxyModel::setNameFilter(const QString& v)
{
    if (v == m_nameFilter)
        return;
    beginFilterChange();
    m_nameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

bool TAssignableBossSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (m_nameFilter.isEmpty())
        return true;
    const QString name = m_source->data(m_source->index(sourceRow), TAssignableBossModel::Name).toString();
    return name.contains(m_nameFilter, Qt::CaseInsensitive);
}

bool TAssignableBossSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString a = m_source->data(left, TAssignableBossModel::Name).toString();
    const QString b = m_source->data(right, TAssignableBossModel::Name).toString();
    return QString::compare(a, b, Qt::CaseSensitive) < 0;
}

TBossSlotsDialogController::TBossSlotsDialogController(cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TBossSlotsDialogController(parent)
    , m_storage(storage)
    , m_first(new TBossSlotInformation(this))
    , m_second(new TBossSlotInformation(this))
    , m_boosted(new TBossSlotInformation(this))
    , m_firstList(new TAssignableBossSortProxyModel(storage, this))
    , m_secondList(new TAssignableBossSortProxyModel(storage, this))
{
    m_firstSlot = m_first;
    m_secondSlot = m_second;
    m_boostedSlot = m_boosted;
    m_firstSlotBossList = m_firstList;
    m_secondSlotBossList = m_secondList;
    // CIP 0x100dd74b0: slot data, boss points per stage and the gold balance.
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::slotsChanged, this, &TBossSlotsDialogController::update);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::pointsChanged, this, &TBossSlotsDialogController::update);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::killStagesChanged, this, &TBossSlotsDialogController::update);
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type && (*type == Otc::RESOURCE_BANK_BALANCE || *type == Otc::RESOURCE_GOLD_EQUIPPED))
            Q_EMIT goldBalanceChanged();
        return std::any{};
    });
    update();
}

TBossSlotsDialogController::~TBossSlotsDialogController() = default;

qlonglong TBossSlotsDialogController::playerGold() const
{
    return static_cast<qlonglong>(cyclopedia::TBestiaryStorage::goldBalance());
}

void TBossSlotsDialogController::showDialogTab()
{
    m_firstList->resetFilter();
    m_secondList->resetFilter();
    g_game.requestBossSlootInfo();
}

void TBossSlotsDialogController::onClearClicked(const int SlotNumber)
{
    g_game.requestBossSlotAction(SlotNumber != 1 ? 2 : 1, 0);
}

void TBossSlotsDialogController::onSelectClicked(const int SlotNumber, const int SelectedRaceID)
{
    g_game.requestBossSlotAction(SlotNumber != 1 ? 2 : 1, static_cast<uint32_t>(SelectedRaceID));
    (SlotNumber != 1 ? m_secondList : m_firstList)->resetFilter();
}

void TBossSlotsDialogController::update()
{
    const cyclopedia::TBossSlotsData& slots = m_storage.slots();
    m_firstList->source()->setBosses(slots.assignable);
    m_secondList->source()->setBosses(slots.assignable);
    m_first->update(slots.first, m_storage);
    m_second->update(slots.second, m_storage);
    m_boosted->update(slots.boosted, m_storage);
    m_maximumBosspoints = slots.maximumBosspoints;
    m_currentBosspoints = slots.currentBosspoints;
    m_currentLootBonus = slots.currentLootBonus;
    m_nextLootBonus = slots.nextLootBonus;
    Q_EMIT dataChanged();
    Q_EMIT goldBalanceChanged();
}

}
