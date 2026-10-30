#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace tibia::game {

/// POSTs `{type:"showoff"}` to the portal clientservices multiplex (CipSoft
/// start-screen promo / bottom-menu showoff). No cip_gen base class yet.
class TShowOffRequestUploader final : public QObject
{
    Q_OBJECT
public:
    explicit TShowOffRequestUploader(QObject* parent = nullptr);

    void setServiceUrl(const QUrl& url) { m_url = url; }
    void setClientRevision(const QString& revision) { m_revision = revision; }

    QJsonObject reply() const { return m_reply; }
    QString imageUrl() const { return m_imageUrl; }
    QString title() const { return m_title; }
    QString description() const { return m_description; }

    void requestUpdate();

Q_SIGNALS:
    void showOffReceived();

private:
    void onFinished(bool ok, const QJsonObject& reply);

    QNetworkAccessManager* m_network;
    QUrl m_url;
    QString m_revision;
    QJsonObject m_reply;
    QString m_imageUrl;
    QString m_title;
    QString m_description;
};

}
