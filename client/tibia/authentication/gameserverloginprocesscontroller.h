#pragma once

#include "authentication/authenticationjsondocumenthelper.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace tibia {
namespace shared {
class THashHelper;
}
namespace network {
class TGameserverDualConnection;
}
namespace authentication {

class TAuthenticationAndEncryptionInfo;

/// CIP authentication::TGameserverLoginRequestData: what a character's game
/// server login needs: the account's login data and the character's world.
struct TGameserverLoginRequestData : TLoginRequestData
{
    QString characterName;
    QString worldAddressUnprotected;
    QString worldAddressProtected;
    int worldPortUnprotected = 0;
    int worldPortProtected = 0;
    QString worldName;
    bool antiCheatProtection = false;
    bool tutorial = false;
    bool isMale = false;
    /// The second string of the login message (the assets identifier).
    QString assetsIdentifier;
};

/// CIP authentication::TGameserverLoginProcessController: logs a character in
/// at its world's game server. It hands the login's credentials and keys to
/// the authentication and encryption info, connects the dual connection to
/// the world's protected and unprotected addresses and reports (and logs)
/// the connection's state.
class TGameserverLoginProcessController : public QObject
{
    Q_OBJECT
public:
    TGameserverLoginProcessController(std::shared_ptr<network::TGameserverDualConnection> DualConnection,
                                      std::shared_ptr<TAuthenticationAndEncryptionInfo> AuthenticationAndEncryptionInfo,
                                      std::shared_ptr<shared::THashHelper> HashHelper);
    ~TGameserverLoginProcessController() override;

    /// CIP: the character of `LoginData` logs in: the crash reports name the
    /// world's addresses, the authentication info gets the character, the
    /// session key and its hash, the account and the assets identifier, and
    /// the dual connection connects to tcp://protected and tcp://unprotected.
    void login(const TGameserverLoginRequestData& LoginData);

Q_SIGNALS:
    void connected();
    void disconnected();
    void connectionError(QString SocketErrorDescription);

public Q_SLOTS:
    void onGameserverTCPConnectionConnected(QUrl Url);
    void onGameserverTCPConnectionSecondaryConnected(QUrl Url);
    void onGameserverTCPConnectionDisconnected();

private:
    std::shared_ptr<network::TGameserverDualConnection> m_pDualConnection;
    /// The data of the last login.
    std::shared_ptr<TGameserverLoginRequestData> m_pLoginData;
    std::shared_ptr<TAuthenticationAndEncryptionInfo> m_pAuthenticationAndEncryptionInfo;
    std::shared_ptr<shared::THashHelper> m_pHashHelper;
};

}
}
