#pragma once

#include "input/gameaction.h"

#include <QString>
#include <QUrl>

namespace tibia::app {

/// Product portal URLs for CIP OPEN_URL_* / OPEN_NEWS actions (CIP reads
/// tibiaPageUrl and friends from the [URLS] blob at FUN_1009ab1c0; we map each
/// action to LauncherEnv::loginBase instead of tibia.com).
class PortalUrls
{
public:
    /// Path under the portal origin for one OPEN_URL / OPEN_NEWS action, or empty.
    static QUrl urlFor(const QUrl& loginBase, input::EGameAction action);
    /// Login / clientservices multiplex (`/api/login` by default). Same endpoint
    /// CipSoft hits for login, cacheinfo, boostedcreature, eventschedule, news, showoff.
    static QUrl clientServicesUrl(const QUrl& loginBase, const QString& loginPath = QStringLiteral("/api/login"));
    static void open(const QUrl& url);
};

}
