#pragma once

#include "systemsurvey/systemsurvey.h"

#include <memory>

namespace tibia::systemsurvey {

class TSystemSurveyValueRetriever;

/// Collects a TSystemSurvey from a value retriever.
class TSystemSurveyCreator
{
public:
    explicit TSystemSurveyCreator(std::shared_ptr<TSystemSurveyValueRetriever> valueRetriever);
    virtual ~TSystemSurveyCreator() = default;

    virtual TSystemSurvey createSystemSurvey() const;

    /// Monitors that report a size (width and height both non-zero).
    static std::vector<TMonitorInformation> validMonitors(const std::vector<TMonitorInformation>& monitors);

private:
    std::shared_ptr<TSystemSurveyValueRetriever> m_valueRetriever;
};

} // namespace tibia::systemsurvey
