#include "systemsurvey/fpshistoryuploader.h"

#include "jsondocumenthttppostsender.h"
#include "systemsurvey/fpshistorytojsonconverter.h"

namespace tibia::systemsurvey {

TFPSHistoryUploader::TFPSHistoryUploader(const QUrl& url, std::shared_ptr<TFPSHistoryToJsonConverter> converter,
                                         std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender, QObject* parent)
    : TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_url(url)
    , m_converter(std::move(converter))
{
}

TFPSHistoryUploader::~TFPSHistoryUploader() = default;

void TFPSHistoryUploader::uploadHistory(const TFPSHistory& history, const int renderer)
{
    upload(m_url, m_converter->convert(history, renderer));
}

}
