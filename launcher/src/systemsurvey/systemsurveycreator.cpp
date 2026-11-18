#include "systemsurveycreator.h"

#include "systemsurvey/systemsurveyvalueretriever.h"

#include <algorithm>
#include <iterator>

namespace tibia::systemsurvey {

TSystemSurveyCreator::TSystemSurveyCreator(std::shared_ptr<TSystemSurveyValueRetriever> valueRetriever)
    : m_valueRetriever(std::move(valueRetriever))
{
}

std::vector<TMonitorInformation> TSystemSurveyCreator::validMonitors(const std::vector<TMonitorInformation>& monitors)
{
    std::vector<TMonitorInformation> valid;
    valid.reserve(monitors.size());
    std::copy_if(monitors.begin(), monitors.end(), std::back_inserter(valid),
                 [](const TMonitorInformation& monitor) { return monitor.height != 0 && monitor.width != 0; });
    return valid;
}

TSystemSurvey TSystemSurveyCreator::createSystemSurvey() const
{
    TSystemSurvey survey;
    survey.monitors = validMonitors(m_valueRetriever->getMonitorInformation());
    survey.operatingSystem = m_valueRetriever->getOperatingSystemInformation();
    survey.mainMemorySize = m_valueRetriever->getMainMemorySize();
    survey.cpu = m_valueRetriever->getCpuInformation();
    survey.gpus = m_valueRetriever->getGpuInformation();
    survey.openGL = m_valueRetriever->getOpenGLInformation();
    return survey;
}

} // namespace tibia::systemsurvey
