#pragma once

#include <QString>
#include <QVector>

class QByteArray;

namespace tibia::app {

struct Hint
{
    QString title;
    QString richText;
};

/// CIP hints.json: a list of {title, richText}. `shared::parseHints`.
QVector<Hint> parseHints(const QByteArray& body);

/// True when at least one hint carries CIP table/img richText (not a string stub).
bool hasRichHints(const QVector<Hint>& hints);

/// CIP `randomhints.cpp`: one entry from the list.
Hint randomHint(const QVector<Hint>& hints);

/// Point hint <img> sources at our CDN. Files live under /images/news/.
QString hintHtml(const Hint& hint, const QString& cdnBase);

/// Bundled :/hints.json fallback when the CDN still ships the string stub.
QVector<Hint> loadBundledHints();

}
