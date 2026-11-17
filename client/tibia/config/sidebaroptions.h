#pragma once

#include "config/characterconfigurationmanager.h"

#include <cip_gen.h>

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTimer>

namespace tibia {
namespace config {

/// TSidebarPanelsManager's panel types (sidebars.json names "minimap",
/// "inventoryStatus", "hitpointManabar", "buttons").
enum class SidebarPanelType { Minimap = 0, InventoryStatus = 1, HitpointManabar = 2, Buttons = 3 };

/// CIP's sidebar widget types, as sidebars.json names them.
enum class SidebarWidgetType {
    Skills = 0,
    BattleList = 1,
    Vip = 3,
    UnjustifiedPoints = 4,
    Prey = 5,
    SpellList = 6,
    Container = 8,
    QuestTracker = 10,
    BestiaryTracker = 11,
    BosstiaryTracker = 12,
    BossCooldowns = 13,
    LootTracker = 14,
    XpAnalyser = 15,
    LootAnalyser = 16,
    WasteAnalyser = 17,
    ImpactAnalyser = 18,
    DamageInputAnalyser = 19,
    HuntingSessionAnalyser = 20,
    AnalyticsSelector = 21,
    PartyHuntAnalyser = 22,
    ImbuementTracker = 23,
    PlayerGuide = 24,
};

/// The options every sidebar widget keeps: its content height and whether it
/// is maximized (CIP: TWidgetOptionsBase).
struct TWidgetOptionsBase
{
    int contentHeight = 0;
    bool contentMaximized = true;

    void setContentHeightAndMaximized(int height, bool maximized)
    {
        contentHeight = height;
        contentMaximized = maximized;
    }
    bool operator==(const TWidgetOptionsBase&) const = default;
};
using ContainerWidgetOptions = TWidgetOptionsBase;
/// containersOptions, keyed by container id (a byte in CIP).
using ContainersOptions = std::map<int, ContainerWidgetOptions>;

/// sidebarPanelsOptions.
struct SidebarPanelsOptions
{
    /// Minimap zoom exponent, -1 to 2; stored as round(zoom * 100).
    float minimapZoomLevel = 2.0f;
    bool controlButtonsVisible = true;
    bool minimizeInventory = false;
    bool chaseModeEnabled = true;

    bool operator==(const SidebarPanelsOptions&) const = default;
};

/// sidebarWidgetsMangerOptions: per sidebar the open widgets as
/// (type, instance) pairs, right sidebars first, then the left ones.
struct TSidebarWidgetsManagerOptions
{
    std::vector<std::vector<std::pair<int, int>>> openWidgetsOrderPerSidebar;
    quint8 leftSidebarCount = 0;

    bool operator==(const TSidebarWidgetsManagerOptions&) const = default;
};

/// The JSON form of TSidebarWidgetsManagerOptions TSidebarWidgetsManager
/// works on (openWidgetsOrderPerSidebar as written to sidebars.json).
struct SidebarWidgetsManagerOptions
{
    int leftSidebarCount = 0;
    QJsonArray openWidgetsOrderPerSidebar;

    int rightSidebarCount() const { return std::max<int>(1, static_cast<int>(openWidgetsOrderPerSidebar.size()) - leftSidebarCount); }
    bool operator==(const SidebarWidgetsManagerOptions&) const = default;
};

struct TSkillsWidgetOptions : TWidgetOptionsBase
{
    /// Skill ids whose progress bar is hidden.
    std::set<int> invisibleProgressBars;
    bool offenceStatsVisible = true;
    bool defenceStatsVisible = true;
    bool miscStatsVisible = true;

    bool operator==(const TSkillsWidgetOptions&) const = default;
};

struct TSpellListWidgetOptions : TWidgetOptionsBase
{
    bool showOnlyCurrentVocation = false;
    bool showOnlyCurrentLevel = false;
    bool showUnknownSpells = false;
    bool showDruidSpells = true;
    bool showKnightSpells = true;
    bool showPaladinSpells = true;
    bool showSorcererSpells = true;
    bool showMonkSpells = true;
    bool showAttackSpellGroup = true;
    bool showHealingSpellGroup = true;
    bool showSupportSpellGroup = true;
    bool showPremiumOnlySpells = true;
    bool showFreeSpells = true;

    bool operator==(const TSpellListWidgetOptions&) const = default;
};

struct TVipWidgetOptions : TWidgetOptionsBase
{
    /// byName 0, byType 1, byState 2.
    std::vector<int> vipSortOrder;
    bool hideOfflineVips = false;
    bool showGrouped = false;

    bool operator==(const TVipWidgetOptions&) const = default;
};

/// One battle list's options (CIP: TBattleListOptions).
struct TBattleListOptions : TWidgetOptionsBase
{
    bool showFilters = false;
    bool isPartyView = false;
    bool isPrimary = false;
    QString name;
    /// byAgeAscending 0 .. byNameDescending 7.
    std::vector<int> sortOrder{ 0 };
    /// hidePlayers 0 .. hideMembersOfOwnGuild 11.
    std::vector<int> filters;

    /// The sort, then byAgeAscending.
    void setSortOrder(int order);
    bool operator==(const TBattleListOptions&) const = default;
};
/// battleListsOptions, keyed by battle list id (a byte in CIP).
using TBattleListsOptions = std::map<int, TBattleListOptions>;

/// Bestiary and bosstiary tracker options.
struct TCreatureTrackerWidgetOptions : TWidgetOptionsBase
{
    /// name 0, completion 1, remaining 2.
    int sortKey = 0;
    /// ascending 0, descending 1.
    int sortOrder = 0;

    bool operator==(const TCreatureTrackerWidgetOptions&) const = default;
};

struct TBossTrackerWidgetOptions : TWidgetOptionsBase
{
    /// remainingCooldown 0, name 1.
    int sortKey = 0;

    bool operator==(const TBossTrackerWidgetOptions&) const = default;
};

struct TNpcTradeOptions : TWidgetOptionsBase
{
    bool allowSellingEquippedItems = false;
    bool ignoreCapacityWhenBuying = false;
    bool purchaseShoppingBagWhenBuying = false;
    bool showNameFilter = true;
    bool allowLargeTradesWithoutWarning = false;
    /// name 0, price 1, weight 2, amount 3.
    int sortFieldWhenBuying = 0;
    int sortFieldWhenSelling = 3;

    bool operator==(const TNpcTradeOptions&) const = default;
};

struct TDepotSearchWidgetOptions : TWidgetOptionsBase
{
    bool showLockerOnly = false;

    bool operator==(const TDepotSearchWidgetOptions&) const = default;
};

struct TImbuementTrackerWidgetOptions : TWidgetOptionsBase
{
    bool showAlmostGone = true;
    bool showUsed = true;
    bool showAlmostNew = true;
    bool showEmptySlots = true;

    bool operator==(const TImbuementTrackerWidgetOptions&) const = default;
};

struct TPlayerGuideWidgetOptions : TWidgetOptionsBase
{
    std::set<int> pendingLevelSpecialPopups;
    std::set<int> confirmedLevelSpecialPopups;

    bool operator==(const TPlayerGuideWidgetOptions&) const = default;
};

/// A character's sidebar configuration (CIP: TSidebarOptions), kept in
/// <app data>/characterdata/<character id>/sidebars.json as 27 typed
/// sections. Like CIP's TCharacterConfigurationManager, changes are written
/// when the session ends, not as they happen. Keys CIP does not know (written
/// by qtclient widgets) are kept as read and written back.
class TSidebarOptions final : public cip::tibia::config::TSidebarOptions, public ICharacterConfiguration
{
    Q_OBJECT
public:
    /// The battle list ids CIP reserves for the primary and the party list.
    static constexpr int PrimaryBattleListId = 0xfe;
    static constexpr int PartyBattleListId = 0xfd;

    explicit TSidebarOptions(QObject* parent = nullptr);

    /// The file's base name.
    static QString fileName();
    /// All sections as written to the file.
    QJsonObject toJson() const;
    /// Replaces the sections present in json and signals every section.
    void fromJson(const QJsonObject& json);

    // TCharacterConfigurationManager: the file also keeps the keys CIP does
    // not know, as read.
    QString configurationName() const override { return fileName(); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override { fromJson(json); }
    void resetConfiguration() override;

    const QList<SidebarPanelType>& panelsOrder() const { return m_panelsOrder; }
    void setPanelsOrder(const QList<SidebarPanelType>& order);

    const SidebarPanelsOptions& sidebarPanelsOptions() const { return m_panels; }
    void setSidebarPanelsOptions(const SidebarPanelsOptions& options);

    const TSidebarWidgetsManagerOptions& widgetsManagerOptions() const { return m_widgetsManager; }
    void setWidgetsManagerOptions(const TSidebarWidgetsManagerOptions& options);
    /// The same options in their JSON form.
    SidebarWidgetsManagerOptions sidebarWidgetsManagerOptions() const;
    void setSidebarWidgetsManagerOptions(const SidebarWidgetsManagerOptions& options);

    const TWidgetOptionsBase& preyWidgetOptions() const { return m_prey; }
    void setPreyWidgetOptions(const TWidgetOptionsBase& options);

    const TBattleListsOptions& battleListsOptions() const { return m_battleLists; }
    /// Throws when there is no primary battle list (CIP does too).
    const TBattleListOptions& primaryBattleListOptions() const;
    void setBattleListsOptions(const TBattleListsOptions& options);
    void setBattleListOptions(int battleListId, const TBattleListOptions& options);
    void removeBattleListOptions(int battleListId);
    /// Moves the primary battle list's options to battleListId.
    void makePrimaryBattleList(int battleListId);
    /// Makes battleListId the party battle list.
    void makePartyBattleList(int battleListId);

    const TSkillsWidgetOptions& skillsWidgetOptions() const { return m_skills; }
    void setSkillsWidgetOptions(const TSkillsWidgetOptions& options);
    const TSpellListWidgetOptions& spellListWidgetOptions() const { return m_spellList; }
    void setSpellListWidgetOptions(const TSpellListWidgetOptions& options);
    const TVipWidgetOptions& vipWidgetOptions() const { return m_vip; }
    void setVipWidgetOptions(const TVipWidgetOptions& options);
    const TWidgetOptionsBase& huntingSessionAnalyserWidgetOptions() const { return m_huntingSessionAnalyser; }
    void setHuntingSessionAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& impactAnalyserWidgetOptions() const { return m_impactAnalyser; }
    void setImpactAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& damageInputAnalyserWidgetOptions() const { return m_damageInputAnalyser; }
    void setDamageInputAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& lootAnalyserWidgetOptions() const { return m_lootAnalyser; }
    void setLootAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& wasteAnalyserWidgetOptions() const { return m_wasteAnalyser; }
    void setWasteAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& lootTrackerWidgetOptions() const { return m_lootTracker; }
    void setLootTrackerWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& progressAnalyserWidgetOptions() const { return m_progressAnalyser; }
    void setProgressAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& questTrackerWidgetOptions() const { return m_questTracker; }
    void setQuestTrackerWidgetOptions(const TWidgetOptionsBase& options);
    const TCreatureTrackerWidgetOptions& bestiaryTrackerWidgetOptions() const { return m_bestiaryTracker; }
    void setBestiaryTrackerWidgetOptions(const TCreatureTrackerWidgetOptions& options);
    const TCreatureTrackerWidgetOptions& bosstiaryTrackerWidgetOptions() const { return m_bosstiaryTracker; }
    void setBosstiaryTrackerWidgetOptions(const TCreatureTrackerWidgetOptions& options);
    const TBossTrackerWidgetOptions& bossTrackerWidgetOptions() const { return m_bossTracker; }
    void setBossTrackerWidgetOptions(const TBossTrackerWidgetOptions& options);
    const TNpcTradeOptions& npcTradeOptions() const { return m_npcTrade; }
    void setNpcTradeOptions(const TNpcTradeOptions& options);

    /// optionsForAllContainersChanged follows a change on the next event loop
    /// turn, and once when a character's file is read.
    const ContainersOptions& containersOptions() const { return m_containers; }
    void setContainersOptions(const ContainersOptions& options);

    const TWidgetOptionsBase& unjustifiedPointsWidgetOptions() const { return m_unjustifiedPoints; }
    void setUnjustifiedPointsWidgetOptions(const TWidgetOptionsBase& options);
    const TWidgetOptionsBase& analyticsSelectorWidgetOptions() const { return m_analyticsSelector; }
    void setAnalyticsSelectorWidgetOptions(const TWidgetOptionsBase& options);
    const TDepotSearchWidgetOptions& depotSearchWidgetOptions() const { return m_depotSearch; }
    void setDepotSearchWidgetOptions(const TDepotSearchWidgetOptions& options);
    const TWidgetOptionsBase& partyHuntAnalyserWidgetOptions() const { return m_partyHuntAnalyser; }
    void setPartyHuntAnalyserWidgetOptions(const TWidgetOptionsBase& options);
    const TImbuementTrackerWidgetOptions& imbuementTrackerWidgetOptions() const { return m_imbuementTracker; }
    void setImbuementTrackerWidgetOptions(const TImbuementTrackerWidgetOptions& options);
    const TPlayerGuideWidgetOptions& playerGuideWidgetOptions() const { return m_playerGuide; }
    void setPlayerGuideWidgetOptions(const TPlayerGuideWidgetOptions& options);

    /// A top-level section in its file form. CIP's sections go through their
    /// typed options; other keys are kept as written.
    QJsonValue section(const QString& key) const;
    void setSection(const QString& key, const QJsonValue& value);

Q_SIGNALS:
    void battleListWidgetOptionsChanged(int battleListId);

private Q_SLOTS:
    void onDelayOptionsForAllContainersChangedTriggerd();

private:
    void reset();
    void ensurePartyBattleList();
    void ensurePrimaryBattleList();
    void changed();

    /// Keys of the file CIP does not know.
    QJsonObject m_unknownSections;

    QTimer m_containersChangedTimer;
    QList<SidebarPanelType> m_panelsOrder;
    SidebarPanelsOptions m_panels;
    TSidebarWidgetsManagerOptions m_widgetsManager;
    TWidgetOptionsBase m_prey;
    TBattleListsOptions m_battleLists;
    TSkillsWidgetOptions m_skills;
    TSpellListWidgetOptions m_spellList;
    TVipWidgetOptions m_vip;
    TWidgetOptionsBase m_huntingSessionAnalyser;
    TWidgetOptionsBase m_impactAnalyser;
    TWidgetOptionsBase m_damageInputAnalyser;
    TWidgetOptionsBase m_lootAnalyser;
    TWidgetOptionsBase m_wasteAnalyser;
    TWidgetOptionsBase m_lootTracker;
    TWidgetOptionsBase m_progressAnalyser;
    TWidgetOptionsBase m_questTracker;
    TCreatureTrackerWidgetOptions m_bestiaryTracker;
    TCreatureTrackerWidgetOptions m_bosstiaryTracker;
    TBossTrackerWidgetOptions m_bossTracker;
    TNpcTradeOptions m_npcTrade;
    ContainersOptions m_containers;
    TWidgetOptionsBase m_unjustifiedPoints;
    TWidgetOptionsBase m_analyticsSelector;
    TDepotSearchWidgetOptions m_depotSearch;
    TWidgetOptionsBase m_partyHuntAnalyser;
    TImbuementTrackerWidgetOptions m_imbuementTracker;
    TPlayerGuideWidgetOptions m_playerGuide;
};

}
}
