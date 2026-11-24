#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

class QByteArray;

namespace tibia {
namespace shared {

/// One entry of hints.json (CIP: shared::THint).
struct Hint
{
    QString title;
    QString richText;
    /// Shown only from / until then (UTC); invalid when not given.
    QDateTime displayAfter;
    QDateTime displayUntil;
    bool hasEmergency = false;
    bool emergency = false;
};

/// CIP shared::parseHints: the JSON array of hint objects; entries without
/// richText are dropped. Nothing when the document does not parse.
QVector<Hint> parseHints(const QByteArray& body);

/// CIP: the hint to show at `now` - an emergency hint first, then the first
/// scheduled hint in its window, else one picked by the clock.
std::optional<Hint> selectHint(const QVector<Hint>& hints, const QDateTime& now);

/// Point hint <img> sources at our CDN. Files live under /images/news/.
QString hintHtml(const Hint& hint, const QString& cdnBase);

}
}
