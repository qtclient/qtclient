// CIP authentication::TLoginProtocolMessageHandler: the answer to the login
// challenge, the secondary login and the login outcome messages.

#include "authentication/authenticationandencryptioninfo.h"
#include "authentication/loginprotocolmessagehandler.h"

#include <protocol.pb.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>

using tibia::authentication::TAuthenticationAndEncryptionInfo;
using tibia::authentication::TLoginProtocolMessageHandler;
using namespace tibia::protobuf::protocol;

namespace {

std::shared_ptr<GameserverMessage> challengeMessage(uint32_t timestamp, uint32_t random, int connectionType)
{
    auto message = std::make_shared<GameserverMessage>();
    auto* challenge = message->MutableExtension(GameserverMessageLoginChallenge::gameserver_message_login_challenge);
    challenge->set_timestamp(timestamp);
    challenge->set_random(random);
    message->mutable_meta_data()->set_value3(connectionType);
    return message;
}

}

TEST(LoginProtocolMessageHandler, AnswersTheChallengeWithTheLogin)
{
    auto info = std::make_shared<TAuthenticationAndEncryptionInfo>();
    info->setCharacterName(QStringLiteral("Knight"));
    info->setSessionKey(QStringLiteral("session"));
    info->setAssetsIdentifier(QStringLiteral("assets"));
    TLoginProtocolMessageHandler handler(info, false);
    std::optional<GameclientMessageLogin> login;
    int challenges = 0;
    QObject::connect(&handler, &TLoginProtocolMessageHandler::sendLoginMessage,
                     [&login](const GameclientMessageLogin& message) { login = message; });
    QObject::connect(&handler, &TLoginProtocolMessageHandler::gameLoginChallengeMessage, [&challenges] { ++challenges; });

    handler.handleAnyMessage(std::make_shared<GameserverMessage>()); // no challenge: nothing
    EXPECT_FALSE(login.has_value());

    handler.handleLoginSuccessMessage(GameserverMessageLoginSuccess());
    EXPECT_TRUE(handler.isLoggedIn());
    handler.handleAnyMessage(challengeMessage(1234, 77, 0));
    ASSERT_TRUE(login.has_value());
    EXPECT_EQ(challenges, 1);
    EXPECT_FALSE(handler.isLoggedIn());
    EXPECT_EQ(login->client_os(), TLoginProtocolMessageHandler::clientOperatingSystem(false));
    EXPECT_EQ(login->protocol_version(), 0x606u);
    EXPECT_EQ(login->client_version(), 1533u);
    EXPECT_EQ(QString::fromStdString(login->client_version_string()).left(5), QStringLiteral("15.33"));
    EXPECT_EQ(login->asset_identifier(), "assets");
    EXPECT_EQ(login->preview_state(), 0u);
    EXPECT_EQ(login->character_name(), "Knight");
    EXPECT_EQ(login->session_key(), "session");
    EXPECT_EQ(login->challenge_timestamp(), 1234u);
    EXPECT_EQ(login->challenge_random(), 77u);
    EXPECT_FALSE(login->has_product_transport());
    // The key sent is the new session key, little-endian words.
    const QByteArray key = info->xteaKey();
    ASSERT_EQ(login->xtea_key_size(), 4);
    EXPECT_EQ(login->xtea_key(0), uint32_t(uint8_t(key[0])) | uint32_t(uint8_t(key[1])) << 8
                                      | uint32_t(uint8_t(key[2])) << 16 | uint32_t(uint8_t(key[3])) << 24);
}

TEST(LoginProtocolMessageHandler, AnswersWithTheSecondaryLoginOnceTheServerAllowedIt)
{
    auto info = std::make_shared<TAuthenticationAndEncryptionInfo>();
    TLoginProtocolMessageHandler handler(info, false);
    int ready = 0;
    std::optional<GameclientMessageSecondaryLogin> secondaryLogin;
    bool loginSent = false;
    QObject::connect(&handler, &TLoginProtocolMessageHandler::gameReadyForSecondaryConnection, [&ready] { ++ready; });
    QObject::connect(&handler, &TLoginProtocolMessageHandler::sendSecondaryLoginMessage,
                     [&secondaryLogin](const GameclientMessageSecondaryLogin& message) { secondaryLogin = message; });
    QObject::connect(&handler, &TLoginProtocolMessageHandler::sendLoginMessage,
                     [&loginSent](const GameclientMessageLogin&) { loginSent = true; });

    GameserverMessageReadyForSecondaryConnection readyMessage;
    readyMessage.set_token("token");
    handler.handleReadyForSecondaryConnectionMessage(readyMessage);
    EXPECT_EQ(ready, 1);
    EXPECT_TRUE(info->isSecondaryLogin());
    EXPECT_EQ(info->secondaryLoginToken(), QStringLiteral("token"));

    handler.handleAnyMessage(challengeMessage(1, 2, 1));
    EXPECT_FALSE(loginSent);
    ASSERT_TRUE(secondaryLogin.has_value());
    EXPECT_EQ(secondaryLogin->challenge_connection(), 1u);
    EXPECT_EQ(secondaryLogin->first_string(), "");
    EXPECT_EQ(secondaryLogin->token(), "token");
}

TEST(LoginProtocolMessageHandler, PassesTheLoginOutcomeOn)
{
    TLoginProtocolMessageHandler handler(std::make_shared<TAuthenticationAndEncryptionInfo>(), false);
    QString text;
    int number = -1;
    QObject::connect(&handler, &TLoginProtocolMessageHandler::gameLoginErrorMessage,
                     [&](const QString& message, int action) { text = message; number = action; });
    GameserverMessageLoginError error;
    error.set_error("outdated");
    error.set_reason(2);
    handler.handleLoginErrorMessage(error);
    EXPECT_EQ(text, QStringLiteral("outdated"));
    EXPECT_EQ(number, 2);
    error.set_reason(3);
    EXPECT_THROW(handler.handleLoginErrorMessage(error), std::runtime_error);

    QObject::connect(&handler, &TLoginProtocolMessageHandler::gameLoginWaitMessage,
                     [&](const QString& message, int seconds) { text = message; number = seconds; });
    GameserverMessageLoginWait wait;
    wait.set_message("queue");
    wait.set_time(12);
    handler.handleLoginWaitMessage(wait);
    EXPECT_EQ(text, QStringLiteral("queue"));
    EXPECT_EQ(number, 12);

    QObject::connect(&handler, &TLoginProtocolMessageHandler::gameLoginAdviceMessage,
                     [&](const QString& message) { text = message; });
    GameserverMessageLoginAdvice advice;
    advice.set_message("advice");
    handler.handleLoginAdviceMessage(advice);
    EXPECT_EQ(text, QStringLiteral("advice"));
}

TEST(LoginProtocolMessageHandler, ClientVersionNumberKeepsAtMostFourDigits)
{
    EXPECT_EQ(TLoginProtocolMessageHandler::clientVersionNumber(QStringLiteral("15.33")), 1533u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientVersionNumber(QStringLiteral("15.33.123")), 1533u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientVersionNumber(QStringLiteral("9.1")), 91u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientVersionNumber(QString()), 0u);
}

TEST(LoginProtocolMessageHandler, OperatingSystemIdsFollowThePlatform)
{
    const uint32_t os = TLoginProtocolMessageHandler::clientOperatingSystem(false);
#if defined(Q_OS_WIN)
    EXPECT_EQ(os, 5u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientOperatingSystem(true), 8u);
#elif defined(Q_OS_MACOS)
    EXPECT_EQ(os, 6u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientOperatingSystem(true), 9u);
#elif defined(Q_OS_LINUX)
    EXPECT_EQ(os, 7u);
    EXPECT_EQ(TLoginProtocolMessageHandler::clientOperatingSystem(true), 7u);
#else
    EXPECT_EQ(os, 5u);
#endif
}
