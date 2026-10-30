#pragma once

#include <QObject>
#include <QString>

namespace tibia::mods {

/// Optional attention/highlight backend for the control button bar.
class AttentionBackend : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual bool rewardWall() const = 0;
    virtual bool proficiency() const = 0;

Q_SIGNALS:
    void signalChanged(const QString& signalId, bool active);
};

} // namespace tibia::mods
