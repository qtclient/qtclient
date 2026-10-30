#include "analysers/analyserstorages.h"

#include <algorithm>
#include <climits>

namespace tibia::analysers {

namespace {

constexpr auto KeyShowSessionValues = "showSessionValues";
constexpr auto KeyDpsGaugeVisible = "desiredDpsGaugeVisible";
constexpr auto KeyDpsGaugeTarget = "dpsGaugeTargetValue";
constexpr auto KeyHpsGaugeVisible = "desiredHpsGaugeVisible";
constexpr auto KeyHpsGaugeTarget = "hpsGaugeTargetValue";
constexpr auto KeyDpsGraphVisible = "desiredDpsGraphVisible";
constexpr auto KeyHpsGraphVisible = "desiredHpsGraphVisible";
constexpr auto KeyDamageTypesVisible = "desiredDamageTypesVisible";
constexpr auto KeyMaxDamage = "maxDamageImpact";
constexpr auto KeyMaxHealing = "maxHealingImpact";

constexpr auto KeyShowDamageGraph = "showDamageGraph";
constexpr auto KeyShowDamageTypes = "showDamageTypes";
constexpr auto KeyShowDamageSources = "showDamageSources";

/// The same FUN_100973980 clamp as experienceGaugeTargetValue.
constexpr double MinGaugeTarget = 0.1;
constexpr double MaxGaugeTarget = 999999999.9;
constexpr qint64 DataRefreshMs = 401;
constexpr qint64 GraphRefreshMs = 4000;
constexpr qint64 SessionGraphRefreshMs = 30000;
constexpr int GraphStepSeconds = 4;
constexpr int SessionGraphStepSeconds = 60;
constexpr int BinCount = TImpactStorage::RecordCount;

int clampElement(const int element)
{
    return element >= 0 && element < TImpactStorage::ElementCount ? element : 0;
}

}

TImpactStorage::TImpactStorage(QObject* parent)
    : QObject(parent)
    , m_weights15(linearWeights(15))
    , m_weights900(linearWeights(900))
{
    for (auto& records : m_records)
        records.assign(RecordCount, Record{});
}

QJsonObject TImpactStorage::toJson() const
{
    return { { QLatin1String(KeyShowSessionValues), m_showSessionValues },
             { QLatin1String(KeyDpsGaugeVisible), m_gaugeVisible[Damage] },
             { QLatin1String(KeyDpsGaugeTarget), m_gaugeTarget[Damage] },
             { QLatin1String(KeyHpsGaugeVisible), m_gaugeVisible[Healing] },
             { QLatin1String(KeyHpsGaugeTarget), m_gaugeTarget[Healing] },
             { QLatin1String(KeyDpsGraphVisible), m_graphVisible[Damage] },
             { QLatin1String(KeyHpsGraphVisible), m_graphVisible[Healing] },
             { QLatin1String(KeyDamageTypesVisible), m_damageTypesVisible },
             { QLatin1String(KeyMaxDamage), static_cast<int>(std::min<qint64>(m_allTimeMax[Damage], INT_MAX)) },
             { QLatin1String(KeyMaxHealing), static_cast<int>(std::min<qint64>(m_allTimeMax[Healing], INT_MAX)) } };
}

void TImpactStorage::fromJson(const QJsonObject& json)
{
    const auto target = [&json](const char* key) {
        return std::clamp(json.value(QLatin1String(key)).toDouble(100.0), MinGaugeTarget, MaxGaugeTarget);
    };
    m_showSessionValues = json.value(QLatin1String(KeyShowSessionValues)).toBool(false);
    m_gaugeVisible[Damage] = json.value(QLatin1String(KeyDpsGaugeVisible)).toBool(true);
    m_gaugeTarget[Damage] = target(KeyDpsGaugeTarget);
    m_gaugeVisible[Healing] = json.value(QLatin1String(KeyHpsGaugeVisible)).toBool(true);
    m_gaugeTarget[Healing] = target(KeyHpsGaugeTarget);
    m_graphVisible[Damage] = json.value(QLatin1String(KeyDpsGraphVisible)).toBool(true);
    m_graphVisible[Healing] = json.value(QLatin1String(KeyHpsGraphVisible)).toBool(true);
    m_damageTypesVisible = json.value(QLatin1String(KeyDamageTypesVisible)).toBool(true);
    m_allTimeMax[Damage] = std::max(0, json.value(QLatin1String(KeyMaxDamage)).toInt(0));
    m_allTimeMax[Healing] = std::max(0, json.value(QLatin1String(KeyMaxHealing)).toInt(0));
    Q_EMIT optionsChanged();
    Q_EMIT allTimeValuesChanged();
}

void TImpactStorage::shiftRecords(const qint64 nowMs)
{
    const qint64 shift = nowMs / 1000 - m_lastTime / 1000;
    if (shift > 0) {
        for (auto& records : m_records) {
            if (shift >= RecordCount) {
                std::fill(records.begin(), records.end(), Record{});
            } else {
                std::move(records.begin() + shift, records.end(), records.begin());
                std::fill(records.end() - shift, records.end(), Record{});
            }
        }
    }
    m_lastTime = nowMs;
}

void TImpactStorage::addDamage(const qint64 primary, const qint64 secondary, const int element, const qint64 nowMs)
{
    shiftRecords(nowMs);
    const int e = clampElement(element);
    Record& record = m_records[Damage].back();
    record[0] += primary + secondary;
    record[1] += primary;
    record[1 + e] += secondary;
    if (m_sessionStart == 0)
        m_sessionStart = nowMs;
    Summary& session = m_session[Damage];
    session.total += primary + secondary;
    session.byElement[0] += primary;
    session.byElement[static_cast<size_t>(e)] += secondary;
    computeWindow(Damage);
    Q_EMIT damageValuesChanged();
    checkAllTimeMax(Damage, primary + secondary);
}

void TImpactStorage::addHealing(const qint64 amount, const qint64 nowMs)
{
    constexpr int HealingElement = 7;
    shiftRecords(nowMs);
    Record& record = m_records[Healing].back();
    record[0] += amount;
    record[1 + HealingElement] += amount;
    if (m_sessionStart == 0)
        m_sessionStart = nowMs;
    Summary& session = m_session[Healing];
    session.total += amount;
    session.byElement[HealingElement] += amount;
    computeWindow(Healing);
    Q_EMIT healingValuesChanged();
    checkAllTimeMax(Healing, amount);
}

void TImpactStorage::checkAllTimeMax(const Kind kind, const qint64 hit)
{
    if (hit <= m_allTimeMax[kind])
        return;
    m_allTimeMax[kind] = hit;
    Q_EMIT allTimeValuesChanged();
}

bool TImpactStorage::computeWindow(const Kind kind)
{
    const std::vector<Record>& records = m_records[kind];
    Summary window;
    const size_t first = records.size() - m_weights15.size();
    double perSecond = 0.0;
    for (size_t k = 0; k < m_weights15.size(); ++k) {
        const Record& record = records[first + k];
        window.total += record[0];
        for (int e = 0; e < ElementCount; ++e)
            window.byElement[static_cast<size_t>(e)] += record[static_cast<size_t>(1 + e)];
        perSecond += m_weights15[k] * static_cast<double>(record[0]);
    }
    const bool changed = window.total != m_window[kind].total || window.byElement != m_window[kind].byElement || perSecond != m_perSecond[kind];
    m_window[kind] = window;
    m_perSecond[kind] = perSecond;
    m_maxPerSecond[kind] = std::max(m_maxPerSecond[kind], perSecond);
    return changed;
}

void TImpactStorage::computeGraph(const Kind kind)
{
    const std::vector<double>& weights = m_showSessionValues ? m_weights900 : m_weights15;
    const int step = m_showSessionValues ? SessionGraphStepSeconds : GraphStepSeconds;
    const std::vector<Record>& records = m_records[kind];
    for (int i = 0; i < GraphPoints; ++i) {
        const qint64 end = static_cast<qint64>(records.size()) - static_cast<qint64>(GraphPoints - 1 - i) * step;
        const qint64 first = end - static_cast<qint64>(weights.size());
        double value = 0.0;
        if (first >= 0) {
            for (size_t k = 0; k < weights.size(); ++k)
                value += weights[k] * static_cast<double>(records[static_cast<size_t>(first) + k][0]);
        }
        m_graph[kind][static_cast<size_t>(i)] = value;
    }
}

double TImpactStorage::sessionPerSecond(const Kind kind) const
{
    if (m_sessionStart == 0)
        return 0.0;
    const qint64 length = std::max<qint64>(1000, m_lastTime - m_sessionStart);
    return static_cast<double>(m_session[kind].total) / static_cast<double>(length) * 1000.0;
}

void TImpactStorage::tick(const qint64 nowMs)
{
    if ((m_lastTime == 0 || m_lastTime + DataRefreshMs < nowMs) && hasSession()) {
        shiftRecords(nowMs);
        if (computeWindow(Damage))
            Q_EMIT damageValuesChanged();
        if (computeWindow(Healing))
            Q_EMIT healingValuesChanged();
        Q_EMIT sessionLengthChanged();
    }
    const bool due = m_showSessionValues ? (m_lastGraphUpdate == 0 || nowMs > m_lastGraphUpdate + SessionGraphRefreshMs)
                                         : (m_lastGraphUpdate == 0 || nowMs > m_lastGraphUpdate + GraphRefreshMs);
    if (!due || !hasSession())
        return;
    shiftRecords(nowMs);
    computeGraph(Damage);
    computeGraph(Healing);
    m_lastGraphUpdate = nowMs;
    Q_EMIT sessionLengthChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::reset()
{
    for (auto& records : m_records)
        std::fill(records.begin(), records.end(), Record{});
    m_session = {};
    m_window = {};
    m_perSecond = {};
    m_maxPerSecond = {};
    for (auto& graph : m_graph)
        graph.fill(0.0);
    m_sessionStart = 0;
    m_lastGraphUpdate = 0;
    Q_EMIT damageValuesChanged();
    Q_EMIT healingValuesChanged();
    Q_EMIT sessionLengthChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::resetAllTimeMax()
{
    m_allTimeMax = {};
    Q_EMIT allTimeValuesChanged();
}

void TImpactStorage::setShowSessionValues(const bool show)
{
    if (m_showSessionValues == show)
        return;
    m_showSessionValues = show;
    if (hasSession()) {
        computeGraph(Damage);
        computeGraph(Healing);
    }
    Q_EMIT optionsChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::setGaugeVisible(const Kind kind, const bool visible)
{
    if (m_gaugeVisible[kind] == visible)
        return;
    m_gaugeVisible[kind] = visible;
    Q_EMIT optionsChanged();
}

void TImpactStorage::setGaugeTarget(const Kind kind, const double target)
{
    const double clamped = std::clamp(target, MinGaugeTarget, MaxGaugeTarget);
    if (m_gaugeTarget[kind] == clamped)
        return;
    m_gaugeTarget[kind] = clamped;
    Q_EMIT optionsChanged();
}

void TImpactStorage::setGraphVisible(const Kind kind, const bool visible)
{
    if (m_graphVisible[kind] == visible)
        return;
    m_graphVisible[kind] = visible;
    Q_EMIT optionsChanged();
}

void TImpactStorage::setDamageTypesVisible(const bool visible)
{
    if (m_damageTypesVisible == visible)
        return;
    m_damageTypesVisible = visible;
    Q_EMIT optionsChanged();
}

TDamageInputStorage::TDamageInputStorage(QObject* parent)
    : QObject(parent)
    , m_bins(BinCount, 0)
    , m_weights15(linearWeights(15))
    , m_weights900(linearWeights(900))
{}

QJsonObject TDamageInputStorage::toJson() const
{
    return { { QLatin1String(KeyShowSessionValues), m_showSessionValues },
             { QLatin1String(KeyShowDamageGraph), m_showGraph },
             { QLatin1String(KeyShowDamageTypes), m_showTypes },
             { QLatin1String(KeyShowDamageSources), m_showSources } };
}

void TDamageInputStorage::fromJson(const QJsonObject& json)
{
    m_showSessionValues = json.value(QLatin1String(KeyShowSessionValues)).toBool(false);
    m_showGraph = json.value(QLatin1String(KeyShowDamageGraph)).toBool(true);
    m_showTypes = json.value(QLatin1String(KeyShowDamageTypes)).toBool(true);
    m_showSources = json.value(QLatin1String(KeyShowDamageSources)).toBool(true);
    rebuildLists();
    Q_EMIT optionsChanged();
}

void TDamageInputStorage::addDamage(const qint64 amount, const int element, const QString& source, const qint64 nowMs)
{
    const qint64 second = nowMs / 1000;
    if (second > m_second) {
        const qint64 shift = second - m_second;
        if (shift >= BinCount) {
            std::fill(m_bins.begin(), m_bins.end(), 0);
        } else {
            std::move(m_bins.begin() + shift, m_bins.end(), m_bins.begin());
            std::fill(m_bins.end() - shift, m_bins.end(), 0);
        }
        m_second = second;
    }
    const int e = clampElement(element);
    m_sessionTotal += amount;
    m_bins.back() += amount;
    m_maxPerSecond = std::max(m_maxPerSecond, m_bins.back());
    m_sessionElements[static_cast<size_t>(e)] += amount;
    Source& sessionSource = m_sessionSources[source];
    sessionSource.name = source;
    sessionSource.total += amount;
    sessionSource.byElement[static_cast<size_t>(e)] += amount;
    m_hits.push_back({ nowMs, amount, e, source });
    m_dirty = true;
}

void TDamageInputStorage::tick(const qint64 nowMs)
{
    if (nowMs <= m_lastUpdate + DataRefreshMs)
        return;
    m_lastUpdate = nowMs;
    bool expired = false;
    while (!m_hits.empty() && m_hits.front().timestamp < nowMs - WindowMs) {
        m_hits.pop_front();
        expired = true;
    }
    if (m_dirty || expired) {
        m_dirty = false;
        rebuildLists();
        Q_EMIT valuesChanged();
    }
    const qint64 graphInterval = m_showSessionValues ? SessionGraphRefreshMs : GraphRefreshMs;
    if (m_sessionTotal > 0 && (m_lastGraph == 0 || nowMs > m_lastGraph + graphInterval)) {
        m_lastGraph = nowMs;
        Q_EMIT graphChanged();
    }
}

void TDamageInputStorage::rebuildLists()
{
    m_windowTotal = 0;
    m_windowElements = {};
    std::map<QString, Source> windowSources;
    for (const Hit& hit : m_hits) {
        m_windowTotal += hit.amount;
        m_windowElements[static_cast<size_t>(hit.element)] += hit.amount;
        Source& source = windowSources[hit.source];
        source.name = hit.source;
        source.total += hit.amount;
        source.byElement[static_cast<size_t>(hit.element)] += hit.amount;
    }
    const std::map<QString, Source>& from = m_showSessionValues ? m_sessionSources : windowSources;
    std::vector<Source> sorted;
    sorted.reserve(from.size());
    for (const auto& [name, source] : from)
        sorted.push_back(source);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Source& a, const Source& b) { return a.total > b.total; });
    if (sorted.size() > ListedSources) {
        Source others;
        for (size_t i = ListedSources; i < sorted.size(); ++i) {
            others.total += sorted[i].total;
            for (int e = 0; e < ElementCount; ++e)
                others.byElement[static_cast<size_t>(e)] += sorted[i].byElement[static_cast<size_t>(e)];
        }
        sorted.resize(ListedSources);
        sorted.push_back(std::move(others));
    }
    m_listedSources = std::move(sorted);
}

const TDamageInputStorage::Source* TDamageInputStorage::source(const QString& name) const
{
    for (const Source& source : m_listedSources) {
        if (source.name == name)
            return &source;
    }
    return nullptr;
}

std::vector<double> TDamageInputStorage::graph(double& maxY) const
{
    const std::vector<double>& weights = m_showSessionValues ? m_weights900 : m_weights15;
    const int step = m_showSessionValues ? SessionGraphStepSeconds : GraphStepSeconds;
    constexpr int Points = TImpactStorage::GraphPoints;
    std::vector<double> points(Points, 0.0);
    double maxValue = 0.0;
    for (int i = 0; i < Points; ++i) {
        const qint64 end = static_cast<qint64>(m_bins.size()) - static_cast<qint64>(Points - 1 - i) * step;
        const qint64 first = end - static_cast<qint64>(weights.size());
        double value = 0.0;
        if (first >= 0) {
            for (size_t k = 0; k < weights.size(); ++k)
                value += weights[k] * static_cast<double>(m_bins[static_cast<size_t>(first) + k]);
        }
        points[static_cast<size_t>(i)] = value;
        maxValue = std::max(maxValue, value);
    }
    maxY = adjustGraphMaxY(maxY, maxValue);
    return points;
}

void TDamageInputStorage::reset()
{
    m_sessionTotal = 0;
    m_windowTotal = 0;
    m_maxPerSecond = 0;
    std::fill(m_bins.begin(), m_bins.end(), 0);
    m_hits.clear();
    m_sessionElements = {};
    m_windowElements = {};
    m_sessionSources.clear();
    m_listedSources.clear();
    m_lastGraph = 0;
    Q_EMIT valuesChanged();
    Q_EMIT graphChanged();
}

void TDamageInputStorage::setShowSessionValues(const bool show)
{
    if (m_showSessionValues == show)
        return;
    m_showSessionValues = show;
    rebuildLists();
    Q_EMIT optionsChanged();
    Q_EMIT valuesChanged();
    Q_EMIT graphChanged();
}

void TDamageInputStorage::setShowGraph(const bool show)
{
    if (m_showGraph == show)
        return;
    m_showGraph = show;
    Q_EMIT optionsChanged();
}

void TDamageInputStorage::setShowTypes(const bool show)
{
    if (m_showTypes == show)
        return;
    m_showTypes = show;
    Q_EMIT optionsChanged();
}

void TDamageInputStorage::setShowSources(const bool show)
{
    if (m_showSources == show)
        return;
    m_showSources = show;
    Q_EMIT optionsChanged();
}

}
