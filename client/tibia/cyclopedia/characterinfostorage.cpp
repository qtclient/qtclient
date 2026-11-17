#include "characterinfostorage.h"

#include "cyclopedia/playerdefencestatsprovider.h"
#include "cyclopedia/playermiscstatsprovider.h"
#include "cyclopedia/playeroffencestatsprovider.h"

#include <QCoreApplication>

namespace tibia::cyclopedia {

namespace {

TCharacterInfoStorage* s_instance = nullptr;

}

TCharacterInfoStorage::TCharacterInfoStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCharacterInfoStorage(parent)
    , m_achievements(std::make_shared<TAchievementStorage>())
{}

TCharacterInfoStorage::~TCharacterInfoStorage()
{
    if (s_instance == this)
        s_instance = nullptr;
}

TCharacterInfoStorage* TCharacterInfoStorage::instance()
{
    if (!s_instance)
        s_instance = new TCharacterInfoStorage(QCoreApplication::instance());
    return s_instance;
}

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
        case Achievements: return m_achievements->loaded;
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

void TCharacterInfoStorage::setGeneralStatistics(TGeneralPlayerStatistics statistics)
{
    m_generalStatistics = std::make_shared<TGeneralPlayerStatistics>(std::move(statistics));
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

void TCharacterInfoStorage::setStoreSummary(TStoreSummary summary)
{
    m_storeSummary = std::make_shared<TStoreSummary>(std::move(summary));
    Q_EMIT infoUpdated(StoreSummary);
}

void TCharacterInfoStorage::setOutfitsMountsSummonSkins(TOutfitsMountsSummonSkins appearances)
{
    m_outfitsMountsSummonSkins = std::make_shared<TOutfitsMountsSummonSkins>(std::move(appearances));
    Q_EMIT infoUpdated(OutfitsMountsSummonSkins);
}

void TCharacterInfoStorage::setAccountInfos(TAccountInfos infos)
{
    m_accountInfos = std::make_shared<TAccountInfos>(std::move(infos));
    // The account infos belong to the general stats page.
    Q_EMIT infoUpdated(GeneralStats);
}

void TCharacterInfoStorage::setInspection(CyclopediaCharacterInspection inspection)
{
    m_inspection = std::make_unique<CyclopediaCharacterInspection>(std::move(inspection));
    Q_EMIT infoUpdated(Inspection);
}

void TCharacterInfoStorage::setTitlesInfo(TCharacterTitlesInfo titles)
{
    m_titlesInfo = std::make_shared<TCharacterTitlesInfo>(std::move(titles));
    Q_EMIT infoUpdated(Titles);
}

void TCharacterInfoStorage::clearAll()
{
    m_achievements->achievements.clear();
    m_achievements->achievementPoints = 0;
    m_achievements->loaded = false;
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
            m_achievements->achievements.clear();
            m_achievements->achievementPoints = 0;
            m_achievements->loaded = false;
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
    m_achievements->loaded = true;
    Q_EMIT infoUpdated(Achievements);
}

void TCharacterInfoStorage::reportError(const int dataType, const int error) { Q_EMIT errorReceived(dataType, error); }

}
