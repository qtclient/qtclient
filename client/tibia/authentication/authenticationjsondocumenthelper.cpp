#include "authentication/authenticationjsondocumenthelper.h"

#include "platform/capabilities.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSysInfo>
#include <QVariant>

#include <algorithm>

namespace tibia::authentication {

namespace {

/// The login service's JSON keys, built once.
struct TJsonKeys
{
    QString type = QStringLiteral("type");
    QString login = QStringLiteral("login");
    QString email = QStringLiteral("email");
    QString password = QStringLiteral("password");
    QString token = QStringLiteral("token");
    QString deviceVerificationCode = QStringLiteral("deviceverificationcode");
    QString emailCode = QStringLiteral("emailcode");
    QString stayLoggedIn = QStringLiteral("stayloggedin");
    QString deviceCookie = QStringLiteral("devicecookie");
    QString trustedDeviceToken = QStringLiteral("trusteddevicetoken");
    QString loginConfirmationMethod = QStringLiteral("loginconfirmationmethod");
    QString loginConfirmationToken = QStringLiteral("loginconfirmationtoken");
    QString hasPendingCode = QStringLiteral("haspendingcode");
    QString loginConfirmationCode = QStringLiteral("loginconfirmationcode");
    QString primaryEmail = QStringLiteral("primaryemail");
    QString secondaryEmail = QStringLiteral("secondaryemail");
    QString sms = QStringLiteral("sms");
    QString clientType = QStringLiteral("clienttype");
    QString operatingSystem = QStringLiteral("operatingsystem");
    QString clientVersion = QStringLiteral("clientversion");
    QString assetVersion = QStringLiteral("assetversion");
    QString playData = QStringLiteral("playdata");
    QString worlds = QStringLiteral("worlds");
    QString id = QStringLiteral("id");
    QString name = QStringLiteral("name");
    QString externalPortProtected = QStringLiteral("externalportprotected");
    QString externalPortUnprotected = QStringLiteral("externalportunprotected");
    QString previewState = QStringLiteral("previewstate");
    QString externalAddressProtected = QStringLiteral("externaladdressprotected");
    QString externalAddressUnprotected = QStringLiteral("externaladdressunprotected");
    QString location = QStringLiteral("location");
    QString antiCheatProtection = QStringLiteral("anticheatprotection");
    QString pvpType = QStringLiteral("pvptype");
    QString restrictedStore = QStringLiteral("restrictedstore");
    QString characters = QStringLiteral("characters");
    QString vocation = QStringLiteral("vocation");
    QString level = QStringLiteral("level");
    QString worldId = QStringLiteral("worldid");
    QString tutorial = QStringLiteral("tutorial");
    QString isMale = QStringLiteral("ismale");
    QString isHidden = QStringLiteral("ishidden");
    QString isMainCharacter = QStringLiteral("ismaincharacter");
    QString dailyRewardState = QStringLiteral("dailyrewardstate");
    QString outfitId = QStringLiteral("outfitid");
    QString headColor = QStringLiteral("headcolor");
    QString torsoColor = QStringLiteral("torsocolor");
    QString legsColor = QStringLiteral("legscolor");
    QString detailColor = QStringLiteral("detailcolor");
    QString addonsFlags = QStringLiteral("addonsflags");
    QString session = QStringLiteral("session");
    QString sessionKey = QStringLiteral("sessionkey");
    QString premiumUntil = QStringLiteral("premiumuntil");
    QString isPremium = QStringLiteral("ispremium");
    QString status = QStringLiteral("status");
    QString isReturner = QStringLiteral("isreturner");
    QString showRewardNews = QStringLiteral("showrewardnews");
    QString fpsTracking = QStringLiteral("fpstracking");
    QString optionTracking = QStringLiteral("optiontracking");
    QString loginEmail = QStringLiteral("loginemail");
    QString survey = QStringLiteral("survey");
    QString invitationText = QStringLiteral("invitationtext");
    QString invitationToken = QStringLiteral("invitationtoken");
    QString endTimestamp = QStringLiteral("endtimestamp");
    QString returnerNotification = QStringLiteral("returnernotification");
    QString showResendButton = QStringLiteral("showresendbutton");
    QString characterTradePayments = QStringLiteral("charactertradepayments");
    QString dueAmount = QStringLiteral("dueamount");
    QString dueTimestamp = QStringLiteral("duetimestamp");
    QString recoverySetupComplete = QStringLiteral("recoverysetupcomplete");
    // Product keys, not in the CipSoft format.
    QString isOnline = QStringLiteral("isonline");
    QString webSocketPort = QStringLiteral("websocketport");
};

const TJsonKeys& jsonKeys()
{
    static const TJsonKeys keys;
    return keys;
}

/// `key` as an int clamped to [min, max]; `fallback` (then clamped) when it is not a number.
int readInt(const QJsonObject& object, const QString& key, const int min, const int max, const int fallback)
{
    const QJsonValue value = object.value(key);
    const int read = value.isDouble() ? value.toInt() : fallback;
    return std::min(std::max(read, min), max);
}

bool readBool(const QJsonObject& object, const QString& key, const bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

QString readString(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

/// An outfit colour index from the service: the low byte, 0 when beyond the 134 colours.
uint8_t outfitColor(const int value)
{
    const auto index = static_cast<uint8_t>(value);
    return index < 134 ? index : 0;
}

uint32_t readUnsigned(const QJsonObject& object, const QString& key)
{
    return static_cast<uint32_t>(object.value(key).toVariant().toULongLong());
}

}

int TWorld::port() const
{
    // Without TCP (the browser) the product's WebSocket game port is the game port.
    if (!tibia::platform::kHasTcpSockets && webSocketPort > 0)
        return webSocketPort;
    return externalPortProtected;
}

QJsonDocument TAuthenticationJsonDocumentHelper::createLoginRequest(const TLoginRequestData& data) const
{
    const TJsonKeys& keys = jsonKeys();
    QJsonObject request;
    request.insert(keys.type, keys.login);
    request.insert(keys.email, data.email);
    request.insert(keys.password, data.password);
    if (!data.token.isEmpty())
        request.insert(keys.token, data.token);
    if (!data.deviceVerificationCode.isEmpty())
        request.insert(keys.deviceVerificationCode, data.deviceVerificationCode);
    if (!data.loginConfirmationCode.isEmpty() && !data.loginConfirmationToken.isEmpty()) {
        request.insert(keys.loginConfirmationCode, data.loginConfirmationCode);
        request.insert(keys.loginConfirmationToken, data.loginConfirmationToken);
    }
    if (!data.trustedDeviceToken.isEmpty()) {
        request.insert(keys.trustedDeviceToken, data.trustedDeviceToken);
        request.insert(keys.operatingSystem, QSysInfo::prettyProductName());
    }
    if (!data.emailCode.isEmpty())
        request.insert(keys.emailCode, data.emailCode);
    request.insert(keys.stayLoggedIn, data.stayLoggedIn);
    request.insert(keys.deviceCookie, data.deviceCookie);
    request.insert(keys.clientType, data.clientType);
    request.insert(keys.clientVersion, data.clientVersion);
    request.insert(keys.assetVersion, data.assetVersion);
    return QJsonDocument(request);
}

bool TAuthenticationJsonDocumentHelper::parseWorld(const QJsonObject& object, TWorld& world) const
{
    const TJsonKeys& keys = jsonKeys();
    for (const QString* key : { &keys.id, &keys.name, &keys.externalAddressProtected, &keys.externalAddressUnprotected,
                                &keys.externalPortProtected, &keys.externalPortUnprotected, &keys.previewState,
                                &keys.location, &keys.antiCheatProtection, &keys.pvpType }) {
        if (!object.contains(*key))
            return false;
    }
    world.id = object.value(keys.id).toInt();
    world.name = object.value(keys.name).toString();
    world.externalAddressProtected = object.value(keys.externalAddressProtected).toString();
    world.externalAddressUnprotected = object.value(keys.externalAddressUnprotected).toString();
    world.externalPortProtected = object.value(keys.externalPortProtected).toInt();
    world.externalPortUnprotected = object.value(keys.externalPortUnprotected).toInt();
    world.previewState = object.value(keys.previewState).toInt();
    world.location = object.value(keys.location).toString();
    world.antiCheatProtection = object.value(keys.antiCheatProtection).toBool();
    world.pvpType = object.value(keys.pvpType).toInt();
    world.webSocketPort = object.value(keys.webSocketPort).toInt();
    return true;
}

bool TAuthenticationJsonDocumentHelper::parseCharacter(const QJsonObject& object, TCharacter& character) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!object.contains(keys.name) || !object.contains(keys.worldId))
        return false;
    constexpr int Max = 0x7fffffff;
    character.name = readString(object, keys.name);
    character.worldId = readInt(object, keys.worldId, 0, Max, 0);
    character.level = readInt(object, keys.level, 0, Max, 0);
    character.vocation = readString(object, keys.vocation);
    character.isMale = readBool(object, keys.isMale, false);
    character.isHidden = readBool(object, keys.isHidden, false);
    character.isMainCharacter = readBool(object, keys.isMainCharacter, false);
    character.tutorial = readBool(object, keys.tutorial, true);
    character.outfitId = static_cast<uint16_t>(readInt(object, keys.outfitId, 0, Max, 0));
    character.headColor = outfitColor(readInt(object, keys.headColor, 0, Max, 0));
    character.torsoColor = outfitColor(readInt(object, keys.torsoColor, 0, Max, 0));
    character.legsColor = outfitColor(readInt(object, keys.legsColor, 0, Max, 0));
    character.detailColor = outfitColor(readInt(object, keys.detailColor, 0, Max, 0));
    const int addOns = readInt(object, keys.addonsFlags, 0, Max, 0);
    character.firstAddOn = addOns & 1;
    character.secondAddOn = (addOns >> 1) & 1;
    character.dailyRewardState = readInt(object, keys.dailyRewardState, 0, 2, 2);
    character.isOnline = readBool(object, keys.isOnline, false);
    return true;
}

void TAuthenticationJsonDocumentHelper::parsePlayData(const QJsonObject& reply, TCharacterList& characters,
                                                      TWorldList& worlds) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.playData) || !reply.value(keys.playData).isObject())
        return;
    const QJsonObject playData = reply.value(keys.playData).toObject();
    if (!playData.contains(keys.characters) || !playData.contains(keys.worlds))
        return;
    if (!playData.value(keys.characters).isArray() || !playData.value(keys.worlds).isArray())
        return;

    for (const QJsonValue& value : playData.value(keys.characters).toArray()) {
        TCharacter character;
        if (parseCharacter(value.toObject(), character))
            characters.push_back(character);
    }
    for (const QJsonValue& value : playData.value(keys.worlds).toArray()) {
        TWorld world;
        if (parseWorld(value.toObject(), world))
            worlds[world.id] = world;
    }
}

void TAuthenticationJsonDocumentHelper::parsePlaySessionData(const QJsonObject& reply, TPlaySessionData& session) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.session) || !reply.value(keys.session).isObject())
        return;
    const QJsonObject object = reply.value(keys.session).toObject();
    session.sessionKey = object.value(keys.sessionKey).toString();
    session.isPremium = object.value(keys.isPremium).toBool();
    session.isReturner = object.value(keys.isReturner).toBool();
    session.showRewardNews = object.value(keys.showRewardNews).toBool();
    session.premiumUntil = object.value(keys.premiumUntil).toInt();
    session.fpsTracking = object.value(keys.fpsTracking).toBool();
    session.optionTracking = object.value(keys.optionTracking).toBool();
    session.recoverySetupComplete = object.value(keys.recoverySetupComplete).toBool();
    const QString status = object.value(keys.status).toString();
    if (status == QLatin1String("frozen"))
        session.status = EAccountStatus::Frozen;
    else if (status == QLatin1String("suspended"))
        session.status = EAccountStatus::Suspended;
    else
        session.status = EAccountStatus::Active;
}

bool TAuthenticationJsonDocumentHelper::parseSurveyData(const QJsonObject& reply, TSurveyData& survey) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.survey) || !reply.value(keys.survey).isObject())
        return false;
    const QJsonObject object = reply.value(keys.survey).toObject();
    survey.id = readUnsigned(object, keys.id);
    survey.invitationText = object.value(keys.invitationText).toString();
    survey.invitationToken = object.value(keys.invitationToken).toString();
    survey.endTimestamp = readUnsigned(object, keys.endTimestamp);
    return true;
}

bool TAuthenticationJsonDocumentHelper::parseReturnerNotification(const QJsonObject& reply) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.session) || !reply.value(keys.session).isObject())
        return false;
    const QJsonObject session = reply.value(keys.session).toObject();
    return session.contains(keys.returnerNotification) && session.value(keys.returnerNotification).toBool();
}

bool TAuthenticationJsonDocumentHelper::parseShowResendButton(const QJsonObject& reply, bool& showResendButton) const
{
    const TJsonKeys& keys = jsonKeys();
    showResendButton = false;
    if (reply.contains(keys.showResendButton))
        showResendButton = reply.value(keys.showResendButton).toBool();
    return showResendButton;
}

bool TAuthenticationJsonDocumentHelper::parseTwoFactorEmailAddresses(const QJsonObject& reply,
                                                                     TTwoFactorEmailAddresses& addresses) const
{
    const TJsonKeys& keys = jsonKeys();
    addresses.sms = QString();
    addresses.secondaryEmail = addresses.sms;
    addresses.primaryEmail = addresses.secondaryEmail;
    if (!reply.contains(keys.primaryEmail))
        return false;
    addresses.primaryEmail = reply.value(keys.primaryEmail).toString();
    if (reply.contains(keys.secondaryEmail))
        addresses.secondaryEmail = reply.value(keys.secondaryEmail).toString();
    if (reply.contains(keys.sms))
        addresses.sms = reply.value(keys.sms).toString();
    return true;
}

bool TAuthenticationJsonDocumentHelper::parseLoginConfirmation(const QJsonObject& reply, QString& confirmationToken,
                                                               bool& hasPendingCode) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.loginConfirmationToken))
        return false;
    confirmationToken = reply.value(keys.loginConfirmationToken).toString();
    if (!reply.contains(keys.hasPendingCode))
        return false;
    hasPendingCode = reply.value(keys.hasPendingCode).toBool();
    return true;
}

bool TAuthenticationJsonDocumentHelper::parseCharacterTradePayments(const QJsonObject& reply,
                                                                    std::vector<TCharacterTradePayment>& payments) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.characterTradePayments) || !reply.value(keys.characterTradePayments).isArray())
        return false;
    for (const QJsonValue& value : reply.value(keys.characterTradePayments).toArray()) {
        const QJsonObject object = value.toObject();
        TCharacterTradePayment payment;
        payment.name = object.value(keys.name).toString();
        payment.dueAmount = object.value(keys.dueAmount).toInt();
        payment.dueTimestamp = readUnsigned(object, keys.dueTimestamp);
        payments.push_back(payment);
    }
    return !payments.empty();
}

bool TAuthenticationJsonDocumentHelper::parseDeviceCookie(const QJsonObject& reply, QString& deviceCookie) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.deviceCookie) || !reply.value(keys.deviceCookie).isString())
        return false;
    deviceCookie = reply.value(keys.deviceCookie).toString();
    return true;
}

bool TAuthenticationJsonDocumentHelper::parseLoginEmail(const QJsonObject& reply, QString& loginEmail) const
{
    const TJsonKeys& keys = jsonKeys();
    if (!reply.contains(keys.loginEmail) || !reply.value(keys.loginEmail).isString())
        return false;
    loginEmail = reply.value(keys.loginEmail).toString();
    return true;
}

}
