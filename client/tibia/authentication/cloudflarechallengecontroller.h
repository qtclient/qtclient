#pragma once

#include "authentication/cloudflarechallengetypes.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QNetworkCookie>
#include <QTimer>
#include <QUrl>

#if defined(TIBIA_DESKTOP_WEBENGINE)
class QQuickWebEngineProfile;
#endif

namespace tibia {
namespace authentication {

/// CloudflareChallengeDialog.qml (CIP: TCloudflareChallengeController).
/// Opens an off-the-record WebEngine profile, waits for `cf_clearance`, then
/// emits challengeSolveSuccess. Timeout / cancel / missing WebEngine → failed.
class TCloudflareChallengeController final : public gamewindow::TDialogController<::cip::TCloudflareChallengeController>
{
    Q_OBJECT
    Q_PROPERTY(QObject* webProfile READ webProfileObject CONSTANT)
public:
    explicit TCloudflareChallengeController(QObject* parent = nullptr);
    ~TCloudflareChallengeController() override;

    QObject* webProfileObject() const;

    QUrl targetUrl() const override;
    void setTargetUrl(const QUrl& v) override;

    Q_INVOKABLE bool isRunning() override;
    Q_INVOKABLE void cancel(const QString& Reason) override;
    Q_INVOKABLE void cancel() override;

    /// Start the challenge for `url` (login reply URL / cf-mitigated target).
    void startChallenge(const QUrl& url);

Q_SIGNALS:
    /// Skipped in cipgen (ClearanceInfo / Error types); qtclient owns these.
    void challengeSolveSuccess(const tibia::authentication::TCloudflareChallengeClearanceInfo& CloudflareClearanceResult);
    void challengeSolveFailed(tibia::authentication::TCloudflareChallengeError Error);

public Q_SLOTS:
    void onCookieAdded(const QNetworkCookie& Cookie);
    void stopAndCloseDialog();
    void onSolveTimeout();

private:
    void fail(TCloudflareChallengeError error);
    void succeed();

#if defined(TIBIA_DESKTOP_WEBENGINE)
    QQuickWebEngineProfile* m_profile = nullptr;
#endif
    QTimer m_timeout;
    bool m_running = false;
    bool m_finished = false;
    TCloudflareChallengeClearanceInfo m_clearance;
};

}
}
