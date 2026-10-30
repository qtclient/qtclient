#include "portalurls.h"

#include "input/gameaction.h"

#include <QDesktopServices>

namespace tibia::app {

namespace {

using A = input::EGameAction;

QUrl withPath(const QUrl& base, const QString& path)
{
    QUrl url = base;
    if (url.isEmpty())
        return {};
    // Keep portal origin; append a site path (CIP opens absolute tibia.com URLs).
    const QString cleaned = path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
    url.setPath(cleaned);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

}

QUrl PortalUrls::urlFor(const QUrl& loginBase, const A action)
{
    if (loginBase.isEmpty())
        return {};
    switch (action) {
    case A::OPEN_URL_TIBIA_PAGE:
    case A::OPEN_URL_CIPSOFT:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_URL_MANUAL:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_URL_FAQ:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_URL_GET_PREMIUM:
    case A::OPEN_URL_PREMIUM_FEATURES:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_URL_GET_COINS:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_URL_CREATE_ACCOUNT:
        return withPath(loginBase, QStringLiteral("/account/signup"));
    case A::OPEN_URL_ACCESS_ACCOUNT:
        return withPath(loginBase, QStringLiteral("/account/login"));
    case A::OPEN_URL_LOST_ACCOUNT:
        return withPath(loginBase, QStringLiteral("/account/lost"));
    case A::OPEN_URL_SURVEY:
    case A::OPEN_URL_TWITCH_TIBIA:
    case A::OPEN_URL_YOUTUBE_TIBIA:
        return withPath(loginBase, QStringLiteral("/"));
    case A::OPEN_NEWS_DIALOG:
    case A::OPEN_NEWS_CATEGORY:
    case A::REFRESH_COMPENDIUM_NEWS:
        // In-client NewsDialog + TNewsStorage; browser archive is a fallback only.
        return withPath(loginBase, QStringLiteral("/news/archive"));
    default:
        return {};
    }
}

QUrl PortalUrls::clientServicesUrl(const QUrl& loginBase, const QString& loginPath)
{
    if (loginBase.isEmpty())
        return {};
    QUrl url = loginBase;
    QString path = loginPath;
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

void PortalUrls::open(const QUrl& url)
{
    if (url.isValid() && !url.isEmpty())
        QDesktopServices::openUrl(url);
}

}
