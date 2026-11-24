#include "config/clientoptionsuploader.h"

#include "client/gameapplication.h"
#include "jsondocumenthttppostsender.h"
#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace tibia::config {

TClientOptionsUploader::TClientOptionsUploader(QUrl Url)
    : m_url(std::move(Url))
    , m_pUploader(std::make_shared<shared::TJsonDocumentUploaderBase>(std::make_shared<shared::TJsonDocumentHttpPostSender>()))
{
}

TClientOptionsUploader::~TClientOptionsUploader() = default;

void TClientOptionsUploader::upload(const QString& Account, const QString& ExportedOptions)
{
    if (m_uploaded)
        return;
    QJsonObject object;
    object.insert(QStringLiteral("dataType"), 4);
    object.insert(QStringLiteral("clientID"), client::TGameApplication::hardwareId());
    object.insert(QStringLiteral("account"), Account);
    object.insert(QStringLiteral("config"), ExportedOptions);
    QJsonDocument document;
    document.setObject(object);
    m_pUploader->upload(m_url, document);
    m_uploaded = true;
}

}
