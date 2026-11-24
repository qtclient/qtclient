#pragma once

#include <QJsonDocument>
#include <QUrl>
#include <QtGlobal>

#include <vector>

namespace tibia {
namespace sessiondump {

/// CIP `sessiondump::TSessiondumpCatalog`: the sessiondumps a catalog JSON
/// offers (`{"sessiondumps": [{url, filesize, durationMilliseconds, hints}],
/// "autoplayIndex": n}`).
class TSessiondumpCatalog
{
public:
    struct TEntry
    {
        QUrl url;
        QUrl hintsUrl;
        qint64 fileSize = 0;
        qint64 durationMilliseconds = 0;
        bool hasHints = false;
        /// The character whose configuration a replay of the entry loads; the catalog leaves it 0.
        quint32 characterId = 0;
    };

    TSessiondumpCatalog() = default;
    explicit TSessiondumpCatalog(const QJsonDocument& document);

    const std::vector<TEntry>& entries() const { return m_entries; }
    /// The entry to play without asking; -1 for none.
    int autoplayIndex() const { return m_autoplayIndex; }

private:
    std::vector<TEntry> m_entries;
    int m_autoplayIndex = -1;
};

}
}
