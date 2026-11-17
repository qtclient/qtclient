#include "systemsurveyuploader.h"

#include "jsondocumenthttppostsender.h"
#include "systemsurveyasjsondocumentcreator.h"

namespace tibia::systemsurvey {

TSystemSurveyUploader::TSystemSurveyUploader(const QUrl& url,
                                             std::shared_ptr<TSystemSurveyAsJsonDocumentCreator> documentCreator,
                                             std::shared_ptr<tibia::shared::TJsonDocumentHttpPostSender> sender,
                                             QObject* parent)
    : TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_url(url)
    , m_documentCreator(std::move(documentCreator))
{
}

TSystemSurveyUploader::~TSystemSurveyUploader() = default;

void TSystemSurveyUploader::uploadSystemSurvey()
{
    upload(m_url, m_documentCreator->createJsonDocument());
}

} // namespace tibia::systemsurvey
