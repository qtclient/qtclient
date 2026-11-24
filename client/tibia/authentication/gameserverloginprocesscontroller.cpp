#include "authentication/gameserverloginprocesscontroller.h"

#include "authentication/authenticationandencryptioninfo.h"
#include "network/dualconnectionurlinfo.h"
#include "network/gameserverdualconnection.h"

#include "crashhandler.h"
#include "hashhelper.h"

#include <QDebug>

namespace tibia::authentication {

TGameserverLoginProcessController::TGameserverLoginProcessController(
    std::shared_ptr<network::TGameserverDualConnection> DualConnection,
    std::shared_ptr<TAuthenticationAndEncryptionInfo> AuthenticationAndEncryptionInfo,
    std::shared_ptr<shared::THashHelper> HashHelper)
    : QObject(nullptr)
    , m_pDualConnection(std::move(DualConnection))
    , m_pAuthenticationAndEncryptionInfo(std::move(AuthenticationAndEncryptionInfo))
    , m_pHashHelper(std::move(HashHelper))
{
    using C = TGameserverLoginProcessController;
    using Connection = network::TGameserverDualConnection;
    connect(m_pDualConnection.get(), &Connection::connected, this, &C::onGameserverTCPConnectionConnected);
    connect(m_pDualConnection.get(), &Connection::secondaryConnected, this, &C::onGameserverTCPConnectionSecondaryConnected);
    connect(m_pDualConnection.get(), &Connection::error, this, &C::connectionError);
    connect(m_pDualConnection.get(), &Connection::disconnected, this, &C::onGameserverTCPConnectionDisconnected);
}

TGameserverLoginProcessController::~TGameserverLoginProcessController() = default;

void TGameserverLoginProcessController::login(const TGameserverLoginRequestData& LoginData)
{
    const QString protectedUrl =
        QStringLiteral("tcp://%1:%2").arg(LoginData.worldAddressProtected).arg(LoginData.worldPortProtected);
    const QString unprotectedUrl =
        QStringLiteral("tcp://%1:%2").arg(LoginData.worldAddressUnprotected).arg(LoginData.worldPortUnprotected);
    shared::TCrashHandler::instance().setAnnotation(QStringLiteral("ProtectedWorldAddress"), protectedUrl);
    shared::TCrashHandler::instance().setAnnotation(QStringLiteral("UnprotectedWorldAddress"), unprotectedUrl);
    m_pLoginData = std::make_shared<TGameserverLoginRequestData>(LoginData);
    qInfo() << "Request connection to gameserver " << protectedUrl << " (unprotected: " << unprotectedUrl << ")"
            << " requested (Charakter" << m_pLoginData->characterName << ")";

    m_pAuthenticationAndEncryptionInfo->setCharacterName(m_pLoginData->characterName);
    m_pAuthenticationAndEncryptionInfo->setSessionKey(m_pLoginData->sessionKey);
    m_pAuthenticationAndEncryptionInfo->setEMail(m_pLoginData->email);
    m_pAuthenticationAndEncryptionInfo->setPassword(m_pLoginData->password);
    const QByteArray sessionKeyHash =
        m_pHashHelper->getSha1Hash(m_pAuthenticationAndEncryptionInfo->sessionKey().toLatin1()).toHex().toUpper();
    m_pAuthenticationAndEncryptionInfo->setSecondaryLoginFirstString(QString::fromUtf8(sessionKeyHash));
    m_pAuthenticationAndEncryptionInfo->setAssetsIdentifier(m_pLoginData->assetsIdentifier);

    QUrl secondaryUrl(unprotectedUrl);
    // Host: CrystalServer serves both game connections of a world on one
    // address and port; the fragment keeps the two URLs apart.
    if (secondaryUrl == QUrl(protectedUrl))
        secondaryUrl.setFragment(QStringLiteral("secondary"));
    auto urlInfo = std::make_shared<network::TDualConnectionUrlInfo>(QUrl(protectedUrl), secondaryUrl);
    m_pDualConnection->connectToGameserver(urlInfo, LoginData.worldName);
}

void TGameserverLoginProcessController::onGameserverTCPConnectionConnected(const QUrl Url)
{
    qInfo() << "Connected to gameserver " << Url.url() << m_pLoginData->worldName;
    Q_EMIT connected();
}

void TGameserverLoginProcessController::onGameserverTCPConnectionSecondaryConnected(const QUrl Url)
{
    qInfo() << "Backup connection to gameserver " << Url.url() << m_pLoginData->worldName << "established";
}

void TGameserverLoginProcessController::onGameserverTCPConnectionDisconnected()
{
    qInfo() << "Disconnected from gameserver";
    Q_EMIT disconnected();
}

}
