#pragma once

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace tibia::game {

/// POSTs `{type:"cacheinfo"}` to the portal clientservices multiplex
/// (CIP: TLiveInformationRequestUploader).
class TLiveInformationRequestUploader final : public cip::tibia::game::TLiveInformationRequestUploader
{
    Q_OBJECT
public:
    explicit TLiveInformationRequestUploader(QObject* parent = nullptr);

    void setServiceUrl(const QUrl& url) { m_url = url; }
    void setClientRevision(const QString& revision) { m_revision = revision; }

    QJsonObject reply() const { return m_reply; }

    void requestUpdate() override;
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD) override;

private:
    QNetworkAccessManager* m_network;
    QUrl m_url;
    QString m_revision;
    QJsonObject m_reply;
};

}
