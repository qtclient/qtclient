#include "sessiondump/sessiondumpcatalog.h"

#include <QJsonArray>
#include <QJsonObject>

namespace tibia::sessiondump {

TSessiondumpCatalog::TSessiondumpCatalog(const QJsonDocument& document)
{
    const QJsonObject root = document.object();
    const QJsonArray sessiondumps = root[QStringLiteral("sessiondumps")].toArray();
    for (const QJsonValue& value : sessiondumps) {
        const QJsonObject object = value.toObject();
        TEntry entry;
        entry.url = QUrl(object[QStringLiteral("url")].toString());
        if (object.contains(QStringLiteral("filesize")))
            entry.fileSize = object[QStringLiteral("filesize")].toInt();
        if (object.contains(QStringLiteral("durationMilliseconds")))
            entry.durationMilliseconds = object[QStringLiteral("durationMilliseconds")].toInt();
        if (object.contains(QStringLiteral("hints")))
            entry.hintsUrl = QUrl(object[QStringLiteral("hints")].toString());
        entry.hasHints = entry.hintsUrl != QUrl();
        m_entries.push_back(entry);
    }
    if (root.contains(QStringLiteral("autoplayIndex")))
        m_autoplayIndex = root[QStringLiteral("autoplayIndex")].toInt(-1);
}

}
