#include "showoffrequestuploader.h"

#include "shared/clientservicesuploader.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>

namespace tibia::game {

TShowOffRequestUploader::TShowOffRequestUploader(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void TShowOffRequestUploader::requestUpdate()
{
    shared::postClientService(m_network, this, m_url, QStringLiteral("showoff"), {}, m_revision,
                              [this](bool ok, QJsonDocument doc) {
                                  onFinished(ok, doc.isObject() ? doc.object() : QJsonObject());
                              });
}

void TShowOffRequestUploader::onFinished(const bool ok, const QJsonObject& reply)
{
    if (!ok || reply.isEmpty())
        return;
    m_reply = reply;
    m_imageUrl = reply.value(QStringLiteral("image")).toString();
    m_title = reply.value(QStringLiteral("title")).toString(
        reply.value(QStringLiteral("titletext")).toString());
    m_description = reply.value(QStringLiteral("description")).toString(
        reply.value(QStringLiteral("subtitletext")).toString());
    Q_EMIT showOffReceived();
}

}
