#include "specialconditionsoptions.h"

#include "app/clientoptions.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace tibia::config {

namespace {

using C = SpecialCondition;

QList<SpecialCondition> parseList(const QJsonValue& value)
{
    QList<SpecialCondition> list;
    for (const QJsonValue& entry : value.toArray()) {
        const QJsonArray pair = entry.toArray();
        if (pair.size() != 2)
            continue;
        const SpecialCondition condition{ pair.at(0).toInt(-1), pair.at(1).toInt(-1) };
        if (SpecialConditionsOptions::isKnown(condition))
            list.append(condition);
    }
    return list;
}

QSet<SpecialCondition> parseSet(const QJsonValue& value)
{
    const QList<SpecialCondition> list = parseList(value);
    return { list.begin(), list.end() };
}

}

const QList<SpecialCondition>& SpecialConditionsOptions::known()
{
    static const QList<SpecialCondition> list = {
        { C::PlayerStateFlag, 0 },  { C::PlayerStateFlag, 1 },  { C::PlayerStateFlag, 2 },  { C::PlayerStateFlag, 15 },
        { C::PlayerStateFlag, 27 }, { C::PlayerStateFlag, 28 }, { C::PlayerStateFlag, 19 }, { C::PlayerStateFlag, 20 },
        { C::PlayerStateFlag, 3 },  { C::Generic, C::ManaShield }, { C::PlayerStateFlag, 29 }, { C::PlayerStateFlag, 5 },
        { C::PlayerStateFlag, 6 },  { C::PlayerStateFlag, 7 },  { C::PlayerStateFlag, 8 },  { C::PlayerStateFlag, 9 },
        { C::PlayerStateFlag, 10 }, { C::PlayerStateFlag, 11 }, { C::PlayerStateFlag, 12 }, { C::PlayerStateFlag, 13 },
        { C::PlayerStateFlag, 14 }, { C::Generic, C::RestingArea }, { C::PlayerStateFlag, 16 }, { C::PlayerStateFlag, 17 },
        { C::PlayerStateFlag, 18 }, { C::Generic, C::Goshnar }, { C::Generic, C::RottenBlood }, { C::PlayerKillerFlag, 1 },
        { C::PlayerKillerFlag, 2 }, { C::PlayerKillerFlag, 3 }, { C::PlayerKillerFlag, 4 }, { C::PlayerKillerFlag, 5 },
        { C::PlayerKillerFlag, 6 }, { C::Generic, C::GuildWar }, { C::PlayerStateClient, C::Hungry },
    };
    return list;
}

bool SpecialConditionsOptions::isKnown(const SpecialCondition& condition) { return known().contains(condition); }

std::optional<SpecialCondition> SpecialConditionsOptions::forStateBit(const int bit)
{
    if (bit >= 21 && bit <= 25)
        return SpecialCondition{ C::Generic, C::Goshnar };
    if (bit == 4 || bit == 26)
        return SpecialCondition{ C::Generic, C::ManaShield };
    const SpecialCondition condition{ C::PlayerStateFlag, bit };
    if (isKnown(condition))
        return condition;
    return std::nullopt;
}

SpecialConditionsOptions SpecialConditionsOptions::load()
{
    return fromJson(app::ClientOptions::load().root().value(QLatin1String("specialConditionsOptions")).toObject());
}

SpecialConditionsOptions SpecialConditionsOptions::defaults()
{
    QFile file(QStringLiteral(":/conf/clientoptions.json"));
    const QJsonObject root = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    return fromJson(root.value(QLatin1String("specialConditionsOptions")).toObject());
}

SpecialConditionsOptions SpecialConditionsOptions::fromJson(const QJsonObject& json)
{
    SpecialConditionsOptions o;
    o.showInHUD = json.value(QLatin1String("showInHUD")).toBool(true);
    o.showInBar = json.value(QLatin1String("showInBar")).toBool(true);
    o.visibleInHUD = parseSet(json.value(QLatin1String("visibleInHUD")));
    o.visibleInBar = parseSet(json.value(QLatin1String("visibleInBar")));
    o.specialConditionsOrder = parseList(json.value(QLatin1String("specialConditionsOrder")));
    if (o.specialConditionsOrder.isEmpty())
        o.specialConditionsOrder = known();
    return o;
}

QJsonObject SpecialConditionsOptions::toJson() const
{
    const auto list = [](const QList<SpecialCondition>& conditions) {
        QJsonArray array;
        for (const SpecialCondition& condition : conditions)
            array.append(QJsonArray{ condition.type, condition.value });
        return array;
    };
    const auto set = [&list](const QSet<SpecialCondition>& conditions) {
        QList<SpecialCondition> sorted(conditions.begin(), conditions.end());
        std::sort(sorted.begin(), sorted.end());
        return list(sorted);
    };
    return {
        { QStringLiteral("showInHUD"), showInHUD },
        { QStringLiteral("showInBar"), showInBar },
        { QStringLiteral("visibleInHUD"), set(visibleInHUD) },
        { QStringLiteral("visibleInBar"), set(visibleInBar) },
        { QStringLiteral("specialConditionsOrder"), list(specialConditionsOrder) },
    };
}

bool SpecialConditionsOptions::visible(const SpecialCondition& condition, const SpecialConditionsFilter filter) const
{
    return (filter == SpecialConditionsFilter::HUD ? visibleInHUD : visibleInBar).contains(condition);
}

int SpecialConditionsOptions::sortOrder(const SpecialCondition& condition) const
{
    const qsizetype index = specialConditionsOrder.indexOf(condition);
    return index < 0 ? 9999 : static_cast<int>(index);
}

}
