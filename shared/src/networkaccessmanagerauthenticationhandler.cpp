#include "networkaccessmanagerauthenticationhandler.h"

namespace tibia::shared {

void TNetworkAccessManagerAuthenticationHandler::handleAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(reply);
    Q_UNUSED(authenticator);
}

}
