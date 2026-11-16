#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

class QByteArray;

namespace tibia {
namespace app {

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

/// True when at least one hint carries CIP table/img richText (not a string stub).
bool hasRichHints(const QVector<Hint>& hints);

/// CIP `randomhints.cpp`: one entry from the list.
Hint randomHint(const QVector<Hint>& hints);

/// Point hint <img> sources at our CDN. Files live under /images/news/.
QString hintHtml(const Hint& hint, const QString& cdnBase);

/// Bundled :/hints.json fallback when the CDN still ships the string stub.
QVector<Hint> loadBundledHints();

}
}
