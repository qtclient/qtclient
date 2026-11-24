#include "bosstiarydialogcontroller.h"
#include "game/currentgamesession.h"
#include "protocol/clientmessages.h"

#include "cyclopedia/bestiarystorage.h"
#include "cyclopedia/creaturetrackerstorage.h"
#include "game/gamesessionbase.h"
#include "gamewindow/messagedialogcontroller.h"

#include <QKeySequence>

#include <tibia/assets/things.h>

#include <QLocale>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

/// CIP : eight bosses a page.
constexpr int BossesPerPage = 8;

int pageCount(const int bosses) { return std::max(1, (bosses - 1) / BossesPerPage + 1); }

}

TBosstiaryBossModel::TBosstiaryBossModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TBosstiaryBossModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TBosstiaryBossModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const bossbestiary::TBosstiaryEntry& entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case BossRaceId: return entry.raceId;
        case Name: return entry.name;
        case Kills: return entry.kills;
        case KillsForProwess: return m_storage ? m_storage->killsForStage(entry.category, 1) : 0;
        case KillsForExpertise: return m_storage ? m_storage->killsForStage(entry.category, 2) : 0;
        case KillsForMastery: return m_storage ? m_storage->killsForStage(entry.category, 3) : 0;
        case BossRaritySource: return bossRaritySource(entry.category);
        case BossRarityTooltip: return m_storage ? m_storage->rarityTooltip(entry.category) : QString();
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

void TBosstiaryBossModel::setEntries(std::vector<bossbestiary::TBosstiaryEntry> entries)
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

TBosstiaryDialogController::TBosstiaryDialogController(QObject* parent)
    : cip::tibia::gamewindow::TBosstiaryDialogController(parent)
    , m_message(new TMessageDialogController(this))
    , m_model(new TBosstiaryBossModel(this))
{
    m_bosses = m_model;
    m_currentPage = 1;
    m_message->setCloseOnButtonClicked(true);
}

TBosstiaryDialogController::~TBosstiaryDialogController() = default;

void TBosstiaryDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_storage = session ? session->bosstiaryStorage() : nullptr;
    m_pCreatureTrackerStorage = session ? session->creatureTrackerStorage() : nullptr;
    m_model->setStorage(m_storage.get());
    if (m_storage) {
        m_sessionConnections.push_back(connect(m_storage.get(), &bossbestiary::TBosstiaryStorage::bosstiaryEntriesChanged, this,
                                               &TBosstiaryDialogController::onListChanged));
        m_sessionConnections.push_back(connect(m_storage.get(), &bossbestiary::TBosstiaryStorage::unlockKillRequirementsChanged, this,
                                               &TBosstiaryDialogController::onKillStagesChanged));
        m_sessionConnections.push_back(connect(m_storage.get(), &bossbestiary::TBosstiaryStorage::bossPointsGrantedChanged, this,
                                               &TBosstiaryDialogController::onPointsChanged));
    }
    if (m_pCreatureTrackerStorage)
        m_sessionConnections.push_back(connect(m_pCreatureTrackerStorage.get(), &cyclopedia::TCreatureTrackerStorage::canTrackMoreBossesChanged,
                                               this, &TBosstiaryDialogController::canTrackMoreBossesChanged));
    Q_EMIT canTrackMoreBossesChanged();
    onKillStagesChanged();
    onPointsChanged();
    // The page is rebuilt where it was.
    if (m_storage)
        onListChanged();
}

bossbestiary::TBosstiaryFilter TBosstiaryDialogController::filter() const
{
    return m_storage ? m_storage->filter() : bossbestiary::TBosstiaryFilter{};
}

TCyclopediaDialogContext TBosstiaryDialogController::captureContext() const
{
    return TBosstiaryCyclopediaContext{ filter(), { static_cast<quint16>(m_currentPage), static_cast<quint8>(BossesPerPage) } };
}

void TBosstiaryDialogController::onListChanged()
{
    if (m_restoring)
        return;
    const TCyclopediaDialogContext context = captureContext();
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
    bossbestiary::TBosstiaryFilter filter;
    filter.search = QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(static_cast<uint32_t>(raceId)).name);
    if (m_storage)
        m_storage->setFilter(filter);
}

void TBosstiaryDialogController::handleGameAction(const input::IGameAction& action)
{
    if (action.gameActionType() != input::EGameAction::SHOW_BOSSTIARY_FREEPODIUM_DIALOG)
        return;
    const auto* podium = input::gameActionAs<input::gameactions::TGameActionShowBosstiaryFreePodiumDialog>(&action);
    if (!podium)
        return;
    m_message->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                     QKeySequence(QStringLiteral("Return"))) });
    m_message->showMessage(qtTrId("bosstiary_free_podium_received"), podium->text());
}

QString TBosstiaryDialogController::baneTooltip() const { return m_storage ? m_storage->rarityTooltip(0) : QString(); }
QString TBosstiaryDialogController::archfoeTooltip() const { return m_storage ? m_storage->rarityTooltip(1) : QString(); }
QString TBosstiaryDialogController::nemesisTooltip() const { return m_storage ? m_storage->rarityTooltip(2) : QString(); }
bool TBosstiaryDialogController::canTrackMoreBosses() const
{
    return m_pCreatureTrackerStorage && m_pCreatureTrackerStorage->canTrackMoreBosses();
}

int TBosstiaryDialogController::numberOfPages() const { return pageCount(m_storage ? m_storage->filteredEntryCount() : 0); }

QString TBosstiaryDialogController::searchString() const { return filter().search; }
bool TBosstiaryDialogController::showBaneBosses() const { return filter().showBane; }
bool TBosstiaryDialogController::showArchfoeBosses() const { return filter().showArchfoe; }
bool TBosstiaryDialogController::showNemesisBosses() const { return filter().showNemesis; }
bool TBosstiaryDialogController::showNoKillBosses() const { return filter().showNoKill; }
bool TBosstiaryDialogController::showFewKillsBosses() const { return filter().showFewKills; }
bool TBosstiaryDialogController::showProwessBosses() const { return filter().showProwess; }
bool TBosstiaryDialogController::showExpertiseBosses() const { return filter().showExpertise; }
bool TBosstiaryDialogController::showMasteryBosses() const { return filter().showMastery; }

/// The storage's filter with one field changed.
void TBosstiaryDialogController::changeFilter(const std::function<void(bossbestiary::TBosstiaryFilter&)>& change)
{
    if (!m_storage)
        return;
    bossbestiary::TBosstiaryFilter filter = m_storage->filter();
    change(filter);
    m_storage->setFilter(filter);
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
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession();
        session && session->canPerformGameAction())
        tibia::protocol::gameclient::sendRequestBosstiaryInfo();
}

/// Takes the filter and page back without the list change rebuilding first.
void TBosstiaryDialogController::restoreContext(const TCyclopediaDialogContext& CyclopediaContext)
{
    const auto& context = cyclopediaContextAs<TBosstiaryCyclopediaContext>(CyclopediaContext);
    m_restoring = true;
    if (m_storage)
        m_storage->setFilter(context.filter);
    m_currentPage = std::max(1, static_cast<int>(context.pagination.currentPage));
    rebuild();
    Q_EMIT pagesChanged();
    Q_EMIT filterChanged();
    m_restoring = false;
}

void TBosstiaryDialogController::hideDialogTab()
{
    // Page 1, empty model; the storage forgets the list and its filter.
    m_model->setEntries({});
    m_currentPage = 1;
    if (m_storage)
        m_storage->clearEntries();
    Q_EMIT filterChanged();
}

void TBosstiaryDialogController::rebuild()
{
    const std::vector<bossbestiary::TBosstiaryEntry> filtered =
        m_storage ? m_storage->filteredEntries() : std::vector<bossbestiary::TBosstiaryEntry>{};
    const int pages = pageCount(static_cast<int>(filtered.size()));
    if (m_currentPage > pages)
        m_currentPage = 1;
    const size_t first = static_cast<size_t>(m_currentPage - 1) * BossesPerPage;
    std::vector<bossbestiary::TBosstiaryEntry> page;
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
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession();
        session && session->canPerformGameAction())
        tibia::protocol::gameclient::sendStatusTrackerBestiary(static_cast<uint16_t>(RaceID), ShouldTrack);
}

void TBossSlotInformation::update(const bossbestiary::TBossSlot& slot, const bossbestiary::TBosstiaryStorage& storage)
{
    m_raceID = slot.raceId;
    m_name = slot.name;
    m_unlocked = slot.unlocked;
    m_numberOfKills = slot.hasBoss ? static_cast<int>(slot.kills) : 0;
    m_currentLootBonus = slot.hasBoss ? slot.lootBonus : 0;
    m_killBonus = slot.hasBoss ? slot.killBonus : 0;
    m_priceForReset = slot.hasBoss ? QLocale(QLocale::English).toString(static_cast<qlonglong>(slot.removePrice)) : QStringLiteral("0");
    m_resetPriceNumber = slot.hasBoss ? static_cast<int>(slot.removePrice) : 0;
    m_bossRaritySource = slot.hasBoss ? ::tibia::gamewindow::bossRaritySource(slot.category) : QString();
    m_bossRarityTooltip = slot.hasBoss ? storage.rarityTooltip(slot.category) : QString();
    m_killsForProwess = slot.hasBoss ? storage.killsForStage(slot.category, 1) : 0;
    m_killsForExpertise = slot.hasBoss ? storage.killsForStage(slot.category, 2) : 0;
    m_killsForMastery = slot.hasBoss ? storage.killsForStage(slot.category, 3) : 0;
    m_unlockBosspoints = slot.unlockBosspoints;
    m_isInBoostedSlot = slot.hasBoss && slot.inactive;
    Q_EMIT dataChanged();
}

TBossSlotsDialogController::TBossSlotsDialogController(QObject* parent)
    : cip::tibia::gamewindow::TBossSlotsDialogController(parent)
    , m_first(new TBossSlotInformation(this))
    , m_second(new TBossSlotInformation(this))
    , m_boosted(new TBossSlotInformation(this))
    , m_firstList(new TAssignableBossSortProxyModel(this))
    , m_secondList(new TAssignableBossSortProxyModel(this))
{
    m_firstSlot = m_first;
    m_secondSlot = m_second;
    m_boostedSlot = m_boosted;
    m_firstSlotBossList = m_firstList;
    m_secondSlotBossList = m_secondList;
    // The gold balance (CIP: the session's player data).
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        if (type == static_cast<int>(tibia::client::RESOURCE_BANK_BALANCE) || type == static_cast<int>(tibia::client::RESOURCE_GOLD_EQUIPPED))
            Q_EMIT goldBalanceChanged();
    });
}

TBossSlotsDialogController::~TBossSlotsDialogController() = default;

void TBossSlotsDialogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_storage = session ? session->bosstiaryStorage() : nullptr;
    m_firstList->source()->setStorage(m_storage.get());
    m_secondList->source()->setStorage(m_storage.get());
    if (m_storage) {
        // The slot data and the boss points per stage.
        m_sessionConnections.push_back(connect(m_storage.get(), &bossbestiary::TBosstiaryStorage::bossSlotsMessageData, this,
                                               &TBossSlotsDialogController::update));
        m_sessionConnections.push_back(connect(m_storage.get(), &bossbestiary::TBosstiaryStorage::bossPointsGrantedChanged, this,
                                               &TBossSlotsDialogController::onBossPointsChanged));
    }
    onBossPointsChanged();
    Q_EMIT goldBalanceChanged();
}

qlonglong TBossSlotsDialogController::playerGold() const
{
    return static_cast<qlonglong>(cyclopedia::TBestiaryStorage::goldBalance());
}

void TBossSlotsDialogController::showDialogTab()
{
    m_firstList->resetFilter();
    m_secondList->resetFilter();
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession();
        session && session->canPerformGameAction())
        tibia::protocol::gameclient::sendRequestBossSlootInfo();
}

void TBossSlotsDialogController::onClearClicked(const int SlotNumber)
{
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession();
        session && session->canPerformGameAction())
        tibia::protocol::gameclient::sendRequestBossSlotAction(SlotNumber != 1 ? 2 : 1, 0);
}

void TBossSlotsDialogController::onSelectClicked(const int SlotNumber, const int SelectedRaceID)
{
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession();
        session && session->canPerformGameAction())
        tibia::protocol::gameclient::sendRequestBossSlotAction(SlotNumber != 1 ? 2 : 1, static_cast<uint32_t>(SelectedRaceID));
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
    if (!m_storage)
        return;
    const bossbestiary::TBossSlotsData& slots = m_storage->slots();
    m_firstList->source()->setBosses(slots.assignable);
    m_secondList->source()->setBosses(slots.assignable);
    m_first->update(slots.first, *m_storage);
    m_second->update(slots.second, *m_storage);
    m_boosted->update(slots.boosted, *m_storage);
    m_maximumBosspoints = slots.maximumBosspoints;
    m_currentBosspoints = slots.currentBosspoints;
    m_currentLootBonus = slots.currentLootBonus;
    m_nextLootBonus = slots.nextLootBonus;
    Q_EMIT dataChanged();
    Q_EMIT goldBalanceChanged();
}

}
