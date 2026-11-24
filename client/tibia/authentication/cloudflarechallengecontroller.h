#pragma once

#include "authentication/cloudflarechallengetypes.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QNetworkCookie>
#include <QTimer>
#include <QUrl>

#include <optional>

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

    /// CIP: starts the challenge for the target URL in the dialog's web view,
    /// failing after `Timeout` milliseconds; a running one is cancelled first.
    void start(int Timeout);
    /// CIP: why the last challenge failed, with the reason cancel was given
    /// with (none while it runs or after a success).
    const std::optional<TCloudflareChallengeError>& error() const { return m_error; }

Q_SIGNALS:
    /// Skipped in cipgen (ClearanceInfo / Error types); qtclient owns these.
    void challengeSolveSuccess(const tibia::authentication::TCloudflareChallengeClearanceInfo& CloudflareClearanceResult);
    void challengeSolveFailed(const tibia::authentication::TCloudflareChallengeError& Error);

public Q_SLOTS:
    void onCookieAdded(const QNetworkCookie& Cookie);
    void stopAndCloseDialog();
    void onSolveTimeout();

private:
    /// Ends a running challenge: stops and closes, keeps the error and its reason, emits failed.
    void fail(int code, const QString& reason);
    void failWithoutSolver();
    /// Ends a running challenge with the clearance cookie and the profile's user agent.
    void succeed();

#if defined(TIBIA_DESKTOP_WEBENGINE)
    QQuickWebEngineProfile* m_profile = nullptr;
#endif
    QTimer m_timeout;
    bool m_running = false;
    TCloudflareChallengeClearanceInfo m_clearance;
    std::optional<TCloudflareChallengeError> m_error;
};

}
}
