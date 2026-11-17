#pragma once

#include "jsondocumentuploaderbase.h"

#include <QUrl>

#include <memory>

namespace tibia {
namespace systemsurvey {

class TFPSHistory;
class TFPSHistoryToJsonConverter;

/// CIP `systemsurvey::TFPSHistoryUploader`: posts an FPS history document to its url.
class TFPSHistoryUploader : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TFPSHistoryUploader(const QUrl& url, std::shared_ptr<TFPSHistoryToJsonConverter> converter,
                        std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender, QObject* parent = nullptr);
    ~TFPSHistoryUploader() override;

    virtual void uploadHistory(const TFPSHistory& history, int renderer);

private:
    QUrl m_url;
    std::shared_ptr<TFPSHistoryToJsonConverter> m_converter;
};

}
}
