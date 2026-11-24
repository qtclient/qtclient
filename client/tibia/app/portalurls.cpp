#include "portalurls.h"

#include "input/gameaction.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QObject>
#include <QUrlQuery>

namespace tibia::app {

namespace {

using A = input::EGameAction;

QUrl withPath(const QUrl& base, const QString& path)
{
    QUrl url = base;
    if (url.isEmpty())
        return {};
    const QString cleaned = path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
    url.setPath(cleaned);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QUrl withPathAndQuery(const QUrl& base, const QString& path, const QUrlQuery& query)
{
    QUrl url = withPath(base, path);
    if (url.isEmpty())
        return {};
    url.setQuery(query);
    return url;
}

QUrl resolveOverride(const LauncherEnv& env, const char* key)
{
    const QString raw = env.urlOverride(key);
    if (raw.isEmpty())
        return {};
    const QUrl absolute(raw, QUrl::TolerantMode);
    if (absolute.isValid() && !absolute.scheme().isEmpty())
        return absolute;
    return withPath(env.loginBase, raw);
}

}

QUrl PortalUrls::urlFor(const QUrl& loginBase, const A action)
{
    LauncherEnv env;
    env.loginBase = loginBase;
    return urlFor(env, action);
}

QUrl PortalUrls::urlFor(const LauncherEnv& env, const A action)
{
    if (env.loginBase.isEmpty())
        return {};

    const auto tryOverride = [&](const char* key) -> QUrl {
        return resolveOverride(env, key);
    };

    switch (action) {
    case A::OPEN_URL_TIBIA_PAGE: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_TIBIA_PAGE"); !o.isEmpty())
            return o;
        return withPath(env.loginBase, QStringLiteral("/"));
    }
    case A::OPEN_URL_CIPSOFT: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_CIPSOFT"); !o.isEmpty())
            return o;
        return withPath(env.loginBase, QStringLiteral("/"));
    }
    case A::OPEN_URL_MANUAL: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_MANUAL"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("manual"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/gameguides"), q);
    }
    case A::OPEN_URL_FAQ: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_FAQ"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("faq"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/support"), q);
    }
    case A::OPEN_URL_GET_PREMIUM: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_GET_PREMIUM"); !o.isEmpty())
            return o;
        if (!env.paymentsBase.isEmpty())
            return env.paymentsBase;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("redirectlogin"));
        q.addQueryItem(QStringLiteral("clienttarget"), QStringLiteral("payment"));
        q.addQueryItem(QStringLiteral("clientselection"), QStringLiteral("SC"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/account"), q);
    }
    case A::OPEN_URL_PREMIUM_FEATURES: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_PREMIUM_FEATURES"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("premiumfeatures"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/abouttibia"), q);
    }
    case A::OPEN_URL_GET_COINS: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_GET_COINS"); !o.isEmpty())
            return o;
        if (!env.paymentsBase.isEmpty())
            return env.paymentsBase;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("redirectlogin"));
        q.addQueryItem(QStringLiteral("clienttarget"), QStringLiteral("payment"));
        q.addQueryItem(QStringLiteral("clientselection"), QStringLiteral("SC3"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/account"), q);
    }
    case A::OPEN_URL_CREATE_ACCOUNT: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_CREATE_ACCOUNT"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("createaccount"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/account"), q);
    }
    case A::OPEN_URL_ACCESS_ACCOUNT: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_ACCESS_ACCOUNT"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("accountmanagement"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/account"), q);
    }
    case A::OPEN_URL_LOST_ACCOUNT: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_LOST_ACCOUNT"); !o.isEmpty())
            return o;
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subtopic"), QStringLiteral("lostaccount"));
        return withPathAndQuery(env.loginBase, QStringLiteral("/account"), q);
    }
    case A::OPEN_URL_SURVEY: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_SURVEY"); !o.isEmpty())
            return o;
        // CIP limesurveyUrl: the survey id and `/lang-en?token=` are appended.
        return withPath(env.loginBase, QStringLiteral("/index.php/survey/index/sid/"));
    }
    case A::OPEN_URL_TWITCH_TIBIA: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_TWITCH"); !o.isEmpty())
            return o;
        return withPath(env.loginBase, QStringLiteral("/"));
    }
    case A::OPEN_URL_YOUTUBE_TIBIA: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_YOUTUBE"); !o.isEmpty())
            return o;
        return withPath(env.loginBase, QStringLiteral("/"));
    }
    case A::OPEN_NEWS_DIALOG:
    case A::OPEN_NEWS_CATEGORY:
    case A::REFRESH_COMPENDIUM_NEWS: {
        if (const QUrl o = tryOverride("QTCLIENT_URL_NEWS_ARCHIVE"); !o.isEmpty())
            return o;
        return withPath(env.loginBase, QStringLiteral("/news/archive"));
    }
    default:
        return {};
    }
}

QUrl PortalUrls::clientServicesUrl(const QUrl& loginBase, const QString& clientServicesPath)
{
    if (loginBase.isEmpty())
        return {};
    QUrl url = loginBase;
    QString path = clientServicesPath;
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

namespace {

LauncherEnv& activeEnv()
{
    static LauncherEnv env;
    return env;
}

/// Base origin + path prefix of `base`, then the CIP path/query/fragment.
QUrl rebase(const QUrl& base, const QUrl& cip, bool keepPath = true)
{
    if (base.isEmpty())
        return cip;
    QUrl out = base;
    QString prefix = base.path();
    while (prefix.endsWith(QLatin1Char('/')))
        prefix.chop(1);
    out.setPath(keepPath ? prefix + (cip.path().isEmpty() ? QStringLiteral("/") : cip.path())
                         : prefix + QStringLiteral("/"));
    out.setQuery(keepPath ? cip.query() : QString());
    out.setFragment(keepPath ? cip.fragment() : QString());
    return out;
}

class CipUrlHandler final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

public Q_SLOTS:
    void open(const QUrl& url)
    {
        const QUrl mapped = PortalUrls::mapCipUrl(url);
        // Hand the final URL to the platform without re-entering this handler.
        const QString scheme = mapped.scheme();
        const bool ours = scheme == QLatin1String("http") || scheme == QLatin1String("https");
        if (ours)
            QDesktopServices::unsetUrlHandler(scheme);
        QDesktopServices::openUrl(mapped);
        if (ours)
            QDesktopServices::setUrlHandler(scheme, this, "open");
    }
};

}

QUrl PortalUrls::mapCipUrl(const LauncherEnv& env, const QUrl& url)
{
    const QString host = url.host().toLower();
    if (host == QLatin1String("www.tibia.com") || host == QLatin1String("tibia.com")
        || host == QLatin1String("qt.tibia.com") || host == QLatin1String("www.test.tibia.com"))
        return rebase(env.loginBase, url);
    if (host == QLatin1String("static.tibia.com"))
        return rebase(env.assetBase, url);
    if (host == QLatin1String("payments.cipsoft.com"))
        return rebase(env.paymentsBase.isEmpty() ? env.loginBase : env.paymentsBase, url);
    if (host == QLatin1String("limesurvey.cipsoft.com"))
        return rebase(env.loginBase, url);
    if (host == QLatin1String("www.cipsoft.com") || host == QLatin1String("cipsoft.com"))
        return rebase(env.loginBase, url, false);
    return url;
}

QUrl PortalUrls::mapCipUrl(const QUrl& url)
{
    return mapCipUrl(activeEnv(), url);
}

void PortalUrls::installCipUrlHandler(const LauncherEnv& env)
{
    activeEnv() = env;
    static CipUrlHandler* handler = new CipUrlHandler(QCoreApplication::instance());
    QDesktopServices::setUrlHandler(QStringLiteral("http"), handler, "open");
    QDesktopServices::setUrlHandler(QStringLiteral("https"), handler, "open");
}

void PortalUrls::open(const QUrl& url)
{
    if (url.isEmpty() || !url.isValid())
        return;
    QDesktopServices::openUrl(url);
}

}

#include "portalurls.moc"
