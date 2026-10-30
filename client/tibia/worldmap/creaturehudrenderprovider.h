#pragma once

#include <cip_gen.h>

#include <QPoint>
#include <QVector>

#include <client/declarations.h>

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

/// Creature HUD anchors for one frame (CIP: TCreatureHUDRenderProvider records;
/// filled by TWorldMapRenderProvider and consumed by TCreatureHUDOverlayController).
class TCreatureHUDRenderProvider final : public cip::tibia::worldmap::TCreatureHUDRenderProvider
{
    Q_OBJECT
public:
    struct Record
    {
        CreaturePtr creature;
        QPoint point;
    };

    explicit TCreatureHUDRenderProvider(QObject* parent = nullptr);

    void setRecords(QVector<Record> records);
    const QVector<Record>& records() const { return m_records; }

    /// Copy the provider frame's creature HUD list (CIP record layout).
    void syncFrom(const qmlcomponents::TWorldMapRenderProvider& provider);

private:
    QVector<Record> m_records;
};

}
