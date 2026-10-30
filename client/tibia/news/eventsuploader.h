#pragma once

#include <cip_gen.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace tibia::news {

/// POSTs `{type:"eventschedule"}` to the portal clientservices multiplex
/// (CIP: TEventsUploader). The CIP `updated(std::vector<TEventInformation>)`
/// signal is not generated yet; callers read `eventList()` after `finished`.
class TEventsUploader final : public cip::tibia::news::TEventsUploader
{
    Q_OBJECT
public:
    explicit TEventsUploader(QObject* parent = nullptr);

    void setServiceUrl(const QUrl& url) { m_url = url; }
    void setClientRevision(const QString& revision) { m_revision = revision; }

    QJsonArray eventList() const { return m_eventList; }
    qint64 lastUpdateTimestamp() const { return m_lastUpdateTimestamp; }

    void requestUpdate() override;
    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD) override;

Q_SIGNALS:
    /// Scaffolding stand-in for CIP `updated(std::vector<TEventInformation>)`.
    void scheduleUpdated();

private:
    QNetworkAccessManager* m_network;
    QUrl m_url;
    QString m_revision;
    QJsonArray m_eventList;
    qint64 m_lastUpdateTimestamp = 0;
};

}
