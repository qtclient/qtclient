#pragma once

#include <QAuthenticator>
#include <QNetworkReply>
#include <QObject>

namespace tibia {
namespace shared {

/// CIP `shared::TNetworkAccessManagerAuthenticationHandler`: answers a
/// network access manager's authentication requests. CipSoft's handler
/// answers none.
class TNetworkAccessManagerAuthenticationHandler : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    void handleAuthenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator);
};

}
}
