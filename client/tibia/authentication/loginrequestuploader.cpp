#include "loginrequestuploader.h"

#include "appearances/appearanceinstance.h"
#include "appearances/appearancetype.h"
#include "creatures/creaturestorage.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "teamfinder/teamfinderciptypes.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tibia::authentication {

namespace {

/// The client option holding the id of the last important balancing message seen.
constexpr int LastImportantBalancingMessageOption = 126;
/// An important balancing message is shown for 28 days after its changes.
constexpr int ImportantBalancingMessageSeconds = 28 * 24 * 60 * 60;

bool looksLikeCloudflareChallenge(const QNetworkReply* reply, const QByteArray& mitigated)
{
    if (!mitigated.isEmpty())
        return true;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 403 && status != 503)
        return false;
    const QByteArray server = reply->rawHeader("server").toLower();
    return server.contains("cloudflare") || !reply->rawHeader("cf-ray").isEmpty();
}

}

const TWorld* LoginResult::world(const int id) const
{
    const auto it = worlds.find(id);
    return it == worlds.end() ? nullptr : &it->second;
}

TLoginRequestUploader::TLoginRequestUploader(QObject* parent)
    : cip::tibia::authentication::TLoginRequestUploader(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_helper(std::make_unique<TAuthenticationJsonDocumentHelper>())
{
    connect(this, &TLoginRequestUploader::finished, this, &TLoginRequestUploader::onFinished);
}

TLoginRequestUploader::TLoginRequestUploader(const QUrl& Url, QObject* parent)
    : TLoginRequestUploader(parent)
{
    m_loginUrl = Url;
}

TLoginRequestUploader::~TLoginRequestUploader() = default;

void TLoginRequestUploader::uploadLoginRequest(const TLoginRequestData& LoginData)
{
    post(m_loginUrl, m_helper->createLoginRequest(LoginData).object(), m_clientRevision);
}

void TLoginRequestUploader::login(const QUrl& url, const TLoginRequestData& data, const QString& clientRevision)
{
    m_loginUrl = url;
    m_clientRevision = clientRevision;
    uploadLoginRequest(data);
}

void TLoginRequestUploader::abort()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->abort();
}

void TLoginRequestUploader::applyClearance(const TCloudflareChallengeClearanceInfo& clearance)
{
    m_lastUserAgent = clearance.userAgent;
    if (!m_network->cookieJar())
        m_network->setCookieJar(new QNetworkCookieJar(m_network));
    QNetworkCookieJar* jar = m_network->cookieJar();
    for (const QNetworkCookie& cookie : clearance.cookies) {
        QNetworkCookie copy = cookie;
        if (copy.domain().isEmpty() && m_lastUrl.isValid())
            copy.setDomain(m_lastUrl.host());
        jar->insertCookie(copy);
    }
}

void TLoginRequestUploader::post(const QUrl& url, QJsonObject body, const QString& clientRevision)
{
    if (!clientRevision.isEmpty())
        body.insert(QStringLiteral("clientRevision"), clientRevision);

    m_lastUrl = url;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!clientRevision.isEmpty())
        request.setRawHeader("X-Tibia-Client", clientRevision.toUtf8());
    if (!m_lastUserAgent.isEmpty())
        request.setHeader(QNetworkRequest::UserAgentHeader, m_lastUserAgent);
    request.setTransferTimeout(30000);

    if (m_reply)
        m_reply->abort();
    QNetworkReply* reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_reply == reply)
            m_reply = nullptr;
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;

        const QByteArray mitigated = reply->rawHeader("cf-mitigated");
        if (looksLikeCloudflareChallenge(reply, mitigated)) {
            onCloudflareMitigated(reply, std::string(mitigated.constData(), mitigated.size()));
            return;
        }

        // Error answers (401 etc.) still carry the service's JSON.
        QJsonParseError parseError{};
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (reply->error() != QNetworkReply::NoError) {
                // TJsonDocumentUploaderBase: a network error keeps its text, code 0.
                m_errorCode = 0;
                m_errorMessage = tr("Could not reach the login service: %1").arg(reply->errorString());
                Q_EMIT loginFailed(m_errorMessage);
            } else
                Q_EMIT loginFailedDueToInvalidServerResponse();
            return;
        }
        // TJsonDocumentUploaderBase: an answer with an error code is a failed upload.
        const QJsonObject answer = doc.object();
        if (answer.contains(QStringLiteral("errorCode"))) {
            m_errorCode = answer.value(QStringLiteral("errorCode")).toInt();
            m_errorMessage = answer.value(QStringLiteral("errorMessage")).toString();
            Q_EMIT finished(false, doc);
            return;
        }
        m_errorCode = 0;
        m_errorMessage.clear();
        Q_EMIT finished(true, doc);
    });
}

void TLoginRequestUploader::onCloudflareMitigated(QNetworkReply* Reply, const std::string& MitigatedHeaderValue)
{
    m_cloudflareMitigatedHeaderValue = MitigatedHeaderValue;
    m_lastMitigatedUrl = Reply ? Reply->url() : m_lastUrl;
    if (!m_lastMitigatedUrl.isValid())
        m_lastMitigatedUrl = m_lastUrl;
    Q_EMIT loginFailedDueToCloudflareMitigation();
}

void TLoginRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    m_characters.clear();
    m_worlds.clear();
    m_session = TPlaySessionData();
    m_loginTimestamp = static_cast<int>(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);

    if (!UploadSuccessful) {
        switch (m_errorCode) {
        case ErrorMissingTwoFactorToken:
            Q_EMIT loginFailedDueToMissingTwoFactorToken();
            break;
        case ErrorClientOutOfDate:
            Q_EMIT loginFailedClientOutOfDate();
            break;
        case ErrorMissingEMailLoginCode:
            m_helper->parseShowResendButton(ReplyFromServerJD.object(), m_showResendButton);
            Q_EMIT loginFailedDueToMissingEMailLoginCode();
            break;
        case ErrorInvalidDeviceCookie:
            Q_EMIT loginFailedDueToInvalidDeviceCookie();
            break;
        case ErrorMissingLoginConfirmation: {
            m_helper->parseTwoFactorEmailAddresses(ReplyFromServerJD.object(), m_twoFactorEmailAddresses);
            QString confirmationToken;
            bool hasPendingCode = false;
            if (m_helper->parseLoginConfirmation(ReplyFromServerJD.object(), confirmationToken, hasPendingCode))
                Q_EMIT loginFailedDueToMissingLoginConfirmation(confirmationToken, hasPendingCode);
            break;
        }
        default:
            Q_EMIT loginFailed(m_errorMessage);
            break;
        }
        return;
    }

    const QJsonObject reply = ReplyFromServerJD.object();
    m_helper->parsePlayData(reply, m_characters, m_worlds);
    m_helper->parsePlaySessionData(reply, m_session);
    if (game::TPremiumTimeStorage* premium = game::TPremiumTimeStorage::liveInstance())
        premium->setPremium(m_session.isPremium, static_cast<uint32_t>(m_session.premiumUntil));

    QString loginEmail;
    QString deviceCookie;
    m_helper->parseLoginEmail(reply, loginEmail);
    if (m_helper->parseDeviceCookie(reply, deviceCookie) && !deviceCookie.isEmpty() && !loginEmail.isEmpty() && m_options)
        m_options->setDeviceCookie(loginEmail.toStdString(), deviceCookie);

    TSurveyData survey;
    if (m_helper->parseSurveyData(reply, survey))
        m_survey = survey;
    else
        m_survey.reset();

    m_characterTradePayments.clear();
    m_helper->parseCharacterTradePayments(reply, m_characterTradePayments);
    m_showReturnerNotification = m_helper->parseReturnerNotification(reply);
    m_showImportantBalancingMessage = false;
    if (m_options) {
        QString data;
        QFile file(QStringLiteral(":/data/ImportantBalancingMessageData.json"));
        if (file.open(QIODevice::ReadOnly))
            data = QString::fromUtf8(file.readAll());
        file.close();
        const QJsonObject message = QJsonDocument::fromJson(data.toUtf8()).object();
        const auto id = static_cast<uint32_t>(message.value(QStringLiteral("id")).toInt());
        const auto now = static_cast<int>(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);
        const int dateOfCurrentChanges = message.value(QStringLiteral("dateOfCurrentChanges")).toInt();
        if (static_cast<uint32_t>(now - dateOfCurrentChanges) <= ImportantBalancingMessageSeconds
            && m_options->intOption(LastImportantBalancingMessageOption) < id)
            m_showImportantBalancingMessage = true;
    }

    Q_EMIT loginSuccessful(LoginResult{ m_session, m_worlds, m_characters });
}

void TLoginRequestUploader::setPlayData(const TCharacterList& Characters, const TWorldList& Worlds)
{
    m_characters = Characters;
    m_worlds = Worlds;
}

void TLoginRequestUploader::setTutorialFinished(const QString& CharacterName)
{
    for (TCharacter& character : m_characters) {
        if (character.name == CharacterName)
            character.tutorial = false;
    }
}

void TLoginRequestUploader::setGameSession(const std::shared_ptr<game::TGameSessionBase>& GameSession)
{
    if (!GameSession)
        updateCharactersFromGameSession();
    m_gameSession = GameSession;
}

void TLoginRequestUploader::updateCharactersFromGameSession()
{
    if (!m_gameSession)
        return;
    const std::shared_ptr<creatures::TCreature> player = m_gameSession->playerCreature().lock();
    const std::shared_ptr<game::TPlayerData> playerData = m_gameSession->playerData();
    for (TCharacter& character : m_characters) {
        if (!player || !playerData || player->name() != character.name)
            continue;
        const auto outfit =
            std::dynamic_pointer_cast<appearances::TCustomizableOutfitAppearanceInstance>(player->outfitAppearance());
        if (outfit) {
            character.outfitId = outfit->thingType()->getId();
            const appearances::TOutfitCustomization& customization = outfit->customization();
            character.headColor = customization.head;
            character.torsoColor = customization.body;
            character.legsColor = customization.legs;
            character.detailColor = customization.feet;
            character.firstAddOn = customization.firstAddon;
            character.secondAddOn = customization.secondAddon;
        }
        character.level = playerData->level();
        character.vocation = teamfinder::vocationName(playerData->vocation(), playerData->hasReachedMain());
        const std::shared_ptr<dailyreward::TDailyRewardStorage>& dailyReward = m_gameSession->dailyRewardStorage();
        if (dailyReward && character.dailyRewardState != 2)
            character.dailyRewardState = dailyReward->dailyRewardReadyToPickUp() ? 1 : 0;
    }
}

LoginResult TLoginRequestUploader::parse(const QJsonObject& reply)
{
    const TAuthenticationJsonDocumentHelper helper;
    LoginResult result;
    helper.parsePlayData(reply, result.characters, result.worlds);
    helper.parsePlaySessionData(reply, result.session);
    return result;
}

}
