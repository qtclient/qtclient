#include "loginrequestuploader.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tibia::authentication {

namespace {

// Login service error codes (CIP 0x100970c70 switch).
constexpr int ErrorTwoFactorRequired = 6;
constexpr int ErrorClientOutOfDate = 7;

}

const LoginWorld* LoginResult::world(const int id) const
{
    for (const auto& w : worlds)
        if (w.id == id)
            return &w;
    return nullptr;
}

TLoginRequestUploader::TLoginRequestUploader(QObject* parent)
    : cip::tibia::authentication::TLoginRequestUploader(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(this, &TLoginRequestUploader::finished, this, &TLoginRequestUploader::onFinished);
}

void TLoginRequestUploader::login(const QUrl& url, const QString& email, const QString& password, const QString& authenticatorToken, const QString& clientRevision)
{
    QJsonObject body{
        { QStringLiteral("type"), QStringLiteral("login") },
        { QStringLiteral("email"), email },
        { QStringLiteral("password"), password },
        { QStringLiteral("stayloggedin"), true },
    };
    if (!authenticatorToken.isEmpty())
        body.insert(QStringLiteral("token"), authenticatorToken);
    post(url, body, clientRevision);
}

void TLoginRequestUploader::abort()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->abort();
}

void TLoginRequestUploader::post(const QUrl& url, QJsonObject body, const QString& clientRevision)
{
    if (!clientRevision.isEmpty())
        body.insert(QStringLiteral("clientRevision"), clientRevision);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!clientRevision.isEmpty())
        request.setRawHeader("X-Tibia-Client", clientRevision.toUtf8());
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
        // Error answers (401 etc.) still carry the service's JSON.
        QJsonParseError parseError{};
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (reply->error() != QNetworkReply::NoError)
                Q_EMIT loginFailed(tr("Could not reach the login service: %1").arg(reply->errorString()));
            else
                Q_EMIT finished(false, {});
            return;
        }
        Q_EMIT finished(true, doc);
    });
}

void TLoginRequestUploader::onFinished(const bool UploadSuccessful, const QJsonDocument& ReplyFromServerJD)
{
    if (!UploadSuccessful || !ReplyFromServerJD.isObject()) {
        Q_EMIT loginFailedDueToInvalidServerResponse();
        return;
    }
    const QJsonObject reply = ReplyFromServerJD.object();

    if (reply.contains(QStringLiteral("errorCode"))) {
        const int errorCode = reply.value(QStringLiteral("errorCode")).toInt();
        if (errorCode == ErrorClientOutOfDate)
            Q_EMIT loginFailedClientOutOfDate();
        else if (errorCode == ErrorTwoFactorRequired)
            Q_EMIT loginFailedDueToMissingTwoFactorToken();
        else
            Q_EMIT loginFailed(reply.value(QStringLiteral("errorMessage")).toString());
        return;
    }
    if (!reply.value(QStringLiteral("session")).isObject() || !reply.value(QStringLiteral("playdata")).isObject()) {
        Q_EMIT loginFailedDueToInvalidServerResponse();
        return;
    }
    Q_EMIT loginSuccessful(parse(reply));
}

LoginResult TLoginRequestUploader::parse(const QJsonObject& reply)
{
    LoginResult result;

    const QJsonObject session = reply.value(QStringLiteral("session")).toObject();
    result.session.sessionKey = session.value(QStringLiteral("sessionkey")).toString();
    result.session.premiumUntil = session.value(QStringLiteral("premiumuntil")).toInteger();
    result.session.isPremium = session.value(QStringLiteral("ispremium")).toBool()
        || result.session.premiumUntil > QDateTime::currentSecsSinceEpoch();

    const QJsonObject playdata = reply.value(QStringLiteral("playdata")).toObject();
    for (const auto& value : playdata.value(QStringLiteral("worlds")).toArray()) {
        const QJsonObject w = value.toObject();
        LoginWorld world;
        world.id = w.value(QStringLiteral("id")).toInt();
        world.name = w.value(QStringLiteral("name")).toString();
        world.host = w.value(QStringLiteral("externaladdressprotected")).toString(w.value(QStringLiteral("externaladdress")).toString());
        world.port = w.value(QStringLiteral("externalportprotected")).toInt(w.value(QStringLiteral("externalport")).toInt());
#ifdef Q_OS_WASM
        // A browser can only open WebSockets: Crystal's native WS game port.
        if (const int ws = w.value(QStringLiteral("websocketport")).toInt(); ws > 0)
            world.port = ws;
#endif
        world.previewState = w.value(QStringLiteral("previewstate")).toInt() == 1;
        world.pvpType = w.value(QStringLiteral("pvptype")).toInt();
        result.worlds.append(world);
    }

    for (const auto& value : playdata.value(QStringLiteral("characters")).toArray()) {
        const QJsonObject c = value.toObject();
        LoginCharacter character;
        character.name = c.value(QStringLiteral("name")).toString();
        character.worldId = c.value(QStringLiteral("worldid")).toInt();
        character.level = c.value(QStringLiteral("level")).toInt();
        character.vocation = c.value(QStringLiteral("vocation")).toString();
        character.outfitId = c.value(QStringLiteral("outfitid")).toInt();
        character.headColor = c.value(QStringLiteral("headcolor")).toInt();
        character.torsoColor = c.value(QStringLiteral("torsocolor")).toInt();
        character.legsColor = c.value(QStringLiteral("legscolor")).toInt();
        character.detailColor = c.value(QStringLiteral("detailcolor")).toInt();
        character.addonsFlags = c.value(QStringLiteral("addonsflags")).toInt();
        character.isMale = c.value(QStringLiteral("ismale")).toBool(true);
        character.isMainCharacter = c.value(QStringLiteral("ismaincharacter")).toBool();
        character.isHidden = c.value(QStringLiteral("ishidden")).toBool();
        character.isOnline = c.value(QStringLiteral("isonline")).toBool();
        character.tutorial = c.value(QStringLiteral("tutorial")).toBool();
        character.dailyRewardState = c.value(QStringLiteral("dailyrewardstate")).toInt();
        result.characters.append(character);
    }
    return result;
}

}
