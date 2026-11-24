#include "specialconditionsoptions.h"

#include "app/clientoptions.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace tibia::config {

namespace {

using C = SpecialCondition;

/// One [variant index, value] entry; nullopt for a non-array entry or an
/// unknown variant index. EPlayerKillerFlag is a byte enum: only the low byte
/// of its value is kept.
std::optional<SpecialCondition> parseCondition(const QJsonValue& entry)
{
    if (!entry.isArray())
        return std::nullopt;
    const QJsonArray pair = entry.toArray();
    const int type = pair.at(0).toInt();
    const int value = pair.at(1).toInt();
    switch (type) {
    case C::PlayerStateFlag:
    case C::PlayerStateClient:
    case C::RottenBloodState:
    case C::Generic:
        return SpecialCondition{ type, value };
    case C::PlayerKillerFlag:
        return SpecialCondition{ type, value & 0xff };
    default:
        return std::nullopt;
    }
}

QList<SpecialCondition> parseList(const QJsonArray& array)
{
    QList<SpecialCondition> list;
    for (const QJsonValue& entry : array) {
        if (const auto condition = parseCondition(entry))
            list.append(*condition);
    }
    return list;
}

QSet<SpecialCondition> parseSet(const QJsonArray& array)
{
    QSet<SpecialCondition> set;
    for (const QJsonValue& entry : array) {
        if (const auto condition = parseCondition(entry))
            set.insert(*condition);
    }
    return set;
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
    // Conditions the HUD leaves out by default; the bar shows every one.
    static const QSet<SpecialCondition> hiddenInHud = {
        { C::PlayerStateFlag, 7 },  { C::PlayerStateFlag, 12 }, { C::PlayerStateFlag, 13 },
        { C::PlayerStateFlag, 14 }, { C::Generic, C::RestingArea }, { C::Generic, C::Goshnar },
        { C::Generic, C::RottenBlood }, { C::Generic, C::GuildWar }, { C::PlayerStateClient, C::Hungry },
    };
    SpecialConditionsOptions o;
    o.showInHUD = false;
    o.showInBar = true;
    o.specialConditionsOrder = known();
    for (const SpecialCondition& condition : known()) {
        o.visibleInBar.insert(condition);
        if (!hiddenInHud.contains(condition))
            o.visibleInHUD.insert(condition);
    }
    return o;
}

SpecialConditionsOptions SpecialConditionsOptions::fromJson(const QJsonObject& json)
{
    SpecialConditionsOptions o;
    o.showInHUD = json.value(QLatin1String("showInHUD")).toBool(true);
    o.showInBar = json.value(QLatin1String("showInBar")).toBool(true);
    o.visibleInHUD = parseSet(json.value(QLatin1String("visibleInHUD")).toArray());
    o.visibleInBar = parseSet(json.value(QLatin1String("visibleInBar")).toArray());
    o.specialConditionsOrder = parseList(json.value(QLatin1String("specialConditionsOrder")).toArray());
    o.fixSpecialConditionsOrder();
    return o;
}

void SpecialConditionsOptions::fixSpecialConditionsOrder()
{
    // No stored order at all: the whole section falls back to the defaults.
    if (specialConditionsOrder.isEmpty()) {
        *this = defaults();
        return;
    }
    // Drop entries this client does not know, then append every known
    // condition the stored order misses, in the default order.
    specialConditionsOrder.removeIf([](const SpecialCondition& condition) { return !isKnown(condition); });
    for (const SpecialCondition& condition : known()) {
        if (!specialConditionsOrder.contains(condition))
            specialConditionsOrder.append(condition);
    }
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
