#pragma once

#include <QMetaType>
#include <QNetworkCookie>
#include <QString>

namespace tibia {
namespace authentication {

/// CIP TCloudflareChallengeError: why a challenge failed (qm:
/// cloudflare_challenge_error_*) and the reason it was cancelled with.
struct TCloudflareChallengeError
{
    enum : int
    {
        Unknown = 0,
        Cancelled = 1,
        Timeout = 2,
    };

    int code = Unknown;
    QString reason;

    bool operator==(const TCloudflareChallengeError& other) const
    {
        return code == other.code && reason == other.reason;
    }
};

/// CIP TCloudflareChallengeClearanceInfo: the cf_clearance cookie of a solved
/// challenge and the solver's user agent (the login request must send both).
struct TCloudflareChallengeClearanceInfo
{
    QNetworkCookie cookie;
    QString userAgent;

    bool operator==(const TCloudflareChallengeClearanceInfo& other) const
    {
        return cookie == other.cookie && userAgent == other.userAgent;
    }
};

}
}

Q_DECLARE_METATYPE(tibia::authentication::TCloudflareChallengeClearanceInfo)
Q_DECLARE_METATYPE(tibia::authentication::TCloudflareChallengeError)
