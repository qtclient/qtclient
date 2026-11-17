#include "systemsurveyasjsondocumentcreator.h"

#include "systemsurveycreator.h"
#include "systemsurveytojsonconverter.h"
#include "systemsurvey/systemsurveyvalueretriever.h"

namespace tibia::systemsurvey {

TSystemSurveyAsJsonDocumentCreator::TSystemSurveyAsJsonDocumentCreator(
    std::shared_ptr<TSystemSurveyCreator> creator, std::shared_ptr<TSystemSurveyToJsonConverter> converter)
    : m_creator(std::move(creator))
    , m_converter(std::move(converter))
{
}

QJsonDocument TSystemSurveyAsJsonDocumentCreator::createJsonDocument() const
{
    return m_converter->convert(m_creator->createSystemSurvey());
}

QJsonDocument TSystemSurveyAsJsonDocumentCreator::createDefaultJsonDocument()
{
    auto creator = std::make_shared<TSystemSurveyCreator>(std::make_shared<TSystemSurveyValueRetriever>());
    const TSystemSurveyAsJsonDocumentCreator documentCreator(creator,
                                                             std::make_shared<TSystemSurveyToJsonConverter>());
    return documentCreator.createJsonDocument();
}

} // namespace tibia::systemsurvey
