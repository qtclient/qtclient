#pragma once

#include "systemsurvey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace systemsurvey {

/// The survey as the JSON document the survey service takes.
class TSystemSurveyToJsonConverter
{
public:
    virtual ~TSystemSurveyToJsonConverter() = default;

    virtual QJsonDocument convert(const TSystemSurvey& survey) const;

    /// SHA-1 of a fixed salt and the hardware addresses of all network
    /// interfaces (by interface index), hex; computed once per process.
    static QString clientId();
    static QJsonArray monitorsToJson(const std::vector<TMonitorInformation>& monitors);
    static QJsonObject monitorToJson(const TMonitorInformation& monitor);
    static QJsonObject operatingSystemToJson(const TOperatingSystemInformation& system);
    static QJsonObject cpuToJson(const TCpuInformation& cpu);
    static QJsonArray gpusToJson(const std::vector<TGpuInformation>& gpus);
    static QJsonObject gpuToJson(const TGpuInformation& gpu);
    static QJsonObject openGLToJson(const TOpenGLInformation& openGL);
};

} // namespace systemsurvey
