#pragma once

#include <cip_gen.h>

#include <QJsonObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace tibia::game {

/// POSTs `{type:"boostedcreature"}` to the portal clientservices multiplex
/// (CIP: TBoostedCreatureRequestUploader).
class TBoostedCreatureRequestUploader final : public cip::tibia::game::TBoostedCreatureRequestUploader
{
    Q_OBJECT
public:
    explicit TBoostedCreatureRequestUploader(QObject* parent = nullptr);

    void setServiceUrl(const QUrl& url) { m_url = url; }
    void setClientRevision(const QString& revision) { m_revision = revision; }

    /// Last successful reply object (empty until the first success).
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
