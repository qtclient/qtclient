#pragma once

#include "input/gameaction.h"
#include "launcherenv.h"

#include <QString>
#include <QUrl>

namespace tibia {
namespace app {

/// Product portal URLs for CIP OPEN_URL_* / OPEN_NEWS actions (CIP reads
/// tibiaPageUrl and friends from the [URLS] blob). Defaults are CipSoft-shaped
/// extensionless website paths under loginBase; QTCLIENT_URL_* / paymentsBase override.
class PortalUrls
{
public:
    /// Path under the portal origin for one OPEN_URL / OPEN_NEWS action, or empty.
    static QUrl urlFor(const LauncherEnv& env, input::EGameAction action);
    /// Backward-compatible: CIP defaults only (no overrides).
    static QUrl urlFor(const QUrl& loginBase, input::EGameAction action);
    /// CIP clientWebService (`/clientservices/clientservices.php` by default).
    static QUrl clientServicesUrl(const QUrl& loginBase,
                                  const QString& clientServicesPath = QStringLiteral("/clientservices/clientservices.php"));
    static void open(const QUrl& url);

    /// Map a CipSoft web URL onto this deployment, keeping path + query (CIP
    /// [URLS] pages default to www.tibia.com; qtclient serves the same paths
    /// from the configured origins):
    ///   www.tibia.com / tibia.com / qt.tibia.com → loginBase
    ///   www.test.tibia.com (the shipped config.ini's planner) → loginBase
    ///   static.tibia.com                         → assetBase
    ///   payments.cipsoft.com                     → paymentsBase (else loginBase)
    ///   limesurvey.cipsoft.com, www.cipsoft.com  → loginBase
    /// Any other URL is returned unchanged.
    static QUrl mapCipUrl(const LauncherEnv& env, const QUrl& url);
    /// Same, against the env registered by installCipUrlHandler().
    static QUrl mapCipUrl(const QUrl& url);
    /// Route every QDesktopServices::openUrl / Qt.openUrlExternally http(s)
    /// call through mapCipUrl (links in CIP dialogs and translations, the
    /// Wheel of Destiny planner, …). Call once after LauncherEnv::load.
    static void installCipUrlHandler(const LauncherEnv& env);
};

}
}
