#pragma once

#include "client/gamecore.h"
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
#include <unordered_map>
#include <vector>

namespace tibia {
namespace analysers {

/// CIP (n): n linear weights w[i] = 2(i+1) / (n(n+1)), oldest first, summing to 1.
std::vector<double> linearWeights(int n);
/// CIP : a graph's y range follows its maximum with hysteresis.
double adjustGraphMaxY(double currentMaxY, double maxValue);

/// The character's analyser options (CIP: ICharacterConfigurationBase::name / toJson / fromJson),
/// one file per storage in <app data>/characterdata/<character id>/<name>.json.
class TCharacterConfiguration : public ::tibia::CipTyped
{
public:
    virtual ~TCharacterConfiguration() = default;
    virtual QString configurationName() const = 0;
    virtual QJsonObject toJson() const = 0;
    virtual void fromJson(const QJsonObject& json) = 0;
};

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
    /// CIP .
    double perHour() const { return perSecond() * 3600.0; }

private:
    qint64 m_total = 0;
    std::vector<double> m_bins;
    qint64 m_second = 0;
};

/// XP analyser storage (CIP: tibia::cyclopedia::TProgressStorage, config "xpanalyser").
class TProgressStorage final : public cip::tibia::cyclopedia::TProgressStorage, public TCharacterConfiguration
{
    Q_OBJECT
public:
    explicit TProgressStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("xpanalyser"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

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
    bool m_gaugeVisible = true;
    double m_gaugeTarget = 100.0;
    bool m_graphVisible = true;
    bool m_showBaseXp = false;
};

class TItemPriceStorage;

/// Loot and supply analyser storage (CIP: tibia::cyclopedia::TGainWasteStorage, config
/// "gainandwaste"). Items are added as they come; metrics and signals follow on the next tick.
class TGainWasteStorage final : public cip::tibia::cyclopedia::TGainWasteStorage, public TCharacterConfiguration
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
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

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
class TImpactStorage final : public cip::tibia::cyclopedia::TImpactStorage, public TCharacterConfiguration
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
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

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
    std::array<double, 2> m_gaugeTarget{ 100.0, 100.0 };
    std::array<bool, 2> m_graphVisible{ true, true };
    bool m_damageTypesVisible = true;
};

/// Damage taken (CIP: tibia::cyclopedia::TDamageInputStorage, config "damageinputanalyser").
class TDamageInputStorage final : public cip::tibia::cyclopedia::TDamageInputStorage, public TCharacterConfiguration
{
    Q_OBJECT
public:
    static constexpr int ElementCount = TImpactStorage::ElementCount;
    static constexpr qint64 WindowMs = 300000;
    static constexpr int ListedSources = 5;

    struct Source
    {
        QString name;
        qint64 total = 0;
        std::array<qint64, ElementCount> byElement{};
    };

    explicit TDamageInputStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("damageinputanalyser"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    /// CIP .
    void addDamage(qint64 amount, int element, const QString& source, qint64 nowMs);
    /// CIP vt15 .
    void tick(qint64 nowMs);
    /// CIP .
    void reset();

    qint64 total() const { return m_showSessionValues ? m_sessionTotal : m_windowTotal; }
    qint64 sessionTotal() const { return m_sessionTotal; }
    qint64 maxPerSecond() const { return m_maxPerSecond; }
    /// The listed sources (top 5, then the rest under an empty name) and the per-element sums,
    /// over the session or the last 5 minutes.
    const std::vector<Source>& sources() const { return m_listedSources; }
    const std::array<qint64, ElementCount>& elements() const { return m_showSessionValues ? m_sessionElements : m_windowElements; }
    const Source* source(const QString& name) const;
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
    struct Hit
    {
        qint64 timestamp = 0;
        qint64 amount = 0;
        int element = 0;
        QString source;
    };

    void rebuildLists();
    void emitValuesChanged();
    void emitGraphChanged();
    void emitOptionsChanged();

    std::vector<qint64> m_bins;
    qint64 m_second = 0;
    qint64 m_sessionTotal = 0;
    qint64 m_windowTotal = 0;
    qint64 m_maxPerSecond = 0;
    std::deque<Hit> m_hits;
    std::array<qint64, ElementCount> m_sessionElements{};
    std::array<qint64, ElementCount> m_windowElements{};
    std::map<QString, Source> m_sessionSources;
    std::vector<Source> m_listedSources;
    std::vector<double> m_weights15;
    std::vector<double> m_weights900;
    qint64 m_lastUpdate = 0;
    qint64 m_lastGraph = 0;
    bool m_dirty = false;
    bool m_showSessionValues = false;
    bool m_showGraph = true;
    bool m_showTypes = true;
    bool m_showSources = true;
};

/// Hunting session analyser storage (CIP: tibia::cyclopedia::THuntingSessionStorage, config
/// "huntingsessionanalyser").
class THuntingSessionStorage final : public cip::tibia::cyclopedia::THuntingSessionStorage, public TCharacterConfiguration
{
    Q_OBJECT
public:
    using Kills = std::map<QString, int>;

    explicit THuntingSessionStorage(TProgressStorage* progress, QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("huntingsessionanalyser"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

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

/// The session's analyser storages (CIP: the storage container behind session+0x40 vt0xb0) and
/// the engine hooks that feed them. One instance lives in main; widgets and the product HUD read
/// it through instance. A new game session starts from zero, as CIP makes new storages per
/// session; the options are character configuration, read when a character enters the world
/// and written when it leaves.
class TAnalyserStorages final : public QObject
{
    Q_OBJECT
public:
    /// The storages' tick interval. CIP's driver was not found; TImpactStorage only refreshes
    /// when more than 400 ms passed, so its ticks come faster than that.
    static constexpr int TickIntervalMs = 100;

    explicit TAnalyserStorages(QObject* parent = nullptr);
    ~TAnalyserStorages() override;

    static TAnalyserStorages* instance();
    static qint64 nowMs();

    TProgressStorage* progress() { return &m_progress; }
    TItemPriceStorage* prices();
    TGainWasteStorage* gainWaste() { return &m_gainWaste; }
    TImpactStorage* impact() { return &m_impact; }
    TDamageInputStorage* damageInput() { return &m_damageInput; }
    THuntingSessionStorage* huntingSession() { return &m_huntingSession; }
    TPartyHuntStorage* partyHunt() { return &m_partyHunt; }

    /// RESET_HUNTING_SESSION (CIP).
    void resetAll();
    /// The character whose configuration the storages use (0: none; saves the previous one).
    void setCharacter(quint32 characterId);
    void saveConfiguration();

Q_SIGNALS:
    void sessionReset();

private:
    void tick();
    QString configurationPath(const TCharacterConfiguration& configuration) const;

    TProgressStorage m_progress;
    TGainWasteStorage m_gainWaste;
    TImpactStorage m_impact;
    TDamageInputStorage m_damageInput;
    THuntingSessionStorage m_huntingSession;
    TPartyHuntStorage m_partyHunt;
    std::vector<client::gamecore::Hook> m_hooks;
    QTimer m_timer;
    quint32 m_characterId = 0;
};

}
}
