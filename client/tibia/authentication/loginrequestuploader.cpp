#include "loginrequestuploader.h"

#include "appearances/appearanceinstance.h"
#include "appearances/appearancetype.h"
#include "creatures/creaturestorage.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "teamfinder/teamfinderciptypes.h"

#include "jsondocumenthttppostsender.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace tibia::authentication {

namespace {

/// The client option holding the id of the last important balancing message seen.
constexpr int LastImportantBalancingMessageOption = 126;
/// An important balancing message is shown for 28 days after its changes.
constexpr int ImportantBalancingMessageSeconds = 28 * 24 * 60 * 60;

/// The sender's form of a solved challenge's clearance.
shared::TJsonDocumentHttpPostSender::TCloudflareClearance senderClearance(const TCloudflareChallengeClearanceInfo& Clearance)
{
    return { cloudflareClearanceCookie(Clearance), Clearance.userAgent };
}

}

QNetworkCookie cloudflareClearanceCookie(const TCloudflareChallengeClearanceInfo& Clearance)
{
    for (const QNetworkCookie& cookie : Clearance.cookies) {
        if (cookie.name() == QByteArrayLiteral("cf_clearance"))
            return cookie;
    }
    return QNetworkCookie();
}

const TWorld* LoginResult::world(const int id) const
{
    const auto it = worlds.find(id);
    return it == worlds.end() ? nullptr : &it->second;
}

TLoginRequestUploader::TLoginRequestUploader(const QUrl& Url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender)
    : shared::TJsonDocumentUploaderBase(Sender)
    , m_pSender(std::move(Sender))
    , m_loginUrl(Url)
    , m_helper(std::make_unique<TAuthenticationJsonDocumentHelper>())
{
    connect(this, &TLoginRequestUploader::finished, this, &TLoginRequestUploader::onFinished);
    connect(m_pSender.get(), &shared::TJsonDocumentHttpPostSender::cloudflareMitigated, this,
            &TLoginRequestUploader::onCloudflareMitigated);
}

TLoginRequestUploader::~TLoginRequestUploader() = default;

void TLoginRequestUploader::uploadLoginRequest(const TLoginRequestData& LoginData)
{
    QJsonDocument request = m_helper->createLoginRequest(LoginData);
    // The sender sends the solved challenge's clearance, or none.
    if (!LoginData.cloudflareClearance)
        m_pSender->resetCloudflareClearance();
    else
        m_pSender->setCloudflareClearance(senderClearance(*LoginData.cloudflareClearance));
    if (!m_clientRevision.isEmpty()) {
        // qtclient host: the portal checks the client revision.
        QJsonObject body = request.object();
        body.insert(QStringLiteral("clientRevision"), m_clientRevision);
        request.setObject(body);
    }
    upload(m_loginUrl, request);
}

void TLoginRequestUploader::onCloudflareMitigated(QNetworkReply* Reply, const std::string MitigatedHeaderValue)
{
    Q_UNUSED(Reply);
    m_cloudflareMitigatedHeaderValue = MitigatedHeaderValue;
    Q_EMIT loginFailedDueToCloudflareMitigation();
}

void TLoginRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument ReplyFromServerJD)
{
    m_characters.clear();
    m_worlds.clear();
    m_session = TPlaySessionData();
    m_loginTimestamp = static_cast<int>(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000);

    if (!UploadSuccessful) {
        switch (errorCode()) {
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
            Q_EMIT loginFailed(errorMessage());
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
