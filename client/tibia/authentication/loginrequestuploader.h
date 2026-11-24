#pragma once

#include "authentication/authenticationjsondocumenthelper.h"
#include "authentication/cloudflarechallengetypes.h"

#include "jsondocumentuploaderbase.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class QNetworkReply;

namespace tibia {
namespace game {
class TGameSessionBase;
}
namespace shared {
class TJsonDocumentHttpPostSender;
}
namespace authentication {

/// A login answer: the account's characters, the worlds and the play session.
struct LoginResult
{
    TPlaySessionData session;
    TWorldList worlds;
    TCharacterList characters;

    const TWorld* world(int id) const;
};

/// The client options a login answer is written to (device cookies, the last
/// important balancing message seen). Optional: without them the answer is
/// read but nothing is remembered.
class ILoginRequestOptions
{
public:
    virtual ~ILoginRequestOptions() = default;
    virtual void setDeviceCookie(const std::string& loginEmail, const QString& deviceCookie) = 0;
    virtual uint32_t intOption(int option) const = 0;
};

/// The cf_clearance cookie of a solved Cloudflare challenge's clearance.
QNetworkCookie cloudflareClearanceCookie(const TCloudflareChallengeClearanceInfo& Clearance);

/// CIP: posts the login request to the login service through a JSON
/// document sender and reads its answer. The authentication process
/// controller reaches the character list updates through virtual calls
/// (CipSoft's slots).
class TLoginRequestUploader : public shared::TJsonDocumentUploaderBase
{
    Q_OBJECT
public:
    /// Login service error codes.
    enum : int
    {
        ErrorMissingTwoFactorToken = 6,
        ErrorClientOutOfDate = 7,
        ErrorMissingEMailLoginCode = 8,
        ErrorInvalidDeviceCookie = 11,
        ErrorMissingLoginConfirmation = 12,
    };

    /// CIP: the uploader of the login service at `Url`; `Sender` posts the
    /// requests and reports a Cloudflare mitigation.
    TLoginRequestUploader(const QUrl& Url, std::shared_ptr<shared::TJsonDocumentHttpPostSender> Sender);
    ~TLoginRequestUploader() override;

    /// CIP: the login request for `LoginData` goes to the login service, with
    /// the login data's Cloudflare clearance or without one.
    virtual void uploadLoginRequest(const TLoginRequestData& LoginData);
    /// qtclient: the client revision the requests carry (the portal's client
    /// version check reads it from the body).
    void setClientRevision(const QString& ClientRevision) { m_clientRevision = ClientRevision; }

    void setOptions(ILoginRequestOptions* options) { m_options = options; }

    /// The characters, worlds and session of a login answer (also the account
    /// service's answers after a character was created).
    static LoginResult parse(const QJsonObject& reply);

    /// The URL the login request goes to.
    QUrl url() const { return m_loginUrl; }
    /// The characters and worlds of the last answer.
    TCharacterList characters() const { return m_characters; }
    TWorldList worlds() const { return m_worlds; }
    /// Replaces the characters and worlds (an account change sends new ones).
    void setPlayData(const TCharacterList& Characters, const TWorldList& Worlds);
    TPlaySessionData playSessionData() const { return m_session; }
    /// The cf-mitigated header of the last Cloudflare-mitigated answer.
    std::string cloudflareMitigatedHeaderValue() const { return m_cloudflareMitigatedHeaderValue; }
    /// The named character has finished the tutorial.
    virtual void setTutorialFinished(const QString& CharacterName);
    /// The game session whose player updates the character list. Clearing it
    /// takes the player's last state into the list first.
    virtual void setGameSession(const std::shared_ptr<game::TGameSessionBase>& GameSession);
    /// Copies the logged-in player's outfit, level, vocation and daily reward
    /// state into its entry of the character list.
    virtual void updateCharactersFromGameSession();

    /// Seconds since the epoch when the last answer arrived.
    int loginTimestamp() const { return m_loginTimestamp; }
    const std::optional<TSurveyData>& survey() const { return m_survey; }
    const std::vector<TCharacterTradePayment>& characterTradePayments() const { return m_characterTradePayments; }
    bool showReturnerNotification() const { return m_showReturnerNotification; }
    bool showImportantBalancingMessage() const { return m_showImportantBalancingMessage; }
    bool showResendButton() const { return m_showResendButton; }
    const TTwoFactorEmailAddresses& twoFactorEmailAddresses() const { return m_twoFactorEmailAddresses; }

Q_SIGNALS:
    void loginSuccessful(const tibia::authentication::LoginResult& result);
    void loginFailed(const QString& ErrorMessage);
    void loginFailedDueToMissingTwoFactorToken();
    void loginFailedDueToMissingEMailLoginCode();
    void loginFailedDueToMissingLoginConfirmation(const QString& ConfirmationToken, bool hasPendingCode);
    void loginFailedDueToInvalidServerResponse();
    void loginFailedDueToInvalidDeviceCookie();
    void loginFailedDueToCloudflareMitigation();
    void loginFailedClientOutOfDate();

public Q_SLOTS:
    void onFinished(bool UploadSuccessful, QJsonDocument ReplyFromServerJD);
    void onCloudflareMitigated(QNetworkReply* Reply, std::string MitigatedHeaderValue);

private:
    /// The sender the base posts through (its Cloudflare clearance is set per request).
    std::shared_ptr<shared::TJsonDocumentHttpPostSender> m_pSender;
    /// The login service (CIP: the constructor's URL).
    QUrl m_loginUrl;
    QString m_clientRevision;

    std::unique_ptr<TAuthenticationJsonDocumentHelper> m_helper;
    ILoginRequestOptions* m_options = nullptr;
    std::shared_ptr<game::TGameSessionBase> m_gameSession;
    TCharacterList m_characters;
    TWorldList m_worlds;
    TPlaySessionData m_session;
    int m_loginTimestamp = 0;
    std::optional<TSurveyData> m_survey;
    std::vector<TCharacterTradePayment> m_characterTradePayments;
    bool m_showReturnerNotification = false;
    bool m_showImportantBalancingMessage = false;
    bool m_showResendButton = false;
    TTwoFactorEmailAddresses m_twoFactorEmailAddresses;
    std::string m_cloudflareMitigatedHeaderValue;
};

}
}
