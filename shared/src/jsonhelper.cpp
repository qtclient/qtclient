#include "jsonhelper.h"

#include "jsonvalidator.h"

#include <climits>

namespace tibia::shared {

namespace {

TJsonValueValidator member(const QJsonObject& object, const QString& key)
{
    TJsonObjectValidator validator(object, true);
    return validator.value(key);
}

}

bool jsonBool(const QJsonObject& object, const QString& key, const bool fallback)
{
    TJsonValueValidator value = member(object, key);
    if (!value.isBool().isValid())
        return fallback;
    return object[key].toBool();
}

int jsonInt(const QJsonObject& object, const QString& key, int minimum, const int maximum, int fallback)
{
    TJsonValueValidator value = member(object, key);
    if (value.isDouble().isValid())
        fallback = object[key].toInt(fallback);
    if (minimum < fallback)
        minimum = fallback;
    if (maximum <= minimum)
        minimum = maximum;
    return minimum;
}

double jsonDouble(const QJsonObject& object, const QString& key, const double minimum, double maximum, double fallback)
{
    TJsonValueValidator value = member(object, key);
    if (value.isDouble().isValid())
        fallback = object[key].toDouble(0.0);
    if (fallback <= minimum)
        fallback = minimum;
    if (fallback <= maximum)
        maximum = fallback;
    return maximum;
}

QString jsonString(const QJsonObject& object, const QString& key)
{
    TJsonValueValidator value = member(object, key);
    if (!value.isString().isValid())
        return QString();
    return object[key].toString();
}

QJsonObject jsonObject(const QJsonObject& object, const QString& key)
{
    TJsonValueValidator value = member(object, key);
    if (!value.isObject().isValid())
        return QJsonObject();
    return object[key].toObject();
}

QJsonArray jsonArray(const QJsonObject& object, const QString& key)
{
    TJsonValueValidator value = member(object, key);
    if (!value.isArray(QJsonValue::Undefined).isValid())
        return QJsonArray();
    return object[key].toArray();
}

QDateTime jsonDateTime(const QJsonObject& object, const QString& key)
{
    const int seconds = jsonInt(object, key, 0, INT_MAX, 0);
    QDateTime dateTime;
    dateTime.setSecsSinceEpoch(seconds);
    return dateTime;
}

}
