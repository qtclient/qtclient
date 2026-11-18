#include "analysers/analyserstorages.h"
#include "input/gameactionhandlers.h"

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

/// The same clamp as experienceGaugeTargetValue.
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
    : cip::tibia::cyclopedia::TImpactStorage(parent)
    , m_weights15(linearWeights(15))
    , m_weights900(linearWeights(900))
{
    for (auto& records : m_records)
        records.assign(RecordCount, Record{});
}

void TImpactStorage::emitDamageValuesChanged()
{
    Q_EMIT damageValuesChanged();
    Q_EMIT damageDataChanged();
}

void TImpactStorage::emitHealingValuesChanged()
{
    Q_EMIT healingValuesChanged();
    Q_EMIT healingDataChanged();
}

void TImpactStorage::emitAllTimeValuesChanged()
{
    Q_EMIT allTimeValuesChanged();
    Q_EMIT allTimeMaxChanged();
}

void TImpactStorage::emitOptionsChanged()
{
    Q_EMIT optionsChanged();
    Q_EMIT configurationChangedFromUserInput();
}

QJsonObject TImpactStorage::configurationToJson() const
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

void TImpactStorage::configurationFromJson(const QJsonObject& json)
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

void TImpactStorage::resetConfiguration() { configurationFromJson({}); }

void TImpactStorage::shiftRecords(const qint64 nowMs)
{
    const qint64 shift = nowMs / 1000 - m_lastTime / 1000;
    for (auto& records : m_records) {
        if (shift < 0 || shift >= static_cast<qint64>(records.size())) {
            std::fill(records.begin(), records.end(), Record{});
        } else if (shift > 0) {
            std::move(records.begin() + shift, records.end(), records.begin());
            std::fill(records.end() - shift, records.end(), Record{});
        }
    }
    m_lastTime = nowMs;
}

qint64 TImpactStorage::sessionLengthMs() const { return std::max<qint64>(1000, m_lastTime - m_sessionStart); }

qint64 TImpactStorage::windowLengthMs() const { return std::min<qint64>(15000, sessionLengthMs()); }

void TImpactStorage::addDamage(const qint64 primary, const qint64 secondary, const int element, const qint64 nowMs)
{
    shiftRecords(nowMs);
    const qint64 amount = primary + secondary;
    Record& record = m_records[Damage].back();
    record[0] += amount;
    record[1] += primary;
    if (element >= 0 && element < ElementCount)
        record[static_cast<size_t>(1 + element)] += secondary;
    if (m_sessionStart == 0)
        m_sessionStart = nowMs;
    Summary& session = m_data[Damage].session;
    session.total += amount;
    session.byElement[0] += primary;
    if (element >= 0 && element < ElementCount)
        session.byElement[static_cast<size_t>(element)] += secondary;
    computeData(Damage);
    m_hasData = true;
    Q_EMIT sessionLengthChanged();
    emitDamageValuesChanged();
    checkAllTimeMax(Damage, amount);
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
    Summary& session = m_data[Healing].session;
    session.total += amount;
    session.byElement[HealingElement] += amount;
    computeData(Healing);
    m_hasData = true;
    Q_EMIT sessionLengthChanged();
    emitHealingValuesChanged();
    checkAllTimeMax(Healing, amount);
}

void TImpactStorage::checkAllTimeMax(const Kind kind, const qint64 hit)
{
    // A new strongest hit or heal takes an automatic screenshot.
    constexpr int ScreenshotParameter[2] = { 50, 51 };
    if (static_cast<quint64>(m_allTimeMax[kind]) >= static_cast<quint64>(hit))
        return;
    m_allTimeMax[kind] = hit;
    emitAllTimeValuesChanged();
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionTakeAutomaticScreenshot>(ScreenshotParameter[kind]));
}

bool TImpactStorage::computeData(const Kind kind)
{
    const std::vector<Record>& records = m_records[kind];
    const Data& current = m_data[kind];
    Data data = current;
    data.lastHour = {};
    data.window = {};
    const auto add = [](Summary& summary, const Record& record) {
        summary.total += record[0];
        for (size_t e = 0; e < ElementCount; ++e)
            summary.byElement[e] += record[1 + e];
    };
    // The last hour's and the last 15 seconds' sums, newest first.
    const size_t size = records.size();
    const size_t hourStart = size > 3600 ? size - 3600 : 0;
    const size_t windowStart = size > 15 ? size - 15 : 0;
    for (size_t i = size; i > hourStart; --i) {
        const Record& record = records[i - 1];
        add(data.lastHour, record);
        if (i - 1 >= windowStart)
            add(data.window, record);
    }
    data.sessionPerSecond = static_cast<double>(static_cast<quint64>(data.session.total)) / static_cast<double>(sessionLengthMs()) * 1000.0;
    data.maxSessionPerSecond = std::max(data.maxSessionPerSecond, data.sessionPerSecond);
    double perSecond = 0.0;
    if (size >= m_weights15.size()) {
        const size_t first = size - m_weights15.size();
        for (size_t k = 0; k < m_weights15.size(); ++k)
            perSecond += static_cast<double>(static_cast<quint64>(records[first + k][0])) * m_weights15[k];
    }
    data.perSecond = perSecond;
    data.maxPerSecond = std::max(data.maxPerSecond, perSecond);
    const bool changed = !(data == current);
    if (changed)
        m_data[kind] = data;
    return changed;
}

void TImpactStorage::computeGraph(const Kind kind)
{
    // One point per 4 s of the 15 s rate, or per minute of the 15 min rate with session values.
    const std::vector<double>& weights = m_showSessionValues ? m_weights900 : m_weights15;
    const qint64 step = m_showSessionValues ? SessionGraphStepSeconds : GraphStepSeconds;
    const std::vector<Record>& records = m_records[kind];
    for (int i = 0; i < GraphPoints; ++i) {
        const qint64 end = static_cast<qint64>(records.size()) + static_cast<qint64>(i - (GraphPoints - 1)) * step;
        const qint64 first = end - static_cast<qint64>(weights.size());
        double value = 0.0;
        if (first >= 0) {
            for (size_t k = 0; k < weights.size(); ++k)
                value += static_cast<double>(static_cast<quint64>(records[static_cast<size_t>(first) + k][0])) * weights[k];
        }
        m_graph[kind][static_cast<size_t>(i)] = value;
    }
}

void TImpactStorage::onTriggerUpdateImpactData(const qint64 currentTimestamp)
{
    shiftRecords(currentTimestamp);
    if (computeData(Damage))
        emitDamageValuesChanged();
    if (computeData(Healing))
        emitHealingValuesChanged();
    Q_EMIT sessionLengthChanged();
}

void TImpactStorage::onTriggerUpdateImpactGraphsValues(const qint64 currentTimestamp)
{
    shiftRecords(currentTimestamp);
    computeGraph(Damage);
    computeGraph(Healing);
    m_lastGraphUpdate = currentTimestamp;
    Q_EMIT sessionLengthChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::tick(const qint64 nowMs)
{
    if ((m_lastTime == 0 || m_lastTime + DataRefreshMs < nowMs) && hasSession())
        onTriggerUpdateImpactData(nowMs);
    const bool graphDue = m_lastGraphUpdate == 0 || (m_showSessionValues ? nowMs > m_lastGraphUpdate + SessionGraphRefreshMs
                                                                         : nowMs > m_lastGraphUpdate + GraphRefreshMs);
    if (graphDue && hasSession())
        onTriggerUpdateImpactGraphsValues(nowMs);
}

void TImpactStorage::reset()
{
    m_data = {};
    m_hasData = false;
    for (auto& graph : m_graph)
        graph.fill(0.0);
    m_sessionStart = 0;
    m_lastTime = 0;
    m_lastGraphUpdate = 0;
    emitDamageValuesChanged();
    emitHealingValuesChanged();
    Q_EMIT sessionLengthChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::resetAllTimeMax()
{
    m_allTimeMax = {};
    emitAllTimeValuesChanged();
}

void TImpactStorage::setShowSessionValues(const bool show)
{
    if (m_showSessionValues == show)
        return;
    m_showSessionValues = show;
    m_lastGraphUpdate = 0;
    for (auto& graph : m_graph)
        graph.fill(0.0);
    emitOptionsChanged();
    emitDamageValuesChanged();
    Q_EMIT graphValuesChanged();
}

void TImpactStorage::setGaugeVisible(const Kind kind, const bool visible)
{
    if (m_gaugeVisible[kind] == visible)
        return;
    m_gaugeVisible[kind] = visible;
    emitOptionsChanged();
}

void TImpactStorage::setGaugeTarget(const Kind kind, const double target)
{
    if (m_gaugeTarget[kind] == target)
        return;
    m_gaugeTarget[kind] = target;
    emitOptionsChanged();
}

void TImpactStorage::setGraphVisible(const Kind kind, const bool visible)
{
    if (m_graphVisible[kind] == visible)
        return;
    m_graphVisible[kind] = visible;
    emitOptionsChanged();
}

void TImpactStorage::setDamageTypesVisible(const bool visible)
{
    if (m_damageTypesVisible == visible)
        return;
    m_damageTypesVisible = visible;
    emitOptionsChanged();
}

TDamageInputStorage::Source::Source(const Hit& hit)
    : name(hit.source)
    , total(hit.amount)
{
    add(hit.element, hit.amount);
}

void TDamageInputStorage::Source::add(const int element, const int amount)
{
    auto it = std::lower_bound(elements.begin(), elements.end(), element,
                               [](const std::pair<int, int>& entry, const int key) { return entry.first < key; });
    if (it == elements.end() || element < it->first)
        it = elements.insert(it, { element, 0 });
    it->second += amount;
}

namespace {

/// The biggest source first; equal totals by name.
bool biggerSource(const TDamageInputStorage::Source& a, const TDamageInputStorage::Source& b)
{
    if (a.total == b.total)
        return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
    return b.total < a.total;
}

const std::vector<double>& sessionGraphWeights()
{
    static const std::vector<double> weights = linearWeights(900);
    return weights;
}

const std::vector<double>& recentGraphWeights()
{
    static const std::vector<double> weights = linearWeights(15);
    return weights;
}

}

TDamageInputStorage::TDamageInputStorage(QObject* parent)
    : cip::tibia::cyclopedia::TDamageInputStorage(parent)
    , m_seconds(BinCount, 0)
{}

void TDamageInputStorage::emitValuesChanged()
{
    Q_EMIT valuesChanged();
    Q_EMIT damageTakenDataChanged();
}

void TDamageInputStorage::emitGraphChanged()
{
    Q_EMIT graphChanged();
    Q_EMIT damageTakenGraphChanged();
}

void TDamageInputStorage::emitOptionsChanged()
{
    Q_EMIT optionsChanged();
    Q_EMIT configurationChangedFromUserInput();
}

QJsonObject TDamageInputStorage::configurationToJson() const
{
    return { { QLatin1String(KeyShowSessionValues), m_showSessionValues },
             { QLatin1String(KeyShowDamageGraph), m_showGraph },
             { QLatin1String(KeyShowDamageTypes), m_showTypes },
             { QLatin1String(KeyShowDamageSources), m_showSources } };
}

void TDamageInputStorage::configurationFromJson(const QJsonObject& json)
{
    m_showSessionValues = json.value(QLatin1String(KeyShowSessionValues)).toBool(false);
    m_showGraph = json.value(QLatin1String(KeyShowDamageGraph)).toBool(true);
    m_showTypes = json.value(QLatin1String(KeyShowDamageTypes)).toBool(true);
    m_showSources = json.value(QLatin1String(KeyShowDamageSources)).toBool(true);
    emitOptionsChanged();
}

void TDamageInputStorage::resetConfiguration() { configurationFromJson({}); }

void TDamageInputStorage::addHit(const Hit& hit)
{
    m_sessionTotal += hit.amount;
    m_seconds.back() += static_cast<quint32>(hit.amount);
    m_maxPerSecond = std::max(m_maxPerSecond, m_seconds.back());
    m_hits.push_back(hit);

    auto element = std::lower_bound(m_sessionElements.begin(), m_sessionElements.end(), hit.element,
                                    [](const std::pair<int, int>& entry, const int key) { return entry.first < key; });
    if (element == m_sessionElements.end() || element->first != hit.element)
        m_sessionElements.insert(element, { hit.element, hit.amount });
    else
        element->second += hit.amount;

    const Source added(hit);
    auto source = std::lower_bound(m_sessionSources.begin(), m_sessionSources.end(), added.name, [](const Source& entry, const QString& key) {
        return QString::compare(entry.name, key, Qt::CaseSensitive) < 0;
    });
    if (source != m_sessionSources.end() && source->name == added.name) {
        source->total += added.total;
        for (const auto& [id, amount] : added.elements)
            source->add(id, amount);
    } else {
        m_sessionSources.insert(source, added);
    }
}

void TDamageInputStorage::update(const qint64 nowMs)
{
    if (nowMs > m_lastDataUpdate + DataRefreshMs) {
        m_hits.erase(std::remove_if(m_hits.begin(), m_hits.end(), [nowMs](const Hit& hit) { return hit.timestamp < nowMs - WindowMs; }),
                     m_hits.end());
        m_windowTotal = 0;
        for (const Hit& hit : m_hits)
            m_windowTotal += hit.amount;

        m_windowSources.clear();
        m_listedSessionSources.clear();
        m_windowElements.clear();
        if (m_sessionSources.size() <= ListedSources) {
            m_listedSessionSources = m_sessionSources;
        } else {
            m_listedSessionSources.assign(ListedSources, Source(Hit{}));
            std::partial_sort_copy(m_sessionSources.begin(), m_sessionSources.end(), m_listedSessionSources.begin(),
                                   m_listedSessionSources.end(), biggerSource);
        }

        for (const Hit& hit : m_hits) {
            const auto source =
                std::find_if(m_windowSources.begin(), m_windowSources.end(), [&hit](const Source& entry) { return entry.name == hit.source; });
            if (source == m_windowSources.end()) {
                m_windowSources.emplace_back(hit);
            } else {
                source->total += hit.amount;
                source->add(hit.element, hit.amount);
            }
            const auto element = std::find_if(m_windowElements.begin(), m_windowElements.end(),
                                              [&hit](const std::pair<int, int>& entry) { return entry.first == hit.element; });
            if (element == m_windowElements.end())
                m_windowElements.push_back({ hit.element, hit.amount });
            else
                element->second += hit.amount;
        }
        if (m_windowSources.size() > ListedSources) {
            std::nth_element(m_windowSources.begin(), m_windowSources.begin() + (ListedSources - 1), m_windowSources.end(), biggerSource);
            m_windowSources.erase(m_windowSources.begin() + ListedSources, m_windowSources.end());
        }

        // Age the per-second damage by the seconds since the last refresh.
        const int shift = static_cast<int>(nowMs / 1000 - m_lastDataUpdate / 1000);
        const int count = static_cast<int>(m_seconds.size());
        if (shift < count) {
            if (shift > 0) {
                std::move(m_seconds.begin() + shift, m_seconds.end(), m_seconds.begin());
                std::fill(m_seconds.end() - shift, m_seconds.end(), 0);
            }
        } else {
            std::fill(m_seconds.begin(), m_seconds.end(), 0);
        }
        emitValuesChanged();
        m_lastDataUpdate = nowMs;
    }

    const std::vector<double>* weights = nullptr;
    int step = 0;
    if (m_showSessionValues && nowMs > m_lastGraphUpdate + SessionGraphRefreshMs) {
        weights = &sessionGraphWeights();
        step = SessionGraphStepSeconds;
    } else if (!m_showSessionValues && nowMs > m_lastGraphUpdate + GraphRefreshMs) {
        weights = &recentGraphWeights();
        step = GraphStepSeconds;
    } else if (m_lastGraphUpdate == 0) {
        weights = m_showSessionValues ? &sessionGraphWeights() : &recentGraphWeights();
        step = m_showSessionValues ? SessionGraphStepSeconds : GraphStepSeconds;
    } else {
        return;
    }
    // Each point is the weighted mean of the seconds before it.
    const qint64 count = static_cast<qint64>(m_seconds.size());
    const qint64 length = static_cast<qint64>(weights->size());
    for (int i = 0; i < GraphPoints; ++i) {
        const qint64 end = count + static_cast<qint64>(i - (GraphPoints - 1)) * step;
        const qint64 first = end - length;
        double value = 0.0;
        if (first != end && first >= 0) {
            for (qint64 k = 0; k < length; ++k)
                value += static_cast<double>(m_seconds[static_cast<size_t>(first + k)]) * (*weights)[static_cast<size_t>(k)];
        }
        m_graph[static_cast<size_t>(i)] = static_cast<int>(static_cast<qint64>(value));
    }
    emitGraphChanged();
    m_lastGraphUpdate = nowMs;
}

std::vector<double> TDamageInputStorage::graph(double& maxY) const
{
    std::vector<double> points(m_graph.begin(), m_graph.end());
    const double maxValue = points.empty() ? 0.0 : *std::max_element(points.begin(), points.end());
    maxY = adjustGraphMaxY(maxY, maxValue);
    return points;
}

void TDamageInputStorage::reset()
{
    m_sessionTotal = 0;
    m_windowTotal = 0;
    m_maxPerSecond = 0;
    std::fill(m_seconds.begin(), m_seconds.end(), 0);
    m_graph = {};
    m_sessionElements.clear();
    m_sessionSources.clear();
    m_hits.clear();
    m_listedSessionSources.clear();
    m_windowSources.clear();
    m_windowElements.clear();
    emitValuesChanged();
    emitGraphChanged();
}

void TDamageInputStorage::setShowSessionValues(const bool show)
{
    if (m_showSessionValues == show)
        return;
    m_showSessionValues = show;
    m_lastGraphUpdate = 0;
    m_graph = {};
    emitOptionsChanged();
    emitValuesChanged();
    emitGraphChanged();
}

void TDamageInputStorage::setShowGraph(const bool show)
{
    if (m_showGraph == show)
        return;
    m_showGraph = show;
    emitOptionsChanged();
}

void TDamageInputStorage::setShowTypes(const bool show)
{
    if (m_showTypes == show)
        return;
    m_showTypes = show;
    emitOptionsChanged();
}

void TDamageInputStorage::setShowSources(const bool show)
{
    if (m_showSources == show)
        return;
    m_showSources = show;
    emitOptionsChanged();
}

}
