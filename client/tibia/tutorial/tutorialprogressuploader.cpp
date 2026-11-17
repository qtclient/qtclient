#include "tutorial/tutorialprogressuploader.h"

#include <QJsonObject>

namespace tibia::tutorial {

namespace {

const QString ClientSessionIdKey = QStringLiteral("clientsessionid");
const QString CharacterNameKey = QStringLiteral("charactername");
const QString ProgressKey = QStringLiteral("progress");

}

TTutorialProgressUploader::TTutorialProgressUploader(const QUrl& url, const QString& clientSessionId,
                                                     const QString& characterName,
                                                     std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender,
                                                     QObject* parent)
    : TJsonDocumentUploaderBase(std::move(sender), parent)
    , m_url(url)
    , m_clientSessionId(clientSessionId)
    , m_characterName(characterName)
{
    connect(this, &TJsonDocumentUploaderBase::finished, this, &TTutorialProgressUploader::onFinished);
}

TTutorialProgressUploader::~TTutorialProgressUploader() = default;

void TTutorialProgressUploader::uploadProgress(const QString& progress)
{
    QJsonObject object;
    object.insert(ClientSessionIdKey, m_clientSessionId);
    object.insert(CharacterNameKey, m_characterName);
    object.insert(ProgressKey, progress);
    upload(m_url, QJsonDocument(object));
}

void TTutorialProgressUploader::onFinished(const bool UploadSuccessful, const QJsonDocument&)
{
    if (!UploadSuccessful)
        Q_EMIT onTutorialProgressError();
}

}
