#include "hints.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>

namespace tibia::app {

QVector<Hint> parseHints(const QByteArray& body)
{
    QVector<Hint> hints;
    const QJsonDocument json = QJsonDocument::fromJson(body);
    const QJsonArray list = json.isArray() ? json.array() : json.object().value(QStringLiteral("hints")).toArray();
    for (const QJsonValue& value : list) {
        Hint hint;
        if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            hint.title = obj.value(QStringLiteral("title")).toString();
            hint.richText = obj.value(QStringLiteral("richText")).toString();
            if (hint.richText.isEmpty())
                hint.richText = obj.value(QStringLiteral("text")).toString(obj.value(QStringLiteral("hint")).toString());
        } else if (value.isString()) {
            hint.richText = value.toString();
        }
        if (!hint.richText.isEmpty())
            hints.push_back(hint);
    }
    return hints;
}

bool hasRichHints(const QVector<Hint>& hints)
{
    for (const Hint& hint : hints) {
        if (hint.richText.contains(QLatin1String("<img"), Qt::CaseInsensitive)
            || hint.richText.contains(QLatin1String("<table"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

Hint randomHint(const QVector<Hint>& hints)
{
    if (hints.isEmpty())
        return {};
    return hints.at(QRandomGenerator::global()->bounded(hints.size()));
}

QString hintHtml(const Hint& hint, const QString& cdnBase)
{
    QString base = cdnBase;
    while (base.endsWith(u'/'))
        base.chop(1);
    QString html = hint.richText;
    html.replace(QStringLiteral("\"/images/"), QStringLiteral("\"") + base + QStringLiteral("/images/"));
    html.replace(QStringLiteral("https://static.tibia.com"), base);
    html.replace(QStringLiteral("http://static.tibia.com"), base);
    return html;
}

QVector<Hint> loadBundledHints()
{
    QFile file(QStringLiteral(":/hints.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parseHints(file.readAll());
}

}
