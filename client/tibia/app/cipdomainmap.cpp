#include "cipdomainmap.h"

namespace tibia::app {

namespace {

bool hostEquals(const QString& host, const char* expected)
{
    return host.compare(QLatin1String(expected), Qt::CaseInsensitive) == 0;
}

}

bool CipDomainMap::isCipHost(const QString& host)
{
    return hostEquals(host, "www.tibia.com")
        || hostEquals(host, "static.tibia.com")
        || hostEquals(host, "tibia.com")
        || hostEquals(host, "limesurvey.cipsoft.com")
        || hostEquals(host, "www.cipsoft.com")
        || hostEquals(host, "cipsoft.com")
        || hostEquals(host, "payments.cipsoft.com");
}

QUrl CipDomainMap::remapUrl(const QUrl& url, const QUrl& portalOrigin, const QUrl& cdnOrigin)
{
    if (!url.isValid() || url.host().isEmpty())
        return url;

    const QString host = url.host();
    QUrl target;
    if (hostEquals(host, "static.tibia.com")) {
        target = cdnOrigin.isEmpty() ? portalOrigin : cdnOrigin;
    } else if (hostEquals(host, "payments.cipsoft.com")) {
        // Prefer CDN-adjacent payments host when assetBase is the static tip;
        // otherwise portal (caller may pass payments as portalOrigin).
        target = portalOrigin;
    } else if (isCipHost(host)) {
        target = portalOrigin;
    } else {
        return url;
    }
    if (target.isEmpty() || target.host().isEmpty())
        return url;

    QUrl out = url;
    out.setScheme(target.scheme());
    out.setHost(target.host());
    out.setPort(target.port(-1));
    return out;
}

}
