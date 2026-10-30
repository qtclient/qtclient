#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QObject;

namespace tibia::shared {

/// POSTs `{ "type": <serviceType>, ... }` to the portal login/clientservices
/// multiplex (`LauncherEnv::loginUrl` / `PortalUrls::clientServicesUrl`).
/// Invokes `done(ok, doc)` on the GUI thread; `ok` is false on transport/parse failure.
void postClientService(QNetworkAccessManager* network,
                       QObject* context,
                       const QUrl& url,
                       const QString& serviceType,
                       QJsonObject body,
                       const QString& clientRevision,
                       std::function<void(bool, QJsonDocument)> done);

}
