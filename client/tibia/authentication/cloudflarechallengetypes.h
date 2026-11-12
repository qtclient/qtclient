#pragma once

#include <QMetaType>
#include <QNetworkCookie>
#include <QString>
#include <QList>

namespace tibia {
namespace authentication {

/// CIP TCloudflareChallengeError (qm: cloudflare_challenge_error_*).
enum class TCloudflareChallengeError : int
{
    Unknown = 0,
    Timeout = 1,
    Cancelled = 2,
};

/// CIP TCloudflareChallengeClearanceInfo — cookies + UA after cf_clearance lands.
struct TCloudflareChallengeClearanceInfo
{
    QString userAgent;
    QList<QNetworkCookie> cookies;

    bool operator==(const TCloudflareChallengeClearanceInfo& other) const
    {
        return userAgent == other.userAgent && cookies == other.cookies;
    }
};

}
}

Q_DECLARE_METATYPE(tibia::authentication::TCloudflareChallengeClearanceInfo)
Q_DECLARE_METATYPE(tibia::authentication::TCloudflareChallengeError)
