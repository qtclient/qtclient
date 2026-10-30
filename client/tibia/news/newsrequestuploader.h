#pragma once

#include <cip_gen.h>

#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace tibia::news {

class TNewsStorage;

/// POSTs `{type:"news"}` to the portal clientservices multiplex
/// (CIP: TNewsRequestUploader). Parses CIP `{gamenews:[…]}` into TNewsStorage.
class TNewsRequestUploader final : public cip::tibia::news::TNewsRequestUploader
{
    Q_OBJECT
public:
    explicit TNewsRequestUploader(TNewsStorage* storage, QObject* parent = nullptr);

    void setServiceUrl(const QUrl& url) { m_url = url; }
    void setClientRevision(const QString& revision) { m_revision = revision; }

    QJsonObject reply() const { return m_reply; }
    TNewsStorage* storage() const { return m_storage; }

    /// CIP has no generated `requestUpdate` on this class; product uses the same shape.
    void requestUpdate();
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD) override;

Q_SIGNALS:
    void newsReceived();

private:
    QNetworkAccessManager* m_network;
    QPointer<TNewsStorage> m_storage;
    QUrl m_url;
    QString m_revision;
    QJsonObject m_reply;
};

}
