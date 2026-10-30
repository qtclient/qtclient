#include "analysers/analyserstorages.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>

namespace tibia::analysers {

namespace {

constexpr auto KeyGaugeVisible = "desiredExperienceGaugeVisible";
constexpr auto KeyGaugeTarget = "experienceGaugeTargetValue";
constexpr auto KeyGraphVisible = "desiredXPGraphVisible";
constexpr auto KeyShowBaseXp = "showBaseXp";

/// CIP FUN_100973980's clamp for experienceGaugeTargetValue.
constexpr double MinGaugeTarget = 0.1;
constexpr double MaxGaugeTarget = 999999999.9;

const std::vector<double>& xpWeights()
{
    static const std::vector<double> weights = linearWeights(TExperienceChangeTracker::WindowSeconds);
    return weights;
}

}

std::vector<double> linearWeights(const int n)
{
    std::vector<double> weights(static_cast<size_t>(n));
    const double sum = static_cast<double>(n) * (n + 1);
    for (int i = 0; i < n; ++i)
        weights[static_cast<size_t>(i)] = 2.0 * (i + 1) / sum;
    return weights;
}

double adjustGraphMaxY(const double currentMaxY, const double maxValue)
{
    if (maxValue <= currentMaxY && currentMaxY <= maxValue * 1.4)
        return currentMaxY;
    return std::ceil(std::max(1.0, maxValue * 1.2));
}

TExperienceChangeTracker::TExperienceChangeTracker()
    : m_bins(BinCount, 0.0)
{}

bool TExperienceChangeTracker::tick(const qint64 nowMs)
{
    const qint64 second = nowMs / 1000;
    if (second <= m_second)
        return false;
    const qint64 shift = second - m_second;
    m_second = second;
    if (shift >= BinCount) {
        const bool changed = std::any_of(m_bins.begin(), m_bins.end(), [](double v) { return v != 0.0; });
        std::fill(m_bins.begin(), m_bins.end(), 0.0);
        return changed;
    }
    std::move(m_bins.begin() + shift, m_bins.end(), m_bins.begin());
    std::fill(m_bins.end() - shift, m_bins.end(), 0.0);
    return true;
}

void TExperienceChangeTracker::add(const qint64 amount)
{
    m_total += amount;
    m_bins.back() += static_cast<double>(amount);
}

void TExperienceChangeTracker::reset()
{
    m_total = 0;
    std::fill(m_bins.begin(), m_bins.end(), 0.0);
}

double TExperienceChangeTracker::perSecond(const int offsetSeconds) const
{
    const std::vector<double>& weights = xpWeights();
    const qint64 end = static_cast<qint64>(m_bins.size()) - 1 - offsetSeconds;
    const qint64 first = end - static_cast<qint64>(weights.size());
    if (first < 0)
        return 0.0;
    double value = 0.0;
    for (size_t k = 0; k < weights.size(); ++k)
        value += weights[k] * m_bins[static_cast<size_t>(first) + k];
    return value;
}

TProgressStorage::TProgressStorage(QObject* parent)
    : QObject(parent)
{}

QJsonObject TProgressStorage::toJson() const
{
    return { { QLatin1String(KeyGaugeVisible), m_gaugeVisible },
             { QLatin1String(KeyGaugeTarget), m_gaugeTarget },
             { QLatin1String(KeyGraphVisible), m_graphVisible },
             { QLatin1String(KeyShowBaseXp), m_showBaseXp } };
}

void TProgressStorage::fromJson(const QJsonObject& json)
{
    m_gaugeVisible = json.value(QLatin1String(KeyGaugeVisible)).toBool(true);
    m_gaugeTarget = std::clamp(json.value(QLatin1String(KeyGaugeTarget)).toDouble(100.0), MinGaugeTarget, MaxGaugeTarget);
    m_graphVisible = json.value(QLatin1String(KeyGraphVisible)).toBool(true);
    m_showBaseXp = json.value(QLatin1String(KeyShowBaseXp)).toBool(false);
    Q_EMIT optionsChanged();
}

void TProgressStorage::tick(const qint64 nowMs)
{
    const bool finalChanged = m_final.tick(nowMs);
    const bool baseChanged = m_base.tick(nowMs);
    if (finalChanged || baseChanged)
        Q_EMIT dataChanged();
}

void TProgressStorage::reset()
{
    m_final.reset();
    m_base.reset();
    Q_EMIT dataChanged();
    Q_EMIT wasReset();
}

void TProgressStorage::onExperienceGained(const qint64 raw, const qint64 final, const qint64 nowMs)
{
    m_final.tick(nowMs);
    m_base.tick(nowMs);
    m_base.add(raw);
    m_final.add(final);
    Q_EMIT dataChanged();
}

void TProgressStorage::setGaugeVisible(const bool visible)
{
    if (m_gaugeVisible == visible)
        return;
    m_gaugeVisible = visible;
    Q_EMIT optionsChanged();
}

void TProgressStorage::setGaugeTarget(const double target)
{
    const double clamped = std::clamp(target, MinGaugeTarget, MaxGaugeTarget);
    if (m_gaugeTarget == clamped)
        return;
    m_gaugeTarget = clamped;
    Q_EMIT optionsChanged();
}

void TProgressStorage::setGraphVisible(const bool visible)
{
    if (m_graphVisible == visible)
        return;
    m_graphVisible = visible;
    Q_EMIT optionsChanged();
}

void TProgressStorage::setShowBaseXp(const bool show)
{
    if (m_showBaseXp == show)
        return;
    m_showBaseXp = show;
    Q_EMIT optionsChanged();
}

void TProgressStorage::computeHistory(QVariantList& history, double& maxY) const
{
    const qsizetype points = history.size();
    if (points == 0)
        return;
    const float firstX = history.first().value<QVector2D>().x();
    double maxValue = 0.0;
    for (qsizetype i = 0; i < points; ++i) {
        const double perHour = std::max(0.0, m_final.perSecond(static_cast<int>((points - 1 - i) * 60)) * 3600.0);
        maxValue = std::max(maxValue, perHour);
        history[i] = QVector2D(firstX + static_cast<float>(i), static_cast<float>(perHour));
    }
    maxY = adjustGraphMaxY(maxY, maxValue);
}

}
