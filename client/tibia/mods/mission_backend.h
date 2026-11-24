#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace tibia {
namespace mods {

/// Host extension: extra data for the CIP quest tracker rows and the quest
/// log's selected mission, from a pack. The host hands the data to QML
/// untouched (the tracker's `extension` role, the quest log's
/// `missionExtension` property) and passes QML's activations back; it reads
/// no key of it. Without a backend both are empty.
class MissionBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    /// One tracked quest flag (mission) of the quest tracker.
    struct TrackedMission
    {
        int questFlagId = 0;
        /// 0 when the server sent none.
        int questLineId = 0;
        /// The mission's name without the completed marker.
        QString name;
        bool completed = false;
    };

    /// The tracker's extra data per quest flag id, for the tracked missions as
    /// a whole (empty map: nothing for that row).
    virtual QHash<int, QVariantMap> trackerExtensions(const QList<TrackedMission>& missions) const = 0;
    /// The quest log's extra data for its selected quest line, quest flag
    /// (0: none) and mission name.
    virtual QVariantMap questLogExtension(int questLineId, int questFlagId, const QString& missionName) const = 0;
    /// The quest log selected `questLineId` (> 0).
    virtual void questLineSelected(int questLineId) = 0;
    /// QML activated the extra data of a tracker row or of the quest log's
    /// selection; `choice` is whatever the QML passed (may be empty).
    virtual void activate(int questLineId, int questFlagId, const QString& missionName, const QString& choice) = 0;

Q_SIGNALS:
    /// The extra data changed: the tracker and the quest log ask again.
    void changed();
};

}
}
