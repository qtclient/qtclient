#pragma once

#include <QString>
#include <QUrl>

namespace tibia::app {

/// CipSoft host → IdleOT origin remap (paths stay CIP-faithful).
/// www.tibia.com → portal, static.tibia.com → CDN, *.cipsoft.com → portal stubs.
class CipDomainMap
{
public:
    /// Rewrite absolute CIP hosts onto IdleOT origins when `portal` / `cdn` are set.
    /// Leaves path/query/fragment unchanged (including clientservices `*.php`).
    /// Non-CIP hosts (Twitch/YouTube) are left unchanged — use PortalUrls stubs instead.
    static QUrl remapUrl(const QUrl& url, const QUrl& portalOrigin, const QUrl& cdnOrigin);

    /// True when host is a CipSoft / Tibia host we own via remap.
    static bool isCipHost(const QString& host);
};

}
