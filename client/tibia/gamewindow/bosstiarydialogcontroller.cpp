#include "bosstiarydialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/messagedialogcontroller.h"

#include <QKeySequence>

#include <tibia/game/gamesession.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// CIP : eight bosses a page.
constexpr int BossesPerPage = 8;

int pageCount(const int bosses) { return std::max(1, (bosses - 1) / BossesPerPage + 1); }

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

void TBosstiaryBossModel::refreshRoles(const QList<int>& roles)
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), roles);
}

TBosstiaryDialogController::TBosstiaryDialogController(cyclopedia::TBosstiaryStorage& storage, QObject* parent)
    : cip::tibia::gamewindow::TBosstiaryDialogController(parent)
    , m_message(new TMessageDialogController(this))
    , m_storage(storage)
    , m_model(new TBosstiaryBossModel(storage, this))
{
    m_bosses = m_model;
    m_currentPage = 1;
    m_message->setCloseOnButtonClicked(true);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::filterChanged, this, &TBosstiaryDialogController::onListChanged);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::entriesChanged, this, &TBosstiaryDialogController::onListChanged);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::killStagesChanged, this, &TBosstiaryDialogController::onKillStagesChanged);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::pointsChanged, this, &TBosstiaryDialogController::onPointsChanged);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::trackerChanged, this,
            &TBosstiaryDialogController::canTrackMoreBossesChanged);
    onKillStagesChanged();
    onPointsChanged();
    onListChanged();
}

TBosstiaryDialogController::~TBosstiaryDialogController() = default;

TBosstiaryDialogController::Context TBosstiaryDialogController::captureContext() const
{
    return { m_storage.filter(), m_currentPage };
}

void TBosstiaryDialogController::onListChanged()
{
    if (m_restoring)
        return;
    const Context context = captureContext();
    rebuild();
    restoreContext(context);
}

void TBosstiaryDialogController::onKillStagesChanged()
{
    m_model->refreshRoles({ TBosstiaryBossModel::KillsForProwess, TBosstiaryBossModel::KillsForExpertise,
                            TBosstiaryBossModel::KillsForMastery });
}

void TBosstiaryDialogController::onPointsChanged() { m_model->refreshRoles({ TBosstiaryBossModel::BossRarityTooltip }); }

void TBosstiaryDialogController::showRace(const int raceId)
{
    cyclopedia::TBosstiaryFilter filter;
    filter.search = QString::fromStdString(tibia::game::raceData(static_cast<uint32_t>(raceId)).name);
    m_storage.setFilter(filter);
}

void TBosstiaryDialogController::handleGameAction(const input::IGameAction& action)
{
    const auto* podium = input::gameActionAs<input::gameactions::TGameActionShowBosstiaryFreePodiumDialog>(&action);
    if (!podium)
        return;
    m_message->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                     QKeySequence(QStringLiteral("Return"))) });
    m_message->showMessage(qtTrId("bosstiary_free_podium_received"), podium->text());
}

QString TBosstiaryDialogController::baneTooltip() const { return m_storage.rarityTooltip(0); }
QString TBosstiaryDialogController::archfoeTooltip() const { return m_storage.rarityTooltip(1); }
QString TBosstiaryDialogController::nemesisTooltip() const { return m_storage.rarityTooltip(2); }
bool TBosstiaryDialogController::canTrackMoreBosses() const { return m_storage.canTrackMoreBosses(); }

int TBosstiaryDialogController::numberOfPages() const { return pageCount(m_storage.filteredEntryCount()); }

QString TBosstiaryDialogController::searchString() const { return m_storage.filter().search; }
bool TBosstiaryDialogController::showBaneBosses() const { return m_storage.filter().showBane; }
bool TBosstiaryDialogController::showArchfoeBosses() const { return m_storage.filter().showArchfoe; }
bool TBosstiaryDialogController::showNemesisBosses() const { return m_storage.filter().showNemesis; }
bool TBosstiaryDialogController::showNoKillBosses() const { return m_storage.filter().showNoKill; }
bool TBosstiaryDialogController::showFewKillsBosses() const { return m_storage.filter().showFewKills; }
bool TBosstiaryDialogController::showProwessBosses() const { return m_storage.filter().showProwess; }
bool TBosstiaryDialogController::showExpertiseBosses() const { return m_storage.filter().showExpertise; }
bool TBosstiaryDialogController::showMasteryBosses() const { return m_storage.filter().showMastery; }

/// The storage's filter with one field changed.
void TBosstiaryDialogController::changeFilter(const std::function<void(cyclopedia::TBosstiaryFilter&)>& change)
{
    cyclopedia::TBosstiaryFilter filter = m_storage.filter();
    change(filter);
    m_storage.setFilter(filter);
}

void TBosstiaryDialogController::setSearchString(const QString& v) { changeFilter([&](auto& f) { f.search = v; }); }
void TBosstiaryDialogController::setShowBaneBosses(const bool v) { changeFilter([&](auto& f) { f.showBane = v; }); }
void TBosstiaryDialogController::setShowArchfoeBosses(const bool v) { changeFilter([&](auto& f) { f.showArchfoe = v; }); }
void TBosstiaryDialogController::setShowNemesisBosses(const bool v) { changeFilter([&](auto& f) { f.showNemesis = v; }); }
void TBosstiaryDialogController::setShowNoKillBosses(const bool v) { changeFilter([&](auto& f) { f.showNoKill = v; }); }
void TBosstiaryDialogController::setShowFewKillsBosses(const bool v) { changeFilter([&](auto& f) { f.showFewKills = v; }); }
void TBosstiaryDialogController::setShowProwessBosses(const bool v) { changeFilter([&](auto& f) { f.showProwess = v; }); }
void TBosstiaryDialogController::setShowExpertiseBosses(const bool v) { changeFilter([&](auto& f) { f.showExpertise = v; }); }
void TBosstiaryDialogController::setShowMasteryBosses(const bool v) { changeFilter([&](auto& f) { f.showMastery = v; }); }

/// Asks the server for the bosstiary; its answer rebuilds the page.
void TBosstiaryDialogController::showDialogTab()
{
    tibia::game::requestBosstiaryInfo();
}

/// Takes the filter and page back without the list change rebuilding first.
void TBosstiaryDialogController::restoreContext(const Context& context)
{
    m_restoring = true;
    m_storage.setFilter(context.filter);
    m_currentPage = std::max(1, context.page);
    rebuild();
    Q_EMIT pagesChanged();
    Q_EMIT filterChanged();
    m_restoring = false;
}

void TBosstiaryDialogController::hideDialogTab()
{
    // Page 1, empty model, default filter, no list.
    m_model->setEntries({});
    m_restoring = true;
    m_storage.setFilter({});
    m_currentPage = 1;
    m_storage.clearEntries();
    m_restoring = false;
    Q_EMIT filterChanged();
}

void TBosstiaryDialogController::rebuild()
{
    const std::vector<cyclopedia::TBosstiaryEntry> filtered = m_storage.filteredEntries();
    const int pages = pageCount(static_cast<int>(filtered.size()));
    if (m_currentPage > pages)
        m_currentPage = 1;
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

/// Past the last page the rebuild goes back to page 1.
void TBosstiaryDialogController::requestNextPage()
{
    m_currentPage = std::max(1, m_currentPage + 1);
    rebuild();
}

void TBosstiaryDialogController::requestToggleBosstiaryTracking(const int RaceID, const bool ShouldTrack)
{
    tibia::game::sendStatusTrackerBestiary(static_cast<uint16_t>(RaceID), ShouldTrack);
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
    // CIP : slot data, boss points per stage and the gold balance.
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::slotsChanged, this, &TBossSlotsDialogController::update);
    connect(&m_storage, &cyclopedia::TBosstiaryStorage::pointsChanged, this, &TBossSlotsDialogController::onBossPointsChanged);
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        if (type == static_cast<int>(tibia::client::RESOURCE_BANK_BALANCE) || type == static_cast<int>(tibia::client::RESOURCE_GOLD_EQUIPPED))
            Q_EMIT goldBalanceChanged();
    });
    update();
    onBossPointsChanged();
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
    tibia::game::requestBossSlootInfo();
}

void TBossSlotsDialogController::onClearClicked(const int SlotNumber)
{
    tibia::game::requestBossSlotAction(SlotNumber != 1 ? 2 : 1, 0);
}

void TBossSlotsDialogController::onSelectClicked(const int SlotNumber, const int SelectedRaceID)
{
    tibia::game::requestBossSlotAction(SlotNumber != 1 ? 2 : 1, static_cast<uint32_t>(SelectedRaceID));
    (SlotNumber != 1 ? m_secondList : m_firstList)->resetFilter();
}

void TBossSlotsDialogController::onBossPointsChanged()
{
    // Only the boss lists' rarity tooltips follow the new points; the slots
    // keep theirs until their next update.
    m_firstList->source()->refreshRarityTooltips();
    m_secondList->source()->refreshRarityTooltips();
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
