#pragma once

#include <client/outfit.h>
#include <client/staticdata.h>

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace tibia {
namespace cyclopedia {

class TPlayerOffenceStatsProvider;
class TPlayerDefenceStatsProvider;
class TPlayerMiscStatsProvider;

/// The character the info is about: name, vocation, level, outfit and title.
struct TCharacterBaseInformation
{
    QString name;
    QString vocation;
    int level = 0;
    Outfit outfit;
    bool ownCharacter = false;
    QString title;
};

/// The general stats record (experience, health, mana, skills, specialised magic levels).
struct TGeneralPlayerStatistics
{
    CyclopediaCharacterGeneralStats stats{};
    std::vector<std::vector<uint16_t>> skills;
    std::vector<std::tuple<uint8_t, uint16_t>> specializedMagicLevels;
};

/// The account infos the general stats page shows (the badges answer).
struct TAccountInfos
{
    bool canShowAccountInfos = false;
    bool online = false;
    bool premium = false;
    QString loyaltyTitle;
    std::vector<std::tuple<uint32_t, std::string>> badges;
};

/// The store purchases summary.
struct TStoreSummary
{
    uint32_t storeXpBoostTime = 0;
    uint32_t dailyRewardXpBoostTime = 0;
    std::vector<std::tuple<std::string, uint8_t>> blessings;
    int preySlots = 0;
    int preyWildcards = 0;
    bool permanentWeeklyTaskExpansion = false;
    int instantRewardAccesses = 0;
    bool charmExpansion = false;
    int hirelings = 0;
    std::vector<uint16_t> hirelingJobs;
    std::vector<std::tuple<uint16_t, std::string, uint8_t>> houseItems;
    std::vector<std::tuple<std::string, uint8_t>> hirelingOutfits;
};

/// The outfits, mounts and summon skins of the character with its colours.
struct TOutfitsMountsSummonSkins
{
    OutfitColorStruct colors{};
    std::vector<CharacterInfoOutfits> outfits;
    std::vector<CharacterInfoMounts> mounts;
    std::vector<CharacterInfoFamiliar> summonSkins;
};

/// The character titles and the current one.
struct TCharacterTitlesInfo
{
    int currentTitle = 0;
    /// id, name, description, permanent, unlocked
    std::vector<std::tuple<uint8_t, std::string, std::string, bool, bool>> titles;
};

/// The unlocked achievements; filled through TCharacterInfoStorage::achievementStorage.
struct TAchievementStorage
{
    struct TAchievement
    {
        uint16_t id = 0;
        uint32_t timestamp = 0;
        bool secret = false;
        QString name;
        QString description;
        uint8_t grade = 0;
    };
    std::vector<TAchievement> achievements;
    int achievementPoints = 0;
    int secretAchievements = 0;
    bool loaded = false;
};

/// The answers of the character info requests, one slot per page. Every setter emits
/// infoUpdated with its page.
class TCharacterInfoStorage final : public cip::tibia::cyclopedia::TCharacterInfoStorage
{
    Q_OBJECT
public:
    /// The pages of the Cyclopedia's character info.
    enum ECharacterInfoDataType
    {
        BaseInformation = 0,
        GeneralStats = 1,
        OffenceStats = 2,
        DefenceStats = 3,
        MiscStats = 4,
        RecentDeaths = 5,
        RecentPvpKills = 6,
        Achievements = 7,
        ItemSummary = 8,
        OutfitsMountsSummonSkins = 9,
        StoreSummary = 10,
        Inspection = 11,
        Badges = 12,
        Titles = 13,
    };

    explicit TCharacterInfoStorage(QObject* parent = nullptr);
    ~TCharacterInfoStorage() override;

    static TCharacterInfoStorage* instance();

    TAchievementStorage& achievementStorage() { return *m_achievements; }
    bool hasInfo(int dataType) const;

    TCharacterBaseInformation baseInformation() const;
    std::shared_ptr<TGeneralPlayerStatistics> generalStatistics() const { return m_generalStatistics; }
    std::shared_ptr<TPlayerOffenceStatsProvider> offenceStats() const { return m_offenceStats; }
    std::shared_ptr<TPlayerDefenceStatsProvider> defenceStats() const { return m_defenceStats; }
    std::shared_ptr<TPlayerMiscStatsProvider> miscStats() const { return m_miscStats; }
    std::shared_ptr<TStoreSummary> storeSummary() const { return m_storeSummary; }
    std::shared_ptr<TOutfitsMountsSummonSkins> outfitsMountsSummonSkins() const { return m_outfitsMountsSummonSkins; }
    std::vector<RecentDeathEntry> recentDeaths() const;
    std::vector<RecentPvPKillEntry> recentPvpKills() const;
    CyclopediaCharacterItemSummary itemSummary() const;
    CyclopediaCharacterInspection inspection() const;
    std::shared_ptr<TAccountInfos> accountInfos() const { return m_accountInfos; }
    std::shared_ptr<TCharacterTitlesInfo> titlesInfo() const { return m_titlesInfo; }
    /// The shown page of the recent deaths or kills (1 for any other type).
    qint64 currentPage(int dataType) const;
    qint64 pageCount(int dataType) const;

    void setBaseInformation(TCharacterBaseInformation information);
    void setGeneralStatistics(TGeneralPlayerStatistics statistics);
    void setOffenceStats(const CyclopediaCharacterOffenceStats& stats);
    void setDefenceStats(const CyclopediaCharacterDefenceStats& stats);
    void setMiscStats(const CyclopediaCharacterMiscStats& stats);
    void setRecentDeaths(std::vector<RecentDeathEntry> deaths, qint64 currentPage, qint64 pageCount);
    void setRecentPvpKills(std::vector<RecentPvPKillEntry> kills, qint64 currentPage, qint64 pageCount);
    void setItemSummary(CyclopediaCharacterItemSummary summary);
    void setStoreSummary(TStoreSummary summary);
    void setOutfitsMountsSummonSkins(TOutfitsMountsSummonSkins appearances);
    void setAccountInfos(TAccountInfos infos);
    void setInspection(CyclopediaCharacterInspection inspection);
    void setTitlesInfo(TCharacterTitlesInfo titles);
    /// Forgets every page (the account infos stay) and resets the pagination.
    void clearAll();
    void clear(int dataType);
    /// The achievement storage is complete.
    void setAchievementsLoaded();
    /// The server could not answer a request.
    void reportError(int dataType, int error);

Q_SIGNALS:
    void infoUpdated(int dataType);
    void errorReceived(int dataType, int error);

private:
    std::unique_ptr<TCharacterBaseInformation> m_baseInformation;
    std::shared_ptr<TAchievementStorage> m_achievements;
    std::shared_ptr<TGeneralPlayerStatistics> m_generalStatistics;
    std::shared_ptr<TPlayerOffenceStatsProvider> m_offenceStats;
    std::shared_ptr<TPlayerDefenceStatsProvider> m_defenceStats;
    std::shared_ptr<TPlayerMiscStatsProvider> m_miscStats;
    std::shared_ptr<TStoreSummary> m_storeSummary;
    std::shared_ptr<TOutfitsMountsSummonSkins> m_outfitsMountsSummonSkins;
    std::unique_ptr<std::vector<RecentDeathEntry>> m_recentDeaths;
    std::unique_ptr<std::vector<RecentPvPKillEntry>> m_recentPvpKills;
    std::unique_ptr<CyclopediaCharacterItemSummary> m_itemSummary;
    std::unique_ptr<CyclopediaCharacterInspection> m_inspection;
    std::shared_ptr<TAccountInfos> m_accountInfos;
    std::shared_ptr<TCharacterTitlesInfo> m_titlesInfo;
    qint64 m_recentDeathsCurrentPage = 1;
    qint64 m_recentDeathsPageCount = 1;
    qint64 m_recentPvpKillsCurrentPage = 1;
    qint64 m_recentPvpKillsPageCount = 1;
};

}
}
