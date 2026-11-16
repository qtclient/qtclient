#include "crashreportdata.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace tibia::shared {

TCrashReportData::TCrashReportData(const QByteArray& json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QStringList keys = object.keys();
    for (const QString& key : keys)
        (*this)[key] = object[key].toString(QString());
}

std::map<QString, QString>& TCrashReportData::values()
{
    return *this;
}

const std::map<QString, QString>& TCrashReportData::values() const
{
    return *this;
}

QJsonDocument TCrashReportData::toJsonDocument() const
{
    QJsonDocument document;
    QJsonObject object;
    for (const auto& [key, value] : *this)
        object.insert(key, QJsonValue(value));
    document.setObject(object);
    return document;
}

QByteArray TCrashReportData::toJson() const
{
    return toJsonDocument().toJson(QJsonDocument::Indented);
}

void TCrashReportData::anonymize()
{
    const QString character = QStringLiteral("Character");
    if (find(character) != end())
        (*this)[character] = QStringLiteral("xxx");
}

}
