#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace tibia::mods {

/// Optional quest-mission backend (product mods). CIP quest UI talks only to this.
class MissionBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual bool missionLookup(int questLineId) = 0;
    virtual bool missionAdvance(const QString& missionKey, const QString& taskEntryId = {}, int questFlagId = 0) = 0;
    virtual const QHash<int, QJsonObject>& missionTrackers() const = 0;
    virtual const QHash<int, QJsonObject>& missionTrackersByFlag() const = 0;
    virtual const QHash<int, QJsonObject>& missionOffers() const = 0;

Q_SIGNALS:
    void missionTrackerChanged();
    void missionOfferChanged();
};

} // namespace tibia::mods
