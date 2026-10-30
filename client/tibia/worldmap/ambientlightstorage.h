#pragma once

#include <cip_gen.h>

#include <QElapsedTimer>
#include <QTimer>

namespace tibia::worldmap {

/// The Tibia time of day (CIP: TAmbientLightStorage, 0x101054b80). One Tibia
/// day lasts one real hour; the server's world time sets the clock's start.
/// Only the clock is ported: the world light comes from the engine's map.
class TAmbientLightStorage final : public cip::tibia::worldmap::TAmbientLightStorage
{
    Q_OBJECT
public:
    explicit TAmbientLightStorage(QObject* parent = nullptr);
    ~TAmbientLightStorage() override;

    /// Days since the clock's start; the fraction is the time of day (CIP
    /// 0x101054f80). Not wrapped: TibiaMinimapRose takes it modulo 1.
    float normalizedTibiaTimeOfDay() const;
    /// CIP 0x101054f40: start = now - normalized * one hour.
    void setNormalizedTibiaTimeOfDay(float normalized);

private:
    QElapsedTimer m_clock;
    qint64 m_startMs = 0;
    QTimer m_interpolationTimer;
    int m_worldTimeHook = 0;
};

}
