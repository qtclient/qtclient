#pragma once

#include <QDateTime>
#include <QtGlobal>

namespace tibia {
namespace shared {

/// CIP `shared::IClockService`: a source of the current time.
class IClockService
{
public:
    virtual ~IClockService() = default;

    virtual qint64 nowMillis() = 0;
    virtual qint64 nowNanos() = 0;
    virtual qint64 nowMicros() = 0;
    virtual QDateTime currentDateTime() = 0;
};

}
}
