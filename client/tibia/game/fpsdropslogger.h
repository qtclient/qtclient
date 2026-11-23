#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <QUrl>

#include <cstddef>
#include <memory>
#include <vector>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace shared {
class TJsonDocumentUploaderBase;
}

namespace game {

/// CIP game::FPSDropsLogger: keeps the durations of the last 200 frames and,
/// from a minute after it was made, reports a frame that took more than ten
/// times the average of the others (and longer than any reported before,
/// at least 300) to the server as JSON. Frames are not recorded while a
/// dialog is visible.
class FPSDropsLogger final : public cip::tibia::game::FPSDropsLogger
{
    Q_OBJECT
public:
    /// `url`: where the reports are posted. `options`: the client options the
    /// report describes the graphics settings from.
    FPSDropsLogger(const QUrl& url, const config::TClientOptions* options, QObject* parent = nullptr);
    ~FPSDropsLogger() override;

    /// Records one frame's duration and checks for a drop.
    void addFrameDuration(qint64 duration);
    /// The player's position the report names.
    void setPlayerPosition(const worldmap::TWorldMapCoordinate& position);
    void setCharacterName(const QString& characterName) { m_characterName = characterName; }
    void setWorldName(const QString& worldName) { m_worldName = worldName; }
    /// Drops the recorded frame durations.
    void resetFrameDurations();

public Q_SLOTS:
    /// Showing a dialog drops the recorded frame durations.
    void onDialogVisibleChanged(bool Visible);

private:
    /// A fixed-capacity ring: once full, each new frame replaces the oldest.
    class TFrameDurations
    {
    public:
        explicit TFrameDurations(std::size_t capacity)
            : m_storage(capacity)
        {
        }
        std::size_t capacity() const { return m_storage.size(); }
        std::size_t size() const { return m_size; }
        bool full() const { return m_size == m_storage.size(); }
        /// The `index`th oldest duration.
        qint64 at(std::size_t index) const { return m_storage[(m_first + index) % m_storage.size()]; }
        qint64 front() const { return at(0); }
        void pushBack(qint64 duration);
        void clear();

    private:
        std::vector<qint64> m_storage;
        std::size_t m_first = 0;
        std::size_t m_size = 0;
    };

    /// At most when a minute has passed since the logger was made: reports a
    /// drop seen in the middle of a full ring.
    void checkForFrameDrop();
    QJsonDocument createReport() const;

    TFrameDurations m_frameDurations{ 200 };
    qint64 m_frameDurationSum = 0;
    qint64 m_longestReportedDuration = 300;
    QUrl m_url;
    const config::TClientOptions* m_options = nullptr;
    std::shared_ptr<shared::TJsonDocumentUploaderBase> m_uploader;
    worldmap::TWorldMapCoordinate m_playerPosition;
    QString m_characterName;
    QString m_worldName;
    qint64 m_creationTime = 0;
    bool m_dialogVisible = false;
};

}
}
