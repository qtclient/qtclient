#include "qnetworkaccessmanagerwrapper.h"

#include "qnetworkreplywrapper.h"

namespace tibia::shared {

QNetworkAccessManagerWrapper::QNetworkAccessManagerWrapper(std::shared_ptr<QNetworkAccessManager> networkAccessManager)
    : QObject(nullptr)
    , m_networkAccessManager(std::move(networkAccessManager))
{
    if (m_networkAccessManager) {
        connect(m_networkAccessManager.get(), &QNetworkAccessManager::authenticationRequired, this,
            &QNetworkAccessManagerWrapper::authenticationRequired);
    }
}

QNetworkReplyWrapper* QNetworkAccessManagerWrapper::get(const QNetworkRequest& request)
{
    return new QNetworkReplyWrapper(m_networkAccessManager->get(request));
}

void QNetworkAccessManagerWrapper::setCache(QAbstractNetworkCache* cache)
{
    m_networkAccessManager->setCache(cache);
}

void QNetworkAccessManagerWrapper::setRedirectPolicy(const QNetworkRequest::RedirectPolicy policy)
{
    m_networkAccessManager->setRedirectPolicy(policy);
}

}
