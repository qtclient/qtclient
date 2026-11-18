#pragma once

#include <QJsonDocument>

namespace tibia {
namespace systemsurvey {

class TFPSHistory;

/// CIP `systemsurvey::TFPSHistoryToJsonConverter`: the upload document of an
/// FPS history (data type 1, the machine's client id, the samples with their
/// rates rounded, and the renderer).
class TFPSHistoryToJsonConverter
{
public:
    virtual ~TFPSHistoryToJsonConverter() = default;

    virtual QJsonDocument convert(const TFPSHistory& history, int renderer) const;
};

}
}
