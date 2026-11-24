#pragma once

#include "authentication/authenticationjsondocumenthelper.h"
#include "authentication/cloudflarechallengetypes.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QNetworkCookie>
#include <QString>
#include <QUrl>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia {
namespace game {
class TGameSessionBase;
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

/// POSTs the login request to the login service and reads its answer.
/// The authentication process controller reaches the character list updates
/// through virtual calls (CipSoft's slots).
class TLoginRequestUploader : public cip::tibia::authentication::TLoginRequestUploader
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

    explicit TLoginRequestUploader(QObject* parent = nullptr);
    /// CIP: the uploader of the login service at `Url`.
    explicit TLoginRequestUploader(const QUrl& Url, QObject* parent = nullptr);
    ~TLoginRequestUploader() override;

    /// CIP: the login request for `LoginData` goes to the login service.
    virtual void uploadLoginRequest(const TLoginRequestData& LoginData);
    /// qtclient: the client revision the requests carry (X-Tibia-Client).
    void setClientRevision(const QString& ClientRevision) { m_clientRevision = ClientRevision; }
    void login(const QUrl& url, const TLoginRequestData& data, const QString& clientRevision);
    /// Drop an in-flight login. The Abort button on CIP's connecting dialog.
    void abort();

    /// Apply Cloudflare clearance cookies / UA for the next (and future) POSTs.
    void applyClearance(const TCloudflareChallengeClearanceInfo& clearance);
    /// Challenge page URL from the last cf-mitigated reply (empty if none).
    QUrl lastMitigatedUrl() const { return m_lastMitigatedUrl; }

    void setOptions(ILoginRequestOptions* options) { m_options = options; }

    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD) override;
    /// CIP slot (skipped in cipgen for QNetworkReply*/std::string); qtclient invokes it from post().
    void onCloudflareMitigated(QNetworkReply* Reply, const std::string& MitigatedHeaderValue);

    /// The characters, worlds and session of a login answer (also the account
    /// service's answers after a character was created).
    static LoginResult parse(const QJsonObject& reply);

    /// The URL the login request goes to.
    QUrl url() const { return m_lastUrl; }
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
    void setGameSession(const std::shared_ptr<game::TGameSessionBase>& GameSession);
    /// Copies the logged-in player's outfit, level, vocation and daily reward
    /// state into its entry of the character list.
    virtual void updateCharactersFromGameSession();

    /// The server's errorCode of the last answer (0 for a network error).
    int errorCode() const { return m_errorCode; }
    /// The network error, or the server's errorMessage, of the last answer.
    QString errorMessage() const { return m_errorMessage; }
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

private:
    void post(const QUrl& url, QJsonObject body, const QString& clientRevision);

    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
    /// The login service (CIP: the constructor's URL).
    QUrl m_loginUrl;
    QString m_clientRevision;
    QUrl m_lastUrl;
    QUrl m_lastMitigatedUrl;
    QString m_lastUserAgent;
    /// The login service's error of the last answer (TJsonDocumentUploaderBase).
    int m_errorCode = 0;
    QString m_errorMessage;

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
