#pragma once

#include "authentication/cloudflarechallengetypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace authentication {

/// What a login request carries. Optional parts are sent only when set.
struct TLoginRequestData
{
    QString email;
    QString password;
    /// Authenticator token.
    QString token;
    QString trustedDeviceToken;
    QString deviceVerificationCode;
    QString emailCode;
    QString loginConfirmationCode;
    QString loginConfirmationToken;
    bool stayLoggedIn = false;
    /// The play session of the last successful login (the character selection
    /// takes it from the login answer).
    QString sessionKey;
    QString deviceCookie;
    int clientType = 0;
    QString clientVersion;
    QString assetVersion;
    /// The clearance of a solved Cloudflare challenge; the login request is
    /// sent with it, or without one when unset.
    std::optional<TCloudflareChallengeClearanceInfo> cloudflareClearance;
};

/// One "playdata.worlds" entry.
struct TWorld
{
    int id = 0;
    QString name;
    QString externalAddressProtected;
    QString externalAddressUnprotected;
    int externalPortUnprotected = 0;
    int externalPortProtected = 0;
    QString location;
    int previewState = 0;
    bool antiCheatProtection = false;
    int pvpType = -1;
    /// The product's WebSocket game port (browser builds connect there).
    int webSocketPort = 0;
    /// The product's WebSocket port of the secondary connection (0: none).
    int webSocketPortUnprotected = 0;

    /// The address the game connects to.
    const QString& host() const { return externalAddressProtected; }
    /// The game port: TCP on desktop, the WebSocket port in the browser.
    int port() const;
    /// The unprotected game port (the secondary connection's): TCP on
    /// desktop, the unprotected WebSocket port in the browser.
    int portUnprotected() const;
};

/// One "playdata.characters" entry.
struct TCharacter
{
    QString name;
    int worldId = 0;
    int level = 0;
    QString vocation;
    bool isMale = false;
    bool isHidden = false;
    bool isMainCharacter = false;
    bool tutorial = false;
    /// 0 collected, 1 available, 2 not available.
    int dailyRewardState = 2;
    uint16_t outfitId = 0;
    /// Outfit colour indices (0..133): head, torso, legs, detail.
    uint8_t headColor = 0;
    uint8_t torsoColor = 0;
    uint8_t legsColor = 0;
    uint8_t detailColor = 0;
    bool firstAddOn = false;
    bool secondAddOn = false;
    /// The product's online flag (character switch); not in the CipSoft format.
    bool isOnline = false;
};

using TCharacterList = std::vector<TCharacter>;
/// Worlds by id.
using TWorldList = std::map<int, TWorld>;

enum class EAccountStatus : int
{
    Active = 0,
    Frozen = 1,
    Suspended = 2,
};

/// The login service's "session".
struct TPlaySessionData
{
    QString sessionKey;
    bool isPremium = false;
    bool isReturner = false;
    bool showRewardNews = false;
    bool fpsTracking = false;
    bool optionTracking = false;
    int premiumUntil = 0;
    EAccountStatus status = EAccountStatus::Active;
    bool recoverySetupComplete = false;
};

/// The survey invitation of a login answer.
struct TSurveyData
{
    uint32_t id = 0;
    QString invitationText;
    QString invitationToken;
    uint32_t endTimestamp = 0;
};

/// An open payment of a character trade.
struct TCharacterTradePayment
{
    QString name;
    int dueAmount = 0;
    uint32_t dueTimestamp = 0;
};

/// The addresses a two-factor email code can go to.
struct TTwoFactorEmailAddresses
{
    QString primaryEmail;
    QString secondaryEmail;
    QString sms;
};

/// Builds the login service's request and reads its answers.
class TAuthenticationJsonDocumentHelper
{
public:
    virtual ~TAuthenticationJsonDocumentHelper() = default;

    virtual QJsonDocument createLoginRequest(const TLoginRequestData& data) const;
    /// False (and `world` partly untouched) unless every world key is present.
    virtual bool parseWorld(const QJsonObject& object, TWorld& world) const;
    /// False unless the entry has a name and a world id.
    virtual bool parseCharacter(const QJsonObject& object, TCharacter& character) const;
    /// Appends the characters and adds the worlds of "playdata" (both arrays).
    virtual void parsePlayData(const QJsonObject& reply, TCharacterList& characters, TWorldList& worlds) const;
    virtual void parsePlaySessionData(const QJsonObject& reply, TPlaySessionData& session) const;
    virtual bool parseSurveyData(const QJsonObject& reply, TSurveyData& survey) const;
    virtual bool parseReturnerNotification(const QJsonObject& reply) const;
    virtual bool parseShowResendButton(const QJsonObject& reply, bool& showResendButton) const;
    /// True when a primary email is given; all three are cleared first.
    virtual bool parseTwoFactorEmailAddresses(const QJsonObject& reply, TTwoFactorEmailAddresses& addresses) const;
    virtual bool parseLoginConfirmation(const QJsonObject& reply, QString& confirmationToken, bool& hasPendingCode) const;
    /// True when at least one payment was read.
    virtual bool parseCharacterTradePayments(const QJsonObject& reply, std::vector<TCharacterTradePayment>& payments) const;
    virtual bool parseDeviceCookie(const QJsonObject& reply, QString& deviceCookie) const;
    virtual bool parseLoginEmail(const QJsonObject& reply, QString& loginEmail) const;
};

}
}
