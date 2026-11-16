#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace tibia {
namespace shared {

/// CipSoft's shared JSON readers: the member `key` of `object` when it has
/// the expected type, else the fallback.

bool jsonBool(const QJsonObject& object, const QString& key, bool fallback);
/// A number clamped to [minimum, maximum].
int jsonInt(const QJsonObject& object, const QString& key, int minimum, int maximum, int fallback);
/// A number clamped to [minimum, maximum].
double jsonDouble(const QJsonObject& object, const QString& key, double minimum, double maximum, double fallback);
/// Empty when missing.
QString jsonString(const QJsonObject& object, const QString& key);
QJsonObject jsonObject(const QJsonObject& object, const QString& key);
QJsonArray jsonArray(const QJsonObject& object, const QString& key);
/// Seconds since the epoch in [0, INT_MAX] (0 when missing).
QDateTime jsonDateTime(const QJsonObject& object, const QString& key);

}
}
