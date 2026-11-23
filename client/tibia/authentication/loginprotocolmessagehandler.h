#pragma once

#include <protocol.pb.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>

namespace tibia {
namespace authentication {

class TAuthenticationAndEncryptionInfo;

/// CIP authentication::TLoginProtocolMessageHandler: the game server login.
/// It answers the server's login challenge with the login (or, once the
/// server allowed a secondary connection, with the secondary login), passes
/// the server's login error, wait and advice messages on to the session, and
/// keeps the secondary login's token in the authentication info.
class TLoginProtocolMessageHandler : public QObject
{
    Q_OBJECT
public:
    /// `EpicDistribution`: the client is the Epic Games Store distribution
    /// (it reports its own operating system ids).
    TLoginProtocolMessageHandler(std::shared_ptr<TAuthenticationAndEncryptionInfo> AuthenticationAndEncryptionInfo,
                                 bool EpicDistribution);
    ~TLoginProtocolMessageHandler() override;

    /// Answers a login challenge that arrived on connection `ConnectionType`
    /// (0 primary, 1 secondary).
    virtual void handleLoginChallenge(const ::tibia::protobuf::protocol::GameserverMessageLoginChallenge& Challenge,
                                      uint32_t ConnectionType);
    /// Every server message: login challenges are answered.
    virtual void handleAnyMessage(std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage);
    virtual void handleLoginErrorMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginError& Message);
    virtual void handleLoginWaitMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginWait& Message);
    virtual void handleLoginAdviceMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginAdvice& Message);
    virtual void handleLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& Message);
    virtual void handleReadyForSecondaryConnectionMessage(
        const ::tibia::protobuf::protocol::GameserverMessageReadyForSecondaryConnection& Message);
    virtual void emitSendLoginMessage(const ::tibia::protobuf::protocol::GameclientMessageLogin& Message);
    virtual void emitSendSecondaryLoginMessage(
        const ::tibia::protobuf::protocol::GameclientMessageSecondaryLogin& Message);
    /// The server accepted the login (LoginSuccess) and no new login was sent since.
    virtual bool isLoggedIn() const;

    /// The operating system id the login reports: Windows 5, macOS 6, Linux 7
    /// (Epic distribution: Windows 8, macOS 9); anything else reports 5.
    static uint32_t clientOperatingSystem(bool EpicDistribution);
    /// The numeric client version of a version string ("15.33" -> 1533): its
    /// digits, at most four of them.
    static uint32_t clientVersionNumber(const QString& Version);

    /// Host: the product transport the login asks for in the RSA padding
    /// (empty: none, as the CIP client).
    void setProductTransport(const std::string& ProductTransport);

Q_SIGNALS:
    void sendLoginMessage(tibia::protobuf::protocol::GameclientMessageLogin LoginMessage);
    void sendSecondaryLoginMessage(tibia::protobuf::protocol::GameclientMessageSecondaryLogin SecondaryLoginMessage);
    void gameLoginChallengeMessage();
    /// `Action`: 0 back to the character selection, 1 back to the login, 2 quit.
    void gameLoginErrorMessage(QString ErrorMessage, int Action);
    void gameLoginWaitMessage(QString WaitMessage, int WaitSecondsUntilNextTry);
    void gameLoginAdviceMessage(QString AdviceMessage);
    void gameReadyForSecondaryConnection();

private:
    std::shared_ptr<TAuthenticationAndEncryptionInfo> m_authenticationAndEncryptionInfo;
    uint32_t m_operatingSystem = 0;
    bool m_loggedIn = false;
    std::string m_productTransport;
};

}
}
