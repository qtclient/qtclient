#pragma once

#include "jsondocumentuploaderbase.h"

#include <QUrl>

#include <memory>

namespace systemsurvey {

class TSystemSurveyAsJsonDocumentCreator;

/// Uploads this machine's survey document to the survey url.
class TSystemSurveyUploader : public tibia::shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TSystemSurveyUploader(const QUrl& url, std::shared_ptr<TSystemSurveyAsJsonDocumentCreator> documentCreator,
                          std::shared_ptr<tibia::shared::TJsonDocumentHttpPostSender> sender, QObject* parent = nullptr);
    ~TSystemSurveyUploader() override;

    /// Creates the survey document and posts it.
    virtual void uploadSystemSurvey();

private:
    QUrl m_url;
    std::shared_ptr<TSystemSurveyAsJsonDocumentCreator> m_documentCreator;
};

} // namespace systemsurvey
