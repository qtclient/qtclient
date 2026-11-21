#include "randomhints.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>

namespace tibia::shared {

namespace {

/// CIP: an ISO date, else `fallback`.
QDateTime hintDateTime(const QJsonValue& value, const QDateTime& fallback)
{
    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    return parsed.isValid() ? parsed : fallback;
}

/// CIP: link colour for the hint area's rich text.
void styleHintLinks(Hint& hint)
{
    if (!hint.richText.isEmpty())
        hint.richText = QStringLiteral("<style>a { color: #c0c0c0; }</style>") + hint.richText;
}

Hint parseHint(const QJsonObject& object)
{
    Hint hint;
    hint.title = object[QStringLiteral("title")].toString();
    hint.richText = object[QStringLiteral("richText")].toString();
    hint.displayAfter = hintDateTime(object[QStringLiteral("displayAfter")], QDateTime());
    hint.displayUntil = hintDateTime(object[QStringLiteral("displayUntil")], QDateTime());
    hint.hasEmergency = object.contains(QStringLiteral("emergency"));
    if (hint.hasEmergency)
        hint.emergency = object[QStringLiteral("emergency")].toBool();
    styleHintLinks(hint);
    return hint;
}

}

QVector<Hint> parseHints(const QByteArray& body)
{
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Could not parse hints file at offset" << error.offset << ":" << error.errorString();
        return {};
    }
    QVector<Hint> hints;
    const QJsonArray list = json.array();
    for (const QJsonValue& value : list) {
        Hint hint = parseHint(value.toObject());
        if (!hint.richText.isEmpty())
            hints.push_back(std::move(hint));
    }
    return hints;
}

std::optional<Hint> selectHint(const QVector<Hint>& hints, const QDateTime& now)
{
    for (const Hint& hint : hints) {
        if (hint.hasEmergency && hint.emergency)
            return hint;
    }
    QVector<Hint> candidates;
    for (const Hint& hint : hints) {
        const bool started = !hint.displayAfter.isValid() || hint.displayAfter <= now;
        const bool notEnded = !hint.displayUntil.isValid() || now <= hint.displayUntil;
        if (started && notEnded && !hint.hasEmergency)
            candidates.push_back(hint);
    }
    for (const Hint& hint : candidates) {
        if (hint.displayAfter.isValid() || hint.displayUntil.isValid())
            return hint;
    }
    if (candidates.isEmpty())
        return std::nullopt;
    const quint64 msecs = static_cast<quint64>(now.toMSecsSinceEpoch());
    return candidates.at(static_cast<qsizetype>(msecs % static_cast<quint64>(candidates.size())));
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
    // CIP richText uses both quote styles on <img src=…>; rewrite both so Qt
    // does not resolve a bare /images/… path as file:///images/….
    html.replace(QStringLiteral("\"/images/"), QStringLiteral("\"") + base + QStringLiteral("/images/"));
    html.replace(QStringLiteral("'/images/"), QStringLiteral("'") + base + QStringLiteral("/images/"));
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
