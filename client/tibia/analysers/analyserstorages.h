#pragma once

#include "client/gamecore.h"
#include "config/icharacterconfiguration.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TItemTrackingStorage;
}
namespace game {
class IGameSession;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
}

namespace tibia {
namespace analysers {

/// CIP (n): n linear weights w[i] = 2(i+1) / (n(n+1)), oldest first, summing to 1.
std::vector<double> linearWeights(int n);
/// CIP : a graph's y range follows its maximum with hysteresis.
double adjustGraphMaxY(double currentMaxY, double maxValue);

/// One of TProgressStorage's TExperienceChangeTracker (CIP ctor in): the session total
/// and 4501 one-second bins, the newest last.
class TExperienceChangeTracker : public ::tibia::CipTyped
{
public:
    static constexpr int BinCount = 4501;
    static constexpr int WindowSeconds = 900;

    TExperienceChangeTracker();
    /// Shifts the bins to `nowMs`'s second; true if any bin changed.
    bool tick(qint64 nowMs);
    void add(qint64 amount);
    void reset();
    qint64 total() const { return m_total; }
    /// The linear-weighted mean per second of the 900 complete seconds that end `offsetSeconds`
    /// before the newest (open) bin (CIP).
    double perSecond(int offsetSeconds = 0) const;
    /// The weighted rate per hour; 0 while fewer seconds than the window are kept.
    double perHour() const { return perSecond() * 3600.0; }
    /// The weighted rate per minute.
    double perMinute() const { return perSecond() * 60.0; }

private:
    qint64 m_total = 0;
    std::vector<double> m_bins;
    qint64 m_second = 0;
};

/// XP analyser storage (CIP: tibia::cyclopedia::TProgressStorage, config "xpanalyser").
class TProgressStorage final : public cip::tibia::cyclopedia::TProgressStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    explicit TProgressStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("xpanalyser"); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    /// CIP vt15.
    void tick(qint64 nowMs);
    /// CIP vt16.
    void reset();
    /// CIP slot onExperienceGained: raw (base) and final experience of one XpChanged message.
    void onExperienceGained(qint64 raw, qint64 final, qint64 nowMs);

    const TExperienceChangeTracker& finalXp() const { return m_final; }
    const TExperienceChangeTracker& baseXp() const { return m_base; }

    bool gaugeVisible() const { return m_gaugeVisible; }
    void setGaugeVisible(bool visible);
    /// XP per second the gauge aims at (the widget shows it per hour).
    double gaugeTarget() const { return m_gaugeTarget; }
    void setGaugeTarget(double target);
    bool graphVisible() const { return m_graphVisible; }
    void setGraphVisible(bool visible);
    bool showBaseXp() const { return m_showBaseXp; }
    void setShowBaseXp(bool show);

    /// CIP : one point per minute, x from history[0].x, y the XP per hour then.
    void computeHistory(QVariantList& history, double& maxY) const;

Q_SIGNALS:
    /// Tip alias of CIP experienceDataChanged.
    void dataChanged();
    /// Tip alias of CIP experienceDataReset.
    void wasReset();
    /// Tip alias of CIP configurationChangedFromUserInput.
    void optionsChanged();

private:
    TExperienceChangeTracker m_final;
    TExperienceChangeTracker m_base;
    /// The gauge target stays 0 until the configuration is read.
    bool m_gaugeVisible = true;
    double m_gaugeTarget = 0.0;
    bool m_graphVisible = true;
    bool m_showBaseXp = false;
};

class TItemPriceStorage;

/// Loot and supply analyser storage (CIP: tibia::cyclopedia::TGainWasteStorage, config
/// "gainandwaste"). Items are added as they come; metrics and signals follow on the next tick.
class TGainWasteStorage final : public cip::tibia::cyclopedia::TGainWasteStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    enum Side
    {
        Loot = 0,
        Waste = 1
    };

    /// One looted or wasted item type of the session.
    struct Entry
    {
        uint16_t itemId = 0;
        QString name;
        quint64 price = 0;
        quint32 totalCount = 0;
        quint32 lastHourCount = 0;
        /// The newest record already taken off lastHourCount.
        qint64 lastAgedTimestamp = 0;
        void addCount(quint32 count);
        void ageRecord(quint32 count, qint64 timestamp, bool aged);
    };

    struct Sums
    {
        qint64 lastHour = 0;
        double perHour = 0;
        qint64 archived = 0;
        qint64 olderThanHour = 0;
        qint64 session() const { return lastHour + archived + olderThanHour; }
    };

    struct Item
    {
        uint16_t itemId = 0;
        QString name;
        quint32 count = 0;
        qint64 price = 0;
    };

    TGainWasteStorage(TItemPriceStorage* prices, QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("gainandwaste"); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    void addLoot(uint16_t itemId, quint32 count, const QString& name, qint64 nowMs);
    void addWaste(uint16_t itemId, quint32 count, qint64 nowMs);
    void tick(qint64 nowMs);
    void reset(Side side);

    const Sums& sums(Side side) const { return m_sides[side].sums; }
    /// Last hour's counts (loot and waste widgets).
    std::vector<Item> lastHourItems(Side side) const;
    /// Session counts (hunting session).
    std::vector<Item> sessionItems(Side side) const;
    /// 60 points, each the weighted mean of 15 minute bins.
    std::vector<double> history(Side side, double& maxY) const;

    bool gaugeVisible(Side side) const { return m_sides[side].gaugeVisible; }
    void setGaugeVisible(Side side, bool visible);
    bool graphVisible(Side side) const { return m_sides[side].graphVisible; }
    void setGraphVisible(Side side, bool visible);
    double gaugeTarget(Side side) const { return m_sides[side].gaugeTarget; }
    void setGaugeTarget(Side side, double target);

public Q_SLOTS:
    void onItemPriceConfigurationChanged();
    void onObjectInfosChanged();

Q_SIGNALS:
    /// Tip alias of CIP lootedItemsChanged / wastedItemsChanged (side = Loot/Waste).
    void itemsChanged(int side);
    /// Tip alias of CIP lootedItemsMetricsChanged / wastedItemsMetricsChanged.
    void metricsChanged();
    /// Tip alias of CIP configurationChangedFromUserInput.
    void optionsChanged();

private:
    struct Record
    {
        qint64 timestamp = 0;
        quint32 count = 0;
        uint16_t itemId = 0;
        bool aged = false;
    };

    struct SideData
    {
        std::map<uint16_t, Entry> entries;
        std::deque<Record> records;
        Sums sums;
        bool gaugeVisible = true;
        bool graphVisible = true;
        double gaugeTarget = 0;
    };

    void computeMetrics(Side side, qint64 nowMs);
    void refreshPrices();
    std::vector<Item> items(Side side, quint32 Entry::*count) const;
    void emitItemsChanged(Side side);
    void emitMetricsChanged(Side side);
    void emitOptionsChanged();

    QPointer<TItemPriceStorage> m_prices;
    std::vector<double> m_weights;
    std::array<SideData, 2> m_sides;
    qint64 m_nextUpdate = 0;
    bool m_dirty = false;
};

/// Damage and healing output (CIP: tibia::cyclopedia::TImpactStorage, ctor ,
/// config "impactanalyser").
class TImpactStorage final : public cip::tibia::cyclopedia::TImpactStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    /// CIPBIA combat types 0..10 (physical .. mana drain); 11 (agony) folds into physical.
    static constexpr int ElementCount = 11;
    static constexpr int RecordCount = 4500;
    static constexpr int GraphPoints = 60;

    enum Kind
    {
        Damage = 0,
        Healing = 1
    };

    struct Summary
    {
        qint64 total = 0;
        std::array<qint64, ElementCount> byElement{};
        bool operator==(const Summary&) const = default;
    };

    /// The sums and rates of damage or healing.
    struct Data
    {
        Summary lastHour;
        Summary session;
        Summary window;
        double sessionPerSecond = 0;
        double maxSessionPerSecond = 0;
        double perSecond = 0;
        double maxPerSecond = 0;
        bool operator==(const Data&) const = default;
    };

    explicit TImpactStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("impactanalyser"); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    /// Element -1: the secondary amount has no element.
    void addDamage(qint64 primary, qint64 secondary, int element, qint64 nowMs);
    void addHealing(qint64 amount, qint64 nowMs);
    void tick(qint64 nowMs);
    void reset();
    /// The persisted strongest hits start over.
    void resetAllTimeMax();

    bool hasSession() const { return m_hasData; }
    const Summary& lastHour(Kind kind) const { return m_data[kind].lastHour; }
    const Summary& session(Kind kind) const { return m_data[kind].session; }
    /// The last 15 s.
    const Summary& window(Kind kind) const { return m_data[kind].window; }
    /// The 15 s linear-weighted per second value.
    double perSecond(Kind kind) const { return m_data[kind].perSecond; }
    double maxPerSecond(Kind kind) const { return m_data[kind].maxPerSecond; }
    /// Session total over the session's length, per second.
    double sessionPerSecond(Kind kind) const { return m_data[kind].sessionPerSecond; }
    /// The highest session per second value.
    double maxSessionPerSecond(Kind kind) const { return m_data[kind].maxSessionPerSecond; }
    /// At least 1 s.
    qint64 sessionLengthMs() const;
    /// At most 15 s.
    qint64 windowLengthMs() const;
    qint64 allTimeMax(Kind kind) const { return m_allTimeMax[kind]; }
    const std::array<double, GraphPoints>& graph(Kind kind) const { return m_graph[kind]; }

    bool showSessionValues() const { return m_showSessionValues; }
    void setShowSessionValues(bool show);
    bool gaugeVisible(Kind kind) const { return m_gaugeVisible[kind]; }
    void setGaugeVisible(Kind kind, bool visible);
    double gaugeTarget(Kind kind) const { return m_gaugeTarget[kind]; }
    void setGaugeTarget(Kind kind, double target);
    bool graphVisible(Kind kind) const { return m_graphVisible[kind]; }
    void setGraphVisible(Kind kind, bool visible);
    bool damageTypesVisible() const { return m_damageTypesVisible; }
    void setDamageTypesVisible(bool visible);

public Q_SLOTS:
    /// CIP slot: refresh the 15 s window / session length.
    void onTriggerUpdateImpactData(qint64 currentTimestamp);
    /// CIP slot: refresh the DPS/HPS graphs.
    void onTriggerUpdateImpactGraphsValues(qint64 currentTimestamp);

Q_SIGNALS:
    /// Tip alias of CIP damageDataChanged.
    void damageValuesChanged();
    /// Tip alias of CIP healingDataChanged.
    void healingValuesChanged();
    /// Tip alias of CIP allTimeMaxChanged.
    void allTimeValuesChanged();
    /// Tip alias of CIP configurationChangedFromUserInput.
    void optionsChanged();
    // CIP graphValuesChanged / sessionLengthChanged come from the cip:: base.

private:
    using Record = std::array<qint64, ElementCount + 1>;

    /// Drops whole seconds since the last event or tick.
    void shiftRecords(qint64 nowMs);
    /// True if the data changed.
    bool computeData(Kind kind);
    void computeGraph(Kind kind);
    void checkAllTimeMax(Kind kind, qint64 hit);
    void emitDamageValuesChanged();
    void emitHealingValuesChanged();
    void emitAllTimeValuesChanged();
    void emitOptionsChanged();

    bool m_hasData = false;
    std::array<Data, 2> m_data;
    std::array<std::vector<Record>, 2> m_records;
    std::array<std::array<double, GraphPoints>, 2> m_graph{};
    std::array<qint64, 2> m_allTimeMax{};
    std::vector<double> m_weights15;
    std::vector<double> m_weights900;
    qint64 m_sessionStart = 0;
    qint64 m_lastTime = 0;
    qint64 m_lastGraphUpdate = 0;
    bool m_showSessionValues = false;
    std::array<bool, 2> m_gaugeVisible{ true, true };
    /// 0 until the configuration is read.
    std::array<double, 2> m_gaugeTarget{ 0.0, 0.0 };
    std::array<bool, 2> m_graphVisible{ true, true };
    bool m_damageTypesVisible = true;
};

/// Damage taken (CIP: tibia::cyclopedia::TDamageInputStorage, config "damageinputanalyser").
class TDamageInputStorage final : public cip::tibia::cyclopedia::TDamageInputStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    static constexpr int ElementCount = TImpactStorage::ElementCount;
    static constexpr qint64 WindowMs = 300000;
    static constexpr int ListedSources = 5;
    static constexpr int GraphPoints = TImpactStorage::GraphPoints;

    /// One hit taken.
    struct Hit
    {
        int amount = 0;
        int element = 0;
        QString source;
        qint64 timestamp = 0;
    };

    /// The damage of one source, per element (sorted by element).
    struct Source
    {
        QString name;
        int total = 0;
        std::vector<std::pair<int, int>> elements;

        explicit Source(const Hit& hit);
        void add(int element, int amount);
    };

    explicit TDamageInputStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("damageinputanalyser"); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    /// A hit of the damage-received impact tracking.
    void addHit(const Hit& hit);
    /// Refreshes the recent values (at most every 401 ms) and the graph
    /// (every 4 s, or 30 s with session values).
    void update(qint64 nowMs);
    void reset();

    qint64 total() const { return m_showSessionValues ? m_sessionTotal : m_windowTotal; }
    qint64 maxPerSecond() const { return m_maxPerSecond; }
    /// The listed sources (the five biggest) and the per-element sums, over
    /// the session or the last 5 minutes.
    const std::vector<Source>& sources() const { return m_showSessionValues ? m_listedSessionSources : m_windowSources; }
    const std::vector<std::pair<int, int>>& elements() const { return m_showSessionValues ? m_sessionElements : m_windowElements; }
    /// Damage taken per second, one point per 4 s (60 s with session values).
    std::vector<double> graph(double& maxY) const;

    bool showSessionValues() const { return m_showSessionValues; }
    void setShowSessionValues(bool show);
    bool showGraph() const { return m_showGraph; }
    void setShowGraph(bool show);
    bool showTypes() const { return m_showTypes; }
    void setShowTypes(bool show);
    bool showSources() const { return m_showSources; }
    void setShowSources(bool show);

Q_SIGNALS:
    /// Tip alias of CIP damageTakenDataChanged.
    void valuesChanged();
    /// Tip alias of CIP damageTakenGraphChanged.
    void graphChanged();
    /// Tip alias of CIP configurationChangedFromUserInput.
    void optionsChanged();

private:
    void emitValuesChanged();
    void emitGraphChanged();
    void emitOptionsChanged();

    bool m_showSessionValues = false;
    bool m_showGraph = true;
    bool m_showTypes = true;
    bool m_showSources = true;
    qint64 m_lastDataUpdate = 0;
    qint64 m_lastGraphUpdate = 0;
    int m_sessionTotal = 0;
    int m_windowTotal = 0;
    quint32 m_maxPerSecond = 0;
    /// Damage per second, the newest second last.
    std::vector<quint32> m_seconds;
    std::vector<Hit> m_hits;
    /// Sorted by element.
    std::vector<std::pair<int, int>> m_sessionElements;
    /// Sorted by name.
    std::vector<Source> m_sessionSources;
    std::vector<Source> m_listedSessionSources;
    std::vector<Source> m_windowSources;
    std::vector<std::pair<int, int>> m_windowElements;
    std::array<int, GraphPoints> m_graph{};
};

/// Hunting session analyser storage (CIP: tibia::cyclopedia::THuntingSessionStorage, config
/// "huntingsessionanalyser").
class THuntingSessionStorage final : public cip::tibia::cyclopedia::THuntingSessionStorage, public config::ICharacterConfiguration
{
    Q_OBJECT
public:
    using Kills = std::map<QString, int>;

    explicit THuntingSessionStorage(TProgressStorage* progress, QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("huntingsessionanalyser"); }
    QJsonObject configurationToJson() const override;
    void configurationFromJson(const QJsonObject& json) override;
    void resetConfiguration() override;

    void tick(qint64 nowMs);
    /// CIP .
    void reset();
    /// CIP .
    void addKill(const QString& name);

    /// CIP .
    qint64 sessionLengthMs() const { return m_nextTick - m_start; }
    /// CIP : the session start, from the wall clock.
    qint64 sessionStartSecs() const { return m_startEpochMs / 1000; }
    bool hasSession() const { return m_start != 0; }
    const Kills& kills() const { return m_kills; }
    TProgressStorage* progress() const { return m_progress; }

    bool showBaseXp() const { return m_showBaseXp; }
    void setShowBaseXp(bool show);

Q_SIGNALS:
    // CIP sessionLengthChanged / experienceChanged / killedMonstersChanged come from the cip:: base.
    /// Tip alias of CIP killedMonstersChanged.
    void killsChanged();
    /// Tip alias of CIP configurationChangedFromUserInput.
    void optionsChanged();

private:
    void emitKillsChanged();
    void emitOptionsChanged();
    QPointer<TProgressStorage> m_progress;
    qint64 m_start = 0;
    qint64 m_nextTick = 0;
    qint64 m_startEpochMs = 0;
    Kills m_kills;
    bool m_showBaseXp = false;
};

/// The party hunt analyser (CIP: tibia::cyclopedia::TPartyHuntStorage): the server's last
/// PartyHuntAnalyser message.
class TPartyHuntStorage final : public cip::tibia::cyclopedia::TPartyHuntStorage
{
    Q_OBJECT
public:
    struct Member
    {
        quint32 id = 0;
        QString name;
        bool active = false;
        qint64 loot = 0;
        qint64 supplies = 0;
        /// loot - supplies.
        qint64 balance = 0;
        qint64 damage = 0;
        qint64 healing = 0;
        bool operator==(const Member&) const = default;
    };

    explicit TPartyHuntStorage(QObject* parent = nullptr);

    /// The session started `elapsedSeconds` ago.
    void setSessionStart(quint32 elapsedSeconds);
    /// 0 market prices, 1 the leader's prices.
    void setLootType(int lootType);
    void setLeaderId(quint32 leaderId);
    /// The members with the names the server sent with them; an empty party forgets the names.
    void setMembers(const std::vector<Member>& members, const std::unordered_map<quint32, QString>& names);
    void clear();

    /// Seconds since the session started.
    quint32 sessionSeconds() const;
    quint32 leaderId() const { return m_leaderId; }
    int priceType() const { return m_lootType; }
    const std::vector<Member>& members() const { return m_members; }
    qint64 totalLoot() const { return m_totalLoot; }
    qint64 totalSupplies() const { return m_totalSupplies; }
    qint64 totalBalance() const { return m_totalBalance; }

Q_SIGNALS:
    /// Tip alias of CIP partyHuntDataChanged.
    void changed();

private:
    /// The member's name from the names, "unknown" without one.
    void resolveName(Member& member) const;
    /// Names, balances and totals of every member.
    void recompute();

    std::unordered_map<quint32, QString> m_names;
    std::vector<Member> m_members;
    int m_lootType = 0;
    qint64 m_sessionStart = 0;
    quint32 m_leaderId = 0;
    qint64 m_totalLoot = 0;
    qint64 m_totalSupplies = 0;
    qint64 m_totalBalance = 0;
};

/// Host shim over the current game session's analyser storages (CipSoft hands them to the
/// session's character configuration manager). The game window keeps one; widgets and the
/// product HUD read the current session's storages through it (the session's next frame
/// updater ticks them).
/// Until the pack ABI reaches the session, instance() stays for the pack.
class TAnalyserStorages final : public QObject
{
    Q_OBJECT
public:
    explicit TAnalyserStorages(QObject* parent = nullptr);
    ~TAnalyserStorages() override;

    static TAnalyserStorages* instance();
    static qint64 nowMs();

    /// The session's storages from now on; the last session's stay until the
    /// next one (the widgets open with a session close with its world).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    TProgressStorage* progress() { return m_progress.get(); }
    TItemPriceStorage* prices();
    TGainWasteStorage* gainWaste() { return m_gainWaste.get(); }
    TImpactStorage* impact() { return m_impact.get(); }
    TDamageInputStorage* damageInput() { return m_damageInput.get(); }
    THuntingSessionStorage* huntingSession() { return m_huntingSession.get(); }
    TPartyHuntStorage* partyHunt() { return m_partyHunt.get(); }
    cyclopedia::TItemTrackingStorage* itemTracking() { return m_itemTracking.get(); }
    quickloot::TQuickLootBlackWhitelistStorage* lootBlackWhitelist() { return m_lootBlackWhitelist.get(); }

    /// RESET_HUNTING_SESSION (CIP).
    void resetAll();
    /// Whether the storages hold a character's configuration (loaded by the
    /// config::TCharacterConfigurationManager; none after the game ends).
    bool hasCharacter() const { return m_hasCharacter; }

public Q_SLOTS:
    /// The character configuration manager read the character's storages.
    void onCharacterConfigurationLoaded();

Q_SIGNALS:
    /// The storages' publishGameAction (impact, item tracking), forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void sessionReset();
    /// The character's configurations were read (CIP TCharacterConfigurationManager).
    void configurationLoadedForCharacter();
    /// The own character's premium time changed.
    void premiumTimeChanged();

private:
    std::shared_ptr<TProgressStorage> m_progress;
    std::shared_ptr<TGainWasteStorage> m_gainWaste;
    std::shared_ptr<TImpactStorage> m_impact;
    std::shared_ptr<TDamageInputStorage> m_damageInput;
    std::shared_ptr<THuntingSessionStorage> m_huntingSession;
    std::shared_ptr<TPartyHuntStorage> m_partyHunt;
    std::shared_ptr<cyclopedia::TItemTrackingStorage> m_itemTracking;
    std::unique_ptr<quickloot::TQuickLootBlackWhitelistStorage> m_lootBlackWhitelist;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    std::vector<client::gamecore::Hook> m_hooks;
    bool m_hasCharacter = false;
};

}
}
