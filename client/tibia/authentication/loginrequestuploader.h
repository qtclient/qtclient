#pragma once

#include "authentication/cloudflarechallengetypes.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QList>
#include <QNetworkCookie>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia {
namespace authentication {

/// The login service's "session" (CipSoft: TPlaySessionData — no email/ssoswitch).
struct PlaySessionData
{
    QString sessionKey;
    qint64 premiumUntil = 0;
    bool isPremium = false;
};

/// One "playdata.worlds" entry (CIP: TWorldList).
struct LoginWorld
{
    int id = 0;
    QString name;
    QString host;
    /// Game port: TCP on desktop, Crystal's WebSocket port in the browser.
    int port = 0;
    bool previewState = false;
    int pvpType = 0;
};

/// One "playdata.characters" entry (CIP: TCharacterList).
struct LoginCharacter
{
    QString name;
    int worldId = 0;
    int level = 0;
    QString vocation;
    int outfitId = 0;
    int headColor = 0;
    int torsoColor = 0;
    int legsColor = 0;
    int detailColor = 0;
    int addonsFlags = 0;
    bool isMale = true;
    bool isMainCharacter = false;
    bool isHidden = false;
    bool isOnline = false;
    bool tutorial = false;
    int dailyRewardState = 0;
};

struct LoginResult
{
    PlaySessionData session;
    QList<LoginWorld> worlds;
    QList<LoginCharacter> characters;

    const LoginWorld* world(int id) const;
};

/// POSTs the login service's JSON (CIP's own uploader; the product's portal speaks
/// the qtclient login format): CIP account/password login via
/// /clientservices/loginservice.php.
class TLoginRequestUploader final : public cip::tibia::authentication::TLoginRequestUploader
{
    Q_OBJECT
public:
    explicit TLoginRequestUploader(QObject* parent = nullptr);

    void login(const QUrl& url, const QString& email, const QString& password, const QString& authenticatorToken, const QString& clientRevision);
    /// Drop an in-flight login. The Abort button on CIP's connecting dialog.
    void abort();

    /// Apply Cloudflare clearance cookies / UA for the next (and future) POSTs.
    void applyClearance(const TCloudflareChallengeClearanceInfo& clearance);
    /// Challenge page URL from the last cf-mitigated reply (empty if none).
    QUrl lastMitigatedUrl() const { return m_lastMitigatedUrl; }

    void onFinished(bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD) override;
    /// CIP slot (skipped in cipgen for QNetworkReply*/std::string); tip invokes it from post().
    void onCloudflareMitigated(QNetworkReply* Reply, const std::string& MitigatedHeaderValue);

    static LoginResult parse(const QJsonObject& reply);

Q_SIGNALS:
    void loginSuccessful(const tibia::authentication::LoginResult& result);

private:
    void post(const QUrl& url, QJsonObject body, const QString& clientRevision);

    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
    QUrl m_lastUrl;
    QUrl m_lastMitigatedUrl;
    QString m_lastUserAgent;
};

}
}
