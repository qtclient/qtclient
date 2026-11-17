#pragma once

#include <QJsonDocument>

#include <memory>

namespace tibia::systemsurvey {

class TSystemSurveyCreator;
class TSystemSurveyToJsonConverter;

/// Creates the survey and converts it to its JSON document.
class TSystemSurveyAsJsonDocumentCreator
{
public:
    TSystemSurveyAsJsonDocumentCreator(std::shared_ptr<TSystemSurveyCreator> creator,
                                       std::shared_ptr<TSystemSurveyToJsonConverter> converter);
    virtual ~TSystemSurveyAsJsonDocumentCreator() = default;

    virtual QJsonDocument createJsonDocument() const;

    /// The survey document of this machine, from the default retriever.
    static QJsonDocument createDefaultJsonDocument();

private:
    std::shared_ptr<TSystemSurveyCreator> m_creator;
    std::shared_ptr<TSystemSurveyToJsonConverter> m_converter;
};

} // namespace tibia::systemsurvey
