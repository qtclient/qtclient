#include <gtest/gtest.h>

#include "authentication/authenticationjsondocumenthelper.h"

#include <QJsonArray>

using namespace tibia::authentication;

namespace {

QJsonObject worldObject(const int id)
{
    return QJsonObject{
        { "id", id },
        { "name", "Antica" },
        { "externaladdressprotected", "p.example" },
        { "externaladdressunprotected", "u.example" },
        { "externalportprotected", 7172 },
        { "externalportunprotected", 7171 },
        { "previewstate", 1 },
        { "location", "EUR" },
        { "anticheatprotection", true },
        { "pvptype", 2 },
    };
}

}

TEST(AuthenticationJsonDocumentHelper, LoginRequestSendsOptionalPartsOnlyWhenSet)
{
    const TAuthenticationJsonDocumentHelper helper;
    TLoginRequestData data;
    data.email = "a@b";
    data.password = "pw";
    data.loginConfirmationCode = "123";
    data.clientType = 3;
    QJsonObject request = helper.createLoginRequest(data).object();
    EXPECT_EQ(request.value("type").toString(), "login");
    EXPECT_EQ(request.value("email").toString(), "a@b");
    EXPECT_FALSE(request.contains("token"));
    // The confirmation code goes out only with its token.
    EXPECT_FALSE(request.contains("loginconfirmationcode"));
    EXPECT_FALSE(request.contains("operatingsystem"));
    EXPECT_EQ(request.value("stayloggedin").toBool(true), false);
    EXPECT_EQ(request.value("clienttype").toInt(), 3);
    EXPECT_TRUE(request.contains("devicecookie"));

    data.loginConfirmationToken = "tok";
    data.trustedDeviceToken = "trusted";
    request = helper.createLoginRequest(data).object();
    EXPECT_EQ(request.value("loginconfirmationcode").toString(), "123");
    EXPECT_EQ(request.value("loginconfirmationtoken").toString(), "tok");
    EXPECT_TRUE(request.contains("operatingsystem"));
}

TEST(AuthenticationJsonDocumentHelper, WorldNeedsEveryKey)
{
    const TAuthenticationJsonDocumentHelper helper;
    TWorld world;
    ASSERT_TRUE(helper.parseWorld(worldObject(4), world));
    EXPECT_EQ(world.id, 4);
    EXPECT_EQ(world.externalPortProtected, 7172);
    EXPECT_EQ(world.externalPortUnprotected, 7171);
    // Desktop: the secondary connection's port is the unprotected TCP port.
    EXPECT_EQ(world.portUnprotected(), 7171);
    EXPECT_EQ(world.host(), "p.example");
    EXPECT_TRUE(world.antiCheatProtection);
    EXPECT_EQ(world.pvpType, 2);

    QJsonObject incomplete = worldObject(5);
    incomplete.remove("location");
    TWorld other;
    EXPECT_FALSE(helper.parseWorld(incomplete, other));
}

TEST(AuthenticationJsonDocumentHelper, CharacterDefaultsAndClamps)
{
    const TAuthenticationJsonDocumentHelper helper;
    TCharacter character;
    EXPECT_FALSE(helper.parseCharacter(QJsonObject{ { "name", "x" } }, character));

    ASSERT_TRUE(helper.parseCharacter(QJsonObject{ { "name", "Knight" },
                                                   { "worldid", 4 },
                                                   { "level", -5 },
                                                   { "headcolor", 140 },
                                                   { "torsocolor", 12 },
                                                   { "addonsflags", 2 },
                                                   { "dailyrewardstate", 7 },
                                                   { "ismale", "yes" } },
                                      character));
    EXPECT_EQ(character.level, 0);
    // Beyond the 134 outfit colours the colour is 0.
    EXPECT_EQ(character.headColor, 0);
    EXPECT_EQ(character.torsoColor, 12);
    EXPECT_FALSE(character.firstAddOn);
    EXPECT_TRUE(character.secondAddOn);
    EXPECT_EQ(character.dailyRewardState, 2);
    // A missing tutorial flag counts as set; a non-bool ismale as unset.
    EXPECT_TRUE(character.tutorial);
    EXPECT_FALSE(character.isMale);
}

TEST(AuthenticationJsonDocumentHelper, PlayDataNeedsBothArrays)
{
    const TAuthenticationJsonDocumentHelper helper;
    TCharacterList characters;
    TWorldList worlds;
    const QJsonArray characterArray{ QJsonObject{ { "name", "A" }, { "worldid", 4 } }, QJsonObject{ { "name", "B" } } };
    helper.parsePlayData(QJsonObject{ { "playdata", QJsonObject{ { "characters", characterArray } } } }, characters, worlds);
    EXPECT_TRUE(characters.empty());

    helper.parsePlayData(
        QJsonObject{ { "playdata", QJsonObject{ { "characters", characterArray },
                                                { "worlds", QJsonArray{ worldObject(4), worldObject(4), QJsonObject{} } } } } },
        characters, worlds);
    ASSERT_EQ(characters.size(), 1u);
    EXPECT_EQ(characters.front().name, "A");
    EXPECT_EQ(worlds.size(), 1u);
    EXPECT_EQ(worlds.count(4), 1u);
}

TEST(AuthenticationJsonDocumentHelper, SessionStatus)
{
    const TAuthenticationJsonDocumentHelper helper;
    TPlaySessionData session;
    helper.parsePlaySessionData(QJsonObject{ { "session", QJsonObject{ { "sessionkey", "k" },
                                                                       { "ispremium", true },
                                                                       { "premiumuntil", 99 },
                                                                       { "status", "frozen" } } } },
                                session);
    EXPECT_EQ(session.sessionKey, "k");
    EXPECT_TRUE(session.isPremium);
    EXPECT_EQ(session.premiumUntil, 99);
    EXPECT_EQ(session.status, EAccountStatus::Frozen);
    helper.parsePlaySessionData(QJsonObject{ { "session", QJsonObject{ { "status", "Suspended" } } } }, session);
    EXPECT_EQ(session.status, EAccountStatus::Active);
}

TEST(AuthenticationJsonDocumentHelper, ErrorAnswers)
{
    const TAuthenticationJsonDocumentHelper helper;
    TTwoFactorEmailAddresses addresses;
    addresses.sms = "old";
    EXPECT_FALSE(helper.parseTwoFactorEmailAddresses(QJsonObject{ { "sms", "s" } }, addresses));
    EXPECT_TRUE(addresses.sms.isEmpty());
    EXPECT_TRUE(helper.parseTwoFactorEmailAddresses(QJsonObject{ { "primaryemail", "p" }, { "sms", "s" } }, addresses));
    EXPECT_EQ(addresses.sms, "s");

    QString token;
    bool pending = false;
    EXPECT_FALSE(helper.parseLoginConfirmation(QJsonObject{ { "loginconfirmationtoken", "t" } }, token, pending));
    EXPECT_EQ(token, "t");
    EXPECT_TRUE(helper.parseLoginConfirmation(QJsonObject{ { "loginconfirmationtoken", "t" }, { "haspendingcode", true } },
                                              token, pending));
    EXPECT_TRUE(pending);

    bool resend = true;
    EXPECT_FALSE(helper.parseShowResendButton(QJsonObject{}, resend));
    EXPECT_FALSE(resend);

    std::vector<TCharacterTradePayment> payments;
    EXPECT_TRUE(helper.parseCharacterTradePayments(
        QJsonObject{ { "charactertradepayments",
                       QJsonArray{ QJsonObject{ { "name", "n" }, { "dueamount", 5 }, { "duetimestamp", 1700000000 } } } } },
        payments));
    ASSERT_EQ(payments.size(), 1u);
    EXPECT_EQ(payments.front().dueTimestamp, 1700000000u);

    QString cookie;
    EXPECT_FALSE(helper.parseDeviceCookie(QJsonObject{ { "devicecookie", 3 } }, cookie));
    EXPECT_TRUE(helper.parseDeviceCookie(QJsonObject{ { "devicecookie", "c" } }, cookie));
    EXPECT_FALSE(helper.parseReturnerNotification(QJsonObject{ { "session", QJsonObject{} } }));
    EXPECT_TRUE(helper.parseReturnerNotification(
        QJsonObject{ { "session", QJsonObject{ { "returnernotification", true } } } }));
}
