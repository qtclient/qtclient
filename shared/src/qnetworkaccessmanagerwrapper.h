#pragma once

#include <QAuthenticator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

#include <memory>

namespace tibia {
namespace shared {

class QNetworkReplyWrapper;

/// CIP `shared::QNetworkAccessManagerWrapper`: a shared QNetworkAccessManager
/// behind virtual calls that hand out wrapped replies.
class QNetworkAccessManagerWrapper : public QObject
{
    Q_OBJECT

public:
    explicit QNetworkAccessManagerWrapper(std::shared_ptr<QNetworkAccessManager> networkAccessManager);
    ~QNetworkAccessManagerWrapper() override = default;

    /// The wrapper is a child of the reply.
    virtual QNetworkReplyWrapper* get(const QNetworkRequest& request);
    virtual void setCache(QAbstractNetworkCache* cache);
    virtual void setRedirectPolicy(QNetworkRequest::RedirectPolicy policy);

Q_SIGNALS:
    void authenticationRequired(QNetworkReply* pReply, QAuthenticator* pAuthenticator);

private:
    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
};

}
}
