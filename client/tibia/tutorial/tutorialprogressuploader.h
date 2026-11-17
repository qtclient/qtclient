#pragma once

#include "jsondocumentuploaderbase.h"

#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace tutorial {

/// CIP `tutorial::TTutorialProgressUploader`: posts the tutorial progress of a
/// character ({clientsessionid, charactername, progress}) to its url.
class TTutorialProgressUploader : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    TTutorialProgressUploader(const QUrl& url, const QString& clientSessionId, const QString& characterName,
                              std::shared_ptr<shared::TJsonDocumentHttpPostSender> sender, QObject* parent = nullptr);
    ~TTutorialProgressUploader() override;

    virtual void uploadProgress(const QString& progress);

Q_SIGNALS:
    void onTutorialProgressError();

public Q_SLOTS:
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD);

private:
    QUrl m_url;
    QString m_clientSessionId;
    QString m_characterName;
};

}
}
