#include "authentication/loginprotocolmessagehandler.h"

#include "authentication/authenticationandencryptioninfo.h"

#include <QRegularExpression>
#include <QSysInfo>

#include <stdexcept>
#include <utility>

namespace tibia::authentication {

using namespace ::tibia::protobuf::protocol;

namespace {

/// The protocol version the login reports.
constexpr uint32_t ProtocolVersion = 0x604;
/// The client version the login reports (TGameClient::ClientVersion as text).
const char* const ClientVersion = "15.33";

/// The version string the login reports: the client version, then the build
/// commit when the build has one.
QString clientVersionString()
{
    QString version = QString::fromLatin1(ClientVersion);
#ifdef BUILD_COMMIT
    const QString commit = QStringLiteral(BUILD_COMMIT);
    if (!commit.isEmpty())
        version.append(QLatin1Char('.')).append(commit);
#endif
    return version;
}

}

TLoginProtocolMessageHandler::TLoginProtocolMessageHandler(
    std::shared_ptr<TAuthenticationAndEncryptionInfo> AuthenticationAndEncryptionInfo, const bool EpicDistribution)
    : QObject(nullptr)
    , m_authenticationAndEncryptionInfo(std::move(AuthenticationAndEncryptionInfo))
    , m_operatingSystem(clientOperatingSystem(EpicDistribution))
{
}

TLoginProtocolMessageHandler::~TLoginProtocolMessageHandler() = default;

uint32_t TLoginProtocolMessageHandler::clientOperatingSystem(const bool EpicDistribution)
{
    const QString productType = QSysInfo::productType();
    if (productType == QLatin1String("windows"))
        return EpicDistribution ? 8 : 5;
    if (productType == QLatin1String("macos"))
        return EpicDistribution ? 9 : 6;
    if (QSysInfo::kernelType() == QLatin1String("linux"))
        return 7;
    return 5;
}

uint32_t TLoginProtocolMessageHandler::clientVersionNumber(const QString& Version)
{
    QString digits = QString(Version).replace(QRegularExpression(QStringLiteral("\\D+")), QString());
    if (digits.size() > 4)
        digits.truncate(4);
    bool ok = false;
    const qlonglong number = digits.toLongLong(&ok, 10);
    if (!ok || number != qlonglong(int(number)))
        return 0;
    return uint32_t(number);
}

void TLoginProtocolMessageHandler::handleLoginChallenge(const GameserverMessageLoginChallenge& Challenge,
                                                        const uint32_t ConnectionType)
{
    auto& info = *m_authenticationAndEncryptionInfo;
    if (!info.isSecondaryLogin()) {
        // A new login: the server has to accept it again.
        m_loggedIn = false;
        GameclientMessageLogin login;
        login.set_client_os(m_operatingSystem);
        login.set_protocol_version(ProtocolVersion);
        login.set_client_version(clientVersionNumber(QString::fromLatin1(ClientVersion)));
        login.set_client_version_string(clientVersionString().toStdString());
        login.set_asset_identifier(info.assetsIdentifier().toStdString());
        login.set_preview_state(ConnectionType);
        login.set_character_name(info.characterName().toStdString());
        login.set_challenge_timestamp(Challenge.timestamp());
        login.set_challenge_random(Challenge.random());
        login.set_session_key(info.sessionKey().toStdString());
        info.generateXteaKey();
        const QByteArray key = info.xteaKey();
        for (int i = 0; i + 3 < key.size(); i += 4) {
            login.add_xtea_key(uint32_t(uint8_t(key[i])) | (uint32_t(uint8_t(key[i + 1])) << 8)
                               | (uint32_t(uint8_t(key[i + 2])) << 16) | (uint32_t(uint8_t(key[i + 3])) << 24));
        }
        // Host: a pack that listens for extended opcodes asks for the product
        // transport in the RSA padding.
        if (!m_productTransport.empty())
            login.set_product_transport(m_productTransport);
        Q_EMIT gameLoginChallengeMessage();
        emitSendLoginMessage(login);
        return;
    }

    GameclientMessageSecondaryLogin secondaryLogin;
    secondaryLogin.set_challenge_connection(ConnectionType);
    secondaryLogin.set_first_string(info.secondaryLoginFirstString().toStdString());
    secondaryLogin.set_token(info.secondaryLoginToken().toStdString());
    emitSendSecondaryLoginMessage(secondaryLogin);
}

void TLoginProtocolMessageHandler::handleAnyMessage(std::shared_ptr<GameserverMessage> pGameserverMessage)
{
    if (!pGameserverMessage
        || !pGameserverMessage->HasExtension(GameserverMessageLoginChallenge::gameserver_message_login_challenge))
        return;
    const GameserverMessageLoginChallenge challenge =
        pGameserverMessage->GetExtension(GameserverMessageLoginChallenge::gameserver_message_login_challenge);
    handleLoginChallenge(challenge, uint32_t(pGameserverMessage->meta_data().value3()));
}

void TLoginProtocolMessageHandler::handleLoginErrorMessage(const GameserverMessageLoginError& Message)
{
    const QString error = QString::fromStdString(Message.error());
    // The server's reason is the action: 0 back to the character selection,
    // 1 back to the login, 2 quit.
    const uint32_t reason = Message.reason();
    if (reason > 2)
        throw std::runtime_error("unhandled enum value");
    Q_EMIT gameLoginErrorMessage(error, int(reason));
}

void TLoginProtocolMessageHandler::handleLoginWaitMessage(const GameserverMessageLoginWait& Message)
{
    Q_EMIT gameLoginWaitMessage(QString::fromStdString(Message.message()), int(Message.time()));
}

void TLoginProtocolMessageHandler::handleLoginAdviceMessage(const GameserverMessageLoginAdvice& Message)
{
    Q_EMIT gameLoginAdviceMessage(QString::fromStdString(Message.message()));
}

void TLoginProtocolMessageHandler::handleLoginSuccessMessage(const GameserverMessageLoginSuccess& /*Message*/)
{
    m_loggedIn = true;
}

void TLoginProtocolMessageHandler::handleReadyForSecondaryConnectionMessage(
    const GameserverMessageReadyForSecondaryConnection& Message)
{
    m_authenticationAndEncryptionInfo->setSecondaryLogin();
    m_authenticationAndEncryptionInfo->setSecondaryLoginToken(QString::fromStdString(Message.token()));
    Q_EMIT gameReadyForSecondaryConnection();
}

void TLoginProtocolMessageHandler::emitSendLoginMessage(const GameclientMessageLogin& Message)
{
    Q_EMIT sendLoginMessage(Message);
}

void TLoginProtocolMessageHandler::emitSendSecondaryLoginMessage(const GameclientMessageSecondaryLogin& Message)
{
    Q_EMIT sendSecondaryLoginMessage(Message);
}

bool TLoginProtocolMessageHandler::isLoggedIn() const
{
    return m_loggedIn;
}

void TLoginProtocolMessageHandler::setProductTransport(const std::string& ProductTransport)
{
    m_productTransport = ProductTransport;
}

}
