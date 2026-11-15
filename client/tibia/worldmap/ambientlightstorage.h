#pragma once

#include <cip_gen.h>

#include <QColor>
#include <QTimer>

#include <cstdint>
#include <memory>

namespace tibia {
namespace worldmap {

/// A light: brightness 0..255 and colour.
struct TAmbientLight
{
    uint8_t intensity = 0;
    QColor color;
};

/// The world's light and the Tibia time of day (CIP: TAmbientLightStorage).
///
/// The server's light is the target; the shown light walks toward it one
/// unit of brightness and of each colour channel per step, 255 steps taking
/// `GRAPHICS/millisecondsForAmbientLightChange` (default 10200 ms). The first
/// light, or every light when that time is 0, is shown at once.
///
/// One Tibia day lasts one real hour. The time of day runs on the frame clock
/// handed to update(), so it holds still while no frame is drawn.
class TAmbientLightStorage final : public cip::tibia::worldmap::TAmbientLightStorage
{
    Q_OBJECT
public:
    explicit TAmbientLightStorage(QObject* parent = nullptr);
    ~TAmbientLightStorage() override;

    static TAmbientLightStorage* instance();

    /// Per frame: moves the shown light toward the target for the time since
    /// the last frame (at most one full change).
    void update(qint64 nowMs);
    /// The server's world light.
    void setLight(const TAmbientLight& light);
    /// The light shown now.
    TAmbientLight light() const { return m_light; }
    /// The shown light's colour darkened by its brightness.
    QColor color() const;

    /// CIP: start = last frame - normalized * one hour.
    void setNormalizedTibiaTimeOfDay(float normalized);
    /// Days since the clock's start at the last frame; the fraction is the
    /// time of day. Not wrapped: TibiaMinimapRose takes it modulo 1.
    float normalizedTibiaTimeOfDay() const;

private:
    qint64 m_changeDurationMs = 0;
    TAmbientLight m_light;
    std::unique_ptr<TAmbientLight> m_target;
    qint64 m_pendingMs = 0;
    qint64 m_lastUpdateMs = 0;
    qint64 m_dayStartMs = 0;
    QTimer m_interpolationTimer;
    int m_worldTimeHook = 0;
};

}
}
