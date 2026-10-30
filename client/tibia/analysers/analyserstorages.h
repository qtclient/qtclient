#pragma once

#include "client/gamecore.h"
#include "shared/cip_typed.h"

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

namespace tibia::analysers {

/// CIP FUN_10102c340(n): n linear weights w[i] = 2(i+1) / (n(n+1)), oldest first, summing to 1.
std::vector<double> linearWeights(int n);
/// CIP 0x10101a290: a graph's y range follows its maximum with hysteresis.
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

/// One of TProgressStorage's TExperienceChangeTracker (CIP ctor in 0x100a6e0b0): the session total
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
    /// before the newest (open) bin (CIP 0x100a6dea0 / 0x100a6df40).
    double perSecond(int offsetSeconds = 0) const;
    /// CIP 0x100a6dff0.
    double perHour() const { return perSecond() * 3600.0; }

private:
    qint64 m_total = 0;
    std::vector<double> m_bins;
    qint64 m_second = 0;
};

/// XP analyser storage (CIP: tibia::cyclopedia::TProgressStorage, config "xpanalyser").
class TProgressStorage final : public QObject, public TCharacterConfiguration
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
    /// CIP 0x100a6e710: raw (base) and final experience of one XpChanged message.
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

    /// CIP 0x100a6e780: one point per minute, x from history[0].x, y the XP per hour then.
    void computeHistory(QVariantList& history, double& maxY) const;

Q_SIGNALS:
    void dataChanged();
    void wasReset();
    void optionsChanged();

private:
    TExperienceChangeTracker m_final;
    TExperienceChangeTracker m_base;
    bool m_gaugeVisible = true;
    double m_gaugeTarget = 100.0;
    bool m_graphVisible = true;
    bool m_showBaseXp = false;
};

/// Item values for the loot and waste analysers (CIP: tibia::cyclopedia::TItemPriceStorage,
/// vtable 0x102627860). The own values and per-item value sources are the cyclopedia's
/// (`cyclopedia::TItemInfoStorage`, slots 19 / 26).
class TItemPriceService final : public QObject
{
    Q_OBJECT
public:
    explicit TItemPriceService(QObject* parent = nullptr);

    /// CIP slot 20 (0x100a5aa70): a premium player's own value, else the item's source
    /// (NPC buy / market) falling back to the other one.
    qint64 lootPrice(uint16_t itemId) const;
    /// CIP slot 21 (0x100a5ab10): no own value; NPC sale / market by source, falling back
    /// to the other one.
    qint64 wastePrice(uint16_t itemId) const;
    void setMarketPrices(std::unordered_map<uint16_t, qint64> prices);

Q_SIGNALS:
    void pricesChanged();

private:
    /// CIP slot 15 (0x100a5a410): coins at face value, else the best NPC buy price.
    qint64 npcBuyPrice(uint16_t itemId) const;
    /// CIP slot 16 (0x100a5a560): the best NPC sale price.
    qint64 npcSalePrice(uint16_t itemId) const;
    /// CIP slot 17 (0x100a5a690): the world's market average.
    qint64 marketPrice(uint16_t itemId) const;
    /// CIP slot 26 (0x100a5aff0).
    static int valueSource(uint16_t itemId);

    std::unordered_map<uint16_t, qint64> m_market;
};

/// Loot and supply analyser storage (CIP: tibia::cyclopedia::TGainWasteStorage, ctor 0x100a63610,
/// config "gainandwaste").
class TGainWasteStorage final : public QObject, public TCharacterConfiguration
{
    Q_OBJECT
public:
    enum Side
    {
        Loot = 0,
        Waste = 1
    };

    struct Entry
    {
        qint64 price = 0;
        quint32 totalCount = 0;
        quint32 lastHourCount = 0;
        uint16_t itemId = 0;
        QString name;
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

    TGainWasteStorage(TItemPriceService* prices, QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("gainandwaste"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    /// CIP 0x100a63d00.
    void addLoot(uint16_t itemId, quint32 count, const QString& name, qint64 nowMs);
    /// CIP 0x100a64be0.
    void addWaste(uint16_t itemId, quint32 count, qint64 nowMs);
    /// CIP 0x100a64f20.
    void tick(qint64 nowMs);
    /// CIP 0x100a64b20 / 0x100a64e40.
    void reset(Side side);

    const Sums& sums(Side side) const { return m_sides[side].sums; }
    /// The loot / waste widget's items: last hour's counts (CIP 0x100a647d0), by item id.
    std::vector<Item> lastHourItems(Side side) const;
    /// The hunting session's items: session counts (CIP 0x100a64280).
    std::vector<Item> sessionItems(Side side) const;
    /// CIP 0x100a63f60: 60 points, each the weighted mean of 15 minute bins.
    std::vector<double> history(Side side, double& maxY) const;

    bool gaugeVisible(Side side) const { return m_sides[side].gaugeVisible; }
    void setGaugeVisible(Side side, bool visible);
    bool graphVisible(Side side) const { return m_sides[side].graphVisible; }
    void setGraphVisible(Side side, bool visible);
    qint64 gaugeTarget(Side side) const { return m_sides[side].gaugeTarget; }
    /// CIP 0x100a655e0 / 0x100a65650.
    void setGaugeTarget(Side side, qint64 target);

Q_SIGNALS:
    void itemsChanged(int side);
    void metricsChanged();
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
        qint64 gaugeTarget = 0;
    };

    /// CIP 0x100a64fb0.
    void computeMetrics(SideData& side, qint64 nowMs);
    /// CIP 0x100a65920.
    void refreshPrices();
    qint64 priceOf(Side side, uint16_t itemId) const;

    QPointer<TItemPriceService> m_prices;
    std::vector<double> m_weights;
    std::array<SideData, 2> m_sides;
    qint64 m_nextTick = 0;
    bool m_dirty = false;
};

/// Damage and healing output (CIP: tibia::cyclopedia::TImpactStorage, ctor 0x100a66aa0,
/// config "impactanalyser").
class TImpactStorage final : public QObject, public TCharacterConfiguration
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
    };

    explicit TImpactStorage(QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("impactanalyser"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    /// CIP vt19 (0x100a67040).
    void addDamage(qint64 primary, qint64 secondary, int element, qint64 nowMs);
    /// CIP vt20 (0x100a672f0).
    void addHealing(qint64 amount, qint64 nowMs);
    /// CIP vt16 (0x100a66d90).
    void tick(qint64 nowMs);
    /// CIP vt17 (0x100a66f80).
    void reset();
    /// impact_reset_all_time: the persisted strongest hits start over.
    void resetAllTimeMax();

    bool hasSession() const { return m_sessionStart != 0; }
    const Summary& session(Kind kind) const { return m_session[kind]; }
    /// The last 15 s (CIP 0x100a67ec0).
    const Summary& window(Kind kind) const { return m_window[kind]; }
    /// The 15 s linear-weighted per second value.
    double perSecond(Kind kind) const { return m_perSecond[kind]; }
    double maxPerSecond(Kind kind) const { return m_maxPerSecond[kind]; }
    /// Session total over the session's length (at least 1 s), per second.
    double sessionPerSecond(Kind kind) const;
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

Q_SIGNALS:
    void damageValuesChanged();
    void healingValuesChanged();
    void allTimeValuesChanged();
    void graphValuesChanged();
    void sessionLengthChanged();
    void optionsChanged();

private:
    using Record = std::array<qint64, ElementCount + 1>;

    /// CIP 0x100a67ca0: drops whole seconds since the last event or tick.
    void shiftRecords(qint64 nowMs);
    /// CIP 0x100a67ec0: true if the window changed.
    bool computeWindow(Kind kind);
    /// CIP 0x100a684b0.
    void computeGraph(Kind kind);
    void checkAllTimeMax(Kind kind, qint64 hit);

    std::array<std::vector<Record>, 2> m_records;
    std::array<Summary, 2> m_session;
    std::array<Summary, 2> m_window;
    std::array<double, 2> m_perSecond{};
    std::array<double, 2> m_maxPerSecond{};
    std::array<std::array<double, GraphPoints>, 2> m_graph{};
    std::array<qint64, 2> m_allTimeMax{};
    std::vector<double> m_weights15;
    std::vector<double> m_weights900;
    qint64 m_sessionStart = 0;
    qint64 m_lastTime = 0;
    qint64 m_lastDataUpdate = 0;
    qint64 m_lastGraphUpdate = 0;
    bool m_showSessionValues = false;
    std::array<bool, 2> m_gaugeVisible{ true, true };
    std::array<double, 2> m_gaugeTarget{ 100.0, 100.0 };
    std::array<bool, 2> m_graphVisible{ true, true };
    bool m_damageTypesVisible = true;
};

/// Damage taken (CIP: tibia::cyclopedia::TDamageInputStorage, config "damageinputanalyser").
class TDamageInputStorage final : public QObject, public TCharacterConfiguration
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

    /// CIP 0x100a6a480.
    void addDamage(qint64 amount, int element, const QString& source, qint64 nowMs);
    /// CIP vt15 (0x100a693e0).
    void tick(qint64 nowMs);
    /// CIP 0x100a6b250.
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
    void valuesChanged();
    void graphChanged();
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
class THuntingSessionStorage final : public QObject, public TCharacterConfiguration
{
    Q_OBJECT
public:
    struct CaseInsensitiveLess
    {
        bool operator()(const QString& a, const QString& b) const { return a.compare(b, Qt::CaseInsensitive) < 0; }
    };
    using Kills = std::map<QString, int, CaseInsensitiveLess>;

    explicit THuntingSessionStorage(TProgressStorage* progress, QObject* parent = nullptr);

    QString configurationName() const override { return QStringLiteral("huntingsessionanalyser"); }
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    void tick(qint64 nowMs);
    /// CIP 0x100a6f6e0.
    void reset();
    /// CIP 0x100a6f600.
    void addKill(const QString& name);

    /// CIP 0x100a6f770.
    qint64 sessionLengthMs() const { return m_nextTick - m_start; }
    /// CIP 0x100a6f780: the session start, from the wall clock.
    qint64 sessionStartSecs() const { return m_startEpochMs / 1000; }
    bool hasSession() const { return m_start != 0; }
    const Kills& kills() const { return m_kills; }
    TProgressStorage* progress() const { return m_progress; }

    bool showBaseXp() const { return m_showBaseXp; }
    void setShowBaseXp(bool show);

Q_SIGNALS:
    void sessionLengthChanged();
    void experienceChanged();
    void killsChanged();
    void optionsChanged();

private:
    QPointer<TProgressStorage> m_progress;
    qint64 m_start = 0;
    qint64 m_nextTick = 0;
    qint64 m_startEpochMs = 0;
    Kills m_kills;
    bool m_showBaseXp = false;
};

/// The party hunt analyser (CIP: tibia::cyclopedia::TPartyHuntStorage): the server's last
/// PartyHuntAnalyser message.
class TPartyHuntStorage final : public QObject
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
        qint64 damage = 0;
        qint64 healing = 0;
    };

    explicit TPartyHuntStorage(QObject* parent = nullptr);

    void update(quint32 sessionSeconds, quint32 leaderId, int priceType, std::vector<Member> members);
    void clear();

    quint32 sessionSeconds() const { return m_sessionSeconds; }
    quint32 leaderId() const { return m_leaderId; }
    int priceType() const { return m_priceType; }
    const std::vector<Member>& members() const { return m_members; }

Q_SIGNALS:
    void changed();

private:
    quint32 m_sessionSeconds = 0;
    quint32 m_leaderId = 0;
    int m_priceType = 0;
    std::vector<Member> m_members;
    std::unordered_map<quint32, QString> m_names;
};

/// The session's analyser storages (CIP: the storage container behind session+0x40 vt0xb0) and
/// the engine hooks that feed them. One instance lives in main; widgets and the product HUD read
/// it through instance(). A new game session starts from zero, as CIP makes new storages per
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
    TItemPriceService* prices() { return &m_prices; }
    TGainWasteStorage* gainWaste() { return &m_gainWaste; }
    TImpactStorage* impact() { return &m_impact; }
    TDamageInputStorage* damageInput() { return &m_damageInput; }
    THuntingSessionStorage* huntingSession() { return &m_huntingSession; }
    TPartyHuntStorage* partyHunt() { return &m_partyHunt; }

    /// RESET_HUNTING_SESSION (CIP 0x100e66b20).
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
    TItemPriceService m_prices;
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
