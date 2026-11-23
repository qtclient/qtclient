#include "authentication/authenticationandencryptioninfo.h"

#include "xteahelper.h"

#include <QDataStream>
#include <QRandomGenerator>

namespace tibia::authentication {

TAuthenticationAndEncryptionInfo::TAuthenticationAndEncryptionInfo()
    : m_xteaKey(16, '\0')
    , m_xteaHelper(std::make_shared<shared::TXteaHelper>(QDataStream::LittleEndian))
{
}

TAuthenticationAndEncryptionInfo::~TAuthenticationAndEncryptionInfo() = default;

void TAuthenticationAndEncryptionInfo::generateXteaKey()
{
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(m_xteaKey.data()), m_xteaKey.size() / 4);
    m_xteaHelper->setKey(xteaKey());
}

QByteArray TAuthenticationAndEncryptionInfo::xteaKey() const
{
    return QByteArray(m_xteaKey.constData(), m_xteaKey.size());
}

std::shared_ptr<shared::TXteaHelper> TAuthenticationAndEncryptionInfo::xteaHelper() const
{
    return m_xteaHelper;
}

void TAuthenticationAndEncryptionInfo::setCharacterName(const QString& CharacterName)
{
    m_characterName = CharacterName;
}

QString TAuthenticationAndEncryptionInfo::characterName() const
{
    return m_characterName;
}

void TAuthenticationAndEncryptionInfo::setSessionKey(const QString& SessionKey)
{
    m_sessionKey = SessionKey;
}

QString TAuthenticationAndEncryptionInfo::sessionKey() const
{
    return m_sessionKey;
}

void TAuthenticationAndEncryptionInfo::setSecondaryLoginToken(const QString& Token)
{
    m_secondaryLoginToken = Token;
}

QString TAuthenticationAndEncryptionInfo::secondaryLoginToken() const
{
    return m_secondaryLoginToken;
}

void TAuthenticationAndEncryptionInfo::setAssetsIdentifier(const QString& AssetsIdentifier)
{
    m_assetsIdentifier = AssetsIdentifier;
}

QString TAuthenticationAndEncryptionInfo::assetsIdentifier() const
{
    return m_assetsIdentifier;
}

bool TAuthenticationAndEncryptionInfo::isSecondaryLogin() const
{
    return m_secondaryLogin;
}

void TAuthenticationAndEncryptionInfo::setSecondaryLogin()
{
    m_secondaryLogin = true;
}

}
