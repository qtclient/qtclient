#include "jsondocumentuploaderbase.h"

#include "jsondocumenthttppostsender.h"

#include <QJsonObject>

namespace tibia::shared {

TJsonDocumentUploaderBase::TJsonDocumentUploaderBase(std::shared_ptr<TJsonDocumentHttpPostSender> sender, QObject* parent)
    : QObject(parent)
    , m_sender(std::move(sender))
{
    if (!m_sender)
        return;
    connect(m_sender.get(), &TJsonDocumentHttpPostSender::finished, this, &TJsonDocumentUploaderBase::onFinished);
    connect(m_sender.get(), &TJsonDocumentHttpPostSender::authenticationRequired, this,
        &TJsonDocumentUploaderBase::authenticationRequired);
}

TJsonDocumentUploaderBase::~TJsonDocumentUploaderBase() = default;

void TJsonDocumentUploaderBase::upload(const QUrl& url, const QJsonDocument& document)
{
    m_sender->send(url, document);
}

void TJsonDocumentUploaderBase::onFinished(const QNetworkReply::NetworkError NetworkErrorCode, QString NetworkErrorString,
    QByteArray ReplyFromServer)
{
    QJsonDocument reply;
    m_errorMessage = QString();
    m_errorCode = 0;
    bool successful = false;
    if (NetworkErrorCode == QNetworkReply::NoError) {
        reply = QJsonDocument::fromJson(ReplyFromServer);
        if (reply.object()[QStringLiteral("errorCode")].toInt(0) == 0) {
            successful = true;
        } else {
            m_errorMessage = reply.object()[QStringLiteral("errorMessage")].toString(QString());
            m_errorCode = reply.object()[QStringLiteral("errorCode")].toInt(0);
        }
    } else {
        m_errorMessage = NetworkErrorString;
    }
    Q_EMIT finished(successful, reply);
}

QString TJsonDocumentUploaderBase::errorMessage() const
{
    return m_errorMessage;
}

int TJsonDocumentUploaderBase::errorCode() const
{
    return m_errorCode;
}

}
