#include "characterinfostorage.h"

#include "cyclopedia/generalplayerstatisticsprovider.h"
#include "cyclopedia/playerdefencestatsprovider.h"
#include "cyclopedia/playermiscstatsprovider.h"
#include "cyclopedia/playeroffencestatsprovider.h"

#include <QCoreApplication>

#include <protocol.pb.h>

#include <stdexcept>

namespace tibia::cyclopedia {

namespace {

QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }

/// The name of a hireling job (empty for an unknown job).
QString hirelingJobName(const uint32_t job)
{
    switch (job) {
        case 0: return qtTrId("charinfo_hireling_job_banker");
        case 1: return qtTrId("charinfo_hireling_job_trader");
        case 2: return qtTrId("charinfo_hireling_job_cook");
        case 3: return qtTrId("charinfo_hireling_job_steward");
        default: return QString::fromUtf8("");
    }
}

TOutfitsMountsSummonSkins::ESource appearanceSource(const uint32_t source)
{
    if (source > TOutfitsMountsSummonSkins::Store)
        throw std::runtime_error("unhandled enum value");
    return static_cast<TOutfitsMountsSummonSkins::ESource>(source);
}

TOutfitsMountsSummonSkins::TColors colors(const protobuf::protocol::ProtocolCharacterInfoColors& message)
{
    return { static_cast<uint8_t>(message.head()), static_cast<uint8_t>(message.body()), static_cast<uint8_t>(message.legs()),
             static_cast<uint8_t>(message.feet()) };
}

}

TAccountInfos::TAccountInfos(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
    : canShowAccountInfos(message.show_account())
    , online(message.online())
    , premium(message.premium())
    , loyaltyTitle(text(message.loyalty_title()))
{
    for (const auto& badge : message.badges())
        badges.emplace_back(badge.id(), text(badge.name()), QString(), true, true);
}

TStoreSummary::TStoreSummary(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
    : storeXpBoostTime(message.xp_boost_time())
    , dailyRewardXpBoostTime(message.daily_xp_boost_time())
    , preySlots(static_cast<uint8_t>(message.prey_slots()))
    , preyWildcards(static_cast<uint8_t>(message.prey_wildcards()))
    , permanentWeeklyTaskExpansion(message.weekly_task_expansion())
    , instantRewardAccesses(static_cast<uint8_t>(message.instant_rewards()))
    , hirelings(static_cast<uint8_t>(message.hirelings()))
    , charmExpansion(message.charm_expansion())
{
    for (const auto& outfit : message.hireling_outfits())
        hirelingOutfits.push_back({ text(outfit.name()), static_cast<int>(outfit.count()) });
    for (const auto& blessing : message.blessings())
        blessings.push_back({ text(blessing.name()), static_cast<int>(blessing.count()) });
    for (const auto& item : message.house_items())
        houseItems.push_back({ static_cast<uint16_t>(item.object_id()), text(item.name()), static_cast<int>(item.count()) });
    for (const uint32_t job : message.hireling_jobs())
        hirelingJobs.push_back(hirelingJobName(job));
}

TOutfitsMountsSummonSkins::TOutfitsMountsSummonSkins(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
{
    for (const auto& outfit : message.outfits())
        outfits.push_back({ static_cast<uint16_t>(outfit.look_type()), text(outfit.name()), outfit.addons(), outfit.current() != 0,
                            appearanceSource(outfit.source()) });
    outfitColors = colors(message.outfit_colors());
    for (const auto& mount : message.mounts())
        mounts.push_back({ static_cast<uint16_t>(mount.look_type()), text(mount.name()), mount.current() != 0, appearanceSource(mount.source()) });
    mountColors = colors(message.mount_colors());
    for (const auto& familiar : message.familiars())
        summonSkins.push_back(
            { static_cast<uint16_t>(familiar.look_type()), text(familiar.name()), familiar.current() != 0, appearanceSource(familiar.source()) });
}

TCharacterTitle::TCharacterTitle(const uint8_t id, QString name, QString description, const bool permanent, const bool unlocked)
    : id(id)
    , name(std::move(name))
    , description(std::move(description))
    , permanent(permanent)
    , unlocked(unlocked)
{}

TCharacterTitlesInfo::TCharacterTitlesInfo(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
    : currentTitle(static_cast<uint8_t>(message.current_title()))
{
    for (const auto& title : message.titles())
        titles.emplace_back(static_cast<uint8_t>(title.id()), text(title.name()), text(title.description()), title.permanent(),
                            title.unlocked());
}

TCharacterInfoStorage::TCharacterInfoStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCharacterInfoStorage(parent)
    , m_achievements(std::make_shared<TAchievementStorage>())
{}

TCharacterInfoStorage::~TCharacterInfoStorage() = default;

bool TCharacterInfoStorage::hasInfo(const int dataType) const
{
    switch (dataType) {
        case BaseInformation: return m_baseInformation != nullptr;
        case GeneralStats: return m_generalStatistics != nullptr;
        case OffenceStats: return m_offenceStats != nullptr;
        case DefenceStats: return m_defenceStats != nullptr;
        case MiscStats: return m_miscStats != nullptr;
        case RecentDeaths: return m_recentDeaths != nullptr;
        case RecentPvpKills: return m_recentPvpKills != nullptr;
        case Achievements: return m_achievements->isLoaded();
        case ItemSummary: return m_itemSummary != nullptr;
        case OutfitsMountsSummonSkins: return m_outfitsMountsSummonSkins != nullptr;
        case StoreSummary: return m_storeSummary != nullptr;
        case Inspection: return m_inspection != nullptr;
        case Titles: return m_titlesInfo != nullptr;
        default: return false;
    }
}

TCharacterBaseInformation TCharacterInfoStorage::baseInformation() const
{
    return m_baseInformation ? *m_baseInformation : TCharacterBaseInformation{};
}

std::vector<RecentDeathEntry> TCharacterInfoStorage::recentDeaths() const
{
    return m_recentDeaths ? *m_recentDeaths : std::vector<RecentDeathEntry>{};
}

std::vector<RecentPvPKillEntry> TCharacterInfoStorage::recentPvpKills() const
{
    return m_recentPvpKills ? *m_recentPvpKills : std::vector<RecentPvPKillEntry>{};
}

CyclopediaCharacterItemSummary TCharacterInfoStorage::itemSummary() const
{
    return m_itemSummary ? *m_itemSummary : CyclopediaCharacterItemSummary{};
}

CyclopediaCharacterInspection TCharacterInfoStorage::inspection() const
{
    return m_inspection ? *m_inspection : CyclopediaCharacterInspection{};
}

qint64 TCharacterInfoStorage::currentPage(const int dataType) const
{
    if (dataType == RecentPvpKills)
        return m_recentPvpKillsCurrentPage;
    if (dataType == RecentDeaths)
        return m_recentDeathsCurrentPage;
    return 1;
}

qint64 TCharacterInfoStorage::pageCount(const int dataType) const
{
    if (dataType == RecentPvpKills)
        return m_recentPvpKillsPageCount;
    if (dataType == RecentDeaths)
        return m_recentDeathsPageCount;
    return 1;
}

void TCharacterInfoStorage::setBaseInformation(TCharacterBaseInformation information)
{
    m_baseInformation = std::make_unique<TCharacterBaseInformation>(std::move(information));
    Q_EMIT infoUpdated(BaseInformation);
}

void TCharacterInfoStorage::setGeneralStatistics(std::unique_ptr<protobuf::protocol::ProtocolCharacterInfoGeneralStats> statistics)
{
    m_generalStatistics = std::make_shared<TGeneralPlayerStatisticsProvider>(std::move(statistics));
    Q_EMIT infoUpdated(GeneralStats);
}

void TCharacterInfoStorage::setOffenceStats(const CyclopediaCharacterOffenceStats& stats)
{
    m_offenceStats = std::make_shared<TPlayerOffenceStatsProvider>(stats);
    Q_EMIT infoUpdated(OffenceStats);
}

void TCharacterInfoStorage::setDefenceStats(const CyclopediaCharacterDefenceStats& stats)
{
    m_defenceStats = std::make_shared<TPlayerDefenceStatsProvider>(stats);
    Q_EMIT infoUpdated(DefenceStats);
}

void TCharacterInfoStorage::setMiscStats(const CyclopediaCharacterMiscStats& stats)
{
    m_miscStats = std::make_shared<TPlayerMiscStatsProvider>(stats);
    Q_EMIT infoUpdated(MiscStats);
}

void TCharacterInfoStorage::setRecentDeaths(std::vector<RecentDeathEntry> deaths, const qint64 currentPage, const qint64 pageCount)
{
    m_recentDeaths = std::make_unique<std::vector<RecentDeathEntry>>(std::move(deaths));
    m_recentDeathsCurrentPage = currentPage;
    m_recentDeathsPageCount = pageCount;
    Q_EMIT infoUpdated(RecentDeaths);
}

void TCharacterInfoStorage::setRecentPvpKills(std::vector<RecentPvPKillEntry> kills, const qint64 currentPage, const qint64 pageCount)
{
    m_recentPvpKills = std::make_unique<std::vector<RecentPvPKillEntry>>(std::move(kills));
    m_recentPvpKillsCurrentPage = currentPage;
    m_recentPvpKillsPageCount = pageCount;
    Q_EMIT infoUpdated(RecentPvpKills);
}

void TCharacterInfoStorage::setItemSummary(CyclopediaCharacterItemSummary summary)
{
    m_itemSummary = std::make_unique<CyclopediaCharacterItemSummary>(std::move(summary));
    Q_EMIT infoUpdated(ItemSummary);
}

void TCharacterInfoStorage::setStoreSummary(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
{
    m_storeSummary = std::make_shared<TStoreSummary>(message);
    Q_EMIT infoUpdated(StoreSummary);
}

void TCharacterInfoStorage::setOutfitsMountsSummonSkins(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
{
    m_outfitsMountsSummonSkins = std::make_shared<TOutfitsMountsSummonSkins>(message);
    Q_EMIT infoUpdated(OutfitsMountsSummonSkins);
}

void TCharacterInfoStorage::setAccountInfos(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
{
    m_accountInfos = std::make_shared<TAccountInfos>(message);
    // The account infos belong to the general stats page.
    Q_EMIT infoUpdated(GeneralStats);
}

void TCharacterInfoStorage::setInspection(CyclopediaCharacterInspection inspection)
{
    m_inspection = std::make_unique<CyclopediaCharacterInspection>(std::move(inspection));
    Q_EMIT infoUpdated(Inspection);
}

void TCharacterInfoStorage::setTitlesInfo(const protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& message)
{
    m_titlesInfo = std::make_shared<TCharacterTitlesInfo>(message);
    Q_EMIT infoUpdated(Titles);
}

void TCharacterInfoStorage::clearAll()
{
    m_achievements->clear();
    m_baseInformation.reset();
    m_generalStatistics.reset();
    m_offenceStats.reset();
    m_defenceStats.reset();
    m_miscStats.reset();
    m_recentDeaths.reset();
    m_recentPvpKills.reset();
    m_itemSummary.reset();
    m_storeSummary.reset();
    m_outfitsMountsSummonSkins.reset();
    m_inspection.reset();
    m_titlesInfo.reset();
    m_recentDeathsCurrentPage = 1;
    m_recentPvpKillsCurrentPage = 1;
    m_recentDeathsPageCount = 1;
    m_recentPvpKillsPageCount = 1;
}

void TCharacterInfoStorage::clear(const int dataType)
{
    switch (dataType) {
        case BaseInformation: m_baseInformation.reset(); break;
        case GeneralStats: m_generalStatistics.reset(); break;
        case OffenceStats: m_offenceStats.reset(); break;
        case DefenceStats: m_defenceStats.reset(); break;
        case MiscStats: m_miscStats.reset(); break;
        case RecentDeaths: m_recentDeaths.reset(); break;
        case RecentPvpKills: m_recentPvpKills.reset(); break;
        case Achievements:
            m_achievements->clear();
            break;
        case ItemSummary: m_itemSummary.reset(); break;
        case OutfitsMountsSummonSkins: m_outfitsMountsSummonSkins.reset(); break;
        case StoreSummary: m_storeSummary.reset(); break;
        case Inspection: m_inspection.reset(); break;
        default: break;
    }
}

void TCharacterInfoStorage::setAchievementsLoaded()
{
    m_achievements->setLoaded();
    Q_EMIT infoUpdated(Achievements);
}

void TCharacterInfoStorage::reportError(const int dataType, const int error) { Q_EMIT errorReceived(dataType, error); }

}
