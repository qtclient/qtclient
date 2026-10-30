#include "tibiaurltools.h"

#include <QRegularExpression>
#include <QStringList>

namespace tibia::shared {

namespace {

/// CIP 0x1077b34c8 (initialised by 0x10102b5e0).
const QRegularExpression& nonUrlCharacter()
{
    static const QRegularExpression expression(QStringLiteral("[^a-zA-Z0-9$+-_.;:/?&=]"));
    return expression;
}

} // namespace

bool TTibiaUrlTools::hasNonUrlCharacters(const QString& text) { return nonUrlCharacter().match(text).hasMatch(); }

bool TTibiaUrlTools::containsUrl(const QString& text)
{
    static const QRegularExpression weight(QStringLiteral("[0-9]+\\.[0-9]{2} oz"));
    static const QRegularExpression dotted(QStringLiteral("\\S+\\.\\S+"));
    QString stripped = text;
    stripped.replace(weight, QStringLiteral(" weight "));
    return dotted.match(stripped).hasMatch();
}

void TTibiaUrlTools::sanitize(QString& text) { text.replace(nonUrlCharacter(), QStringLiteral("?")); }

QString TTibiaUrlTools::process(const QString& text)
{
    if (!containsUrl(text))
        return text;
    static const QRegularExpression wordBoundary(QStringLiteral("\\b"), QRegularExpression::UseUnicodePropertiesOption);
    const QStringList parts = text.split(wordBoundary);
    if (parts.isEmpty())
        return text;
    QStringList result{ parts.first() };
    for (qsizetype i = 1; i < parts.size(); ++i) {
        if (parts[i] != QLatin1String(".")) {
            result.append(parts[i]);
            continue;
        }
        result.last().append(parts[i]);
        if (++i < parts.size()) {
            result.last().append(parts[i]);
            sanitize(result.last());
        }
    }
    return result.join(QString());
}

} // namespace tibia::shared
