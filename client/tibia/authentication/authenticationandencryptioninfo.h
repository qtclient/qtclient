#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

namespace tibia {
namespace shared {
class TXteaHelper;
}
namespace authentication {

/// CIP authentication::TAuthenticationAndEncryptionInfo: what the game server
/// login needs: the session's XTEA key (with the helper that encrypts with
/// it), the login credentials and, once the server allowed a secondary
/// connection, its token.
class TAuthenticationAndEncryptionInfo
{
public:
    TAuthenticationAndEncryptionInfo();
    virtual ~TAuthenticationAndEncryptionInfo();

    /// A new random 16-byte XTEA key, given to the XTEA helper.
    virtual void generateXteaKey();
    virtual QByteArray xteaKey() const;
    /// Encrypts with the key (32-bit words little-endian).
    virtual std::shared_ptr<shared::TXteaHelper> xteaHelper() const;

    virtual void setCharacterName(const QString& CharacterName);
    virtual QString characterName() const;
    virtual void setSessionKey(const QString& SessionKey);
    virtual QString sessionKey() const;
    /// The account's email address and password (the game server login's
    /// credentials for a login without a session key).
    virtual void setEMail(const QString& EMail);
    virtual QString eMail() const;
    virtual void setPassword(const QString& Password);
    virtual QString password() const;
    /// The first string of the secondary login's RSA block (the game server
    /// login sets the session key's SHA-1, in upper-case hex).
    virtual void setSecondaryLoginFirstString(const QString& FirstString);
    virtual QString secondaryLoginFirstString() const;
    /// The token of ReadyForSecondaryConnection, sent back by the secondary login.
    virtual void setSecondaryLoginToken(const QString& Token);
    virtual QString secondaryLoginToken() const;
    /// The second string of the login message (Crystal reads it as the assets identifier).
    virtual void setAssetsIdentifier(const QString& AssetsIdentifier);
    virtual QString assetsIdentifier() const;
    /// The next login challenge is answered with a secondary login.
    virtual bool isSecondaryLogin() const;
    virtual void setSecondaryLogin();

private:
    QByteArray m_xteaKey;
    std::shared_ptr<shared::TXteaHelper> m_xteaHelper;
    QString m_characterName;
    QString m_sessionKey;
    QString m_eMail;
    QString m_password;
    QString m_secondaryLoginFirstString;
    QString m_secondaryLoginToken;
    QString m_assetsIdentifier;
    bool m_secondaryLogin = false;
};

}
}
