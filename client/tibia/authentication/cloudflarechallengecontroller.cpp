#include "cloudflarechallengecontroller.h"

#include <QMetaType>
#include <QNetworkCookie>

#if defined(TIBIA_DESKTOP_WEBENGINE)
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#endif

namespace tibia::authentication {

namespace {

constexpr int SolveTimeoutMs = 120000;
constexpr auto ClearanceCookieName = "cf_clearance";

}

TCloudflareChallengeController::TCloudflareChallengeController(QObject* parent)
    : gamewindow::TDialogController<::cip::TCloudflareChallengeController>(QStringLiteral("CloudflareChallengeDialog.qml"), parent)
{
    qRegisterMetaType<TCloudflareChallengeClearanceInfo>("tibia::authentication::TCloudflareChallengeClearanceInfo");
    qRegisterMetaType<TCloudflareChallengeError>("tibia::authentication::TCloudflareChallengeError");
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &TCloudflareChallengeController::onSolveTimeout);

#if defined(TIBIA_DESKTOP_WEBENGINE)
    m_profile = new QQuickWebEngineProfile(QStringLiteral("tibia-cf-challenge"), this);
    m_profile->setOffTheRecord(true);
    m_profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::NoPersistentCookies);
    if (QWebEngineCookieStore* store = m_profile->cookieStore())
        connect(store, &QWebEngineCookieStore::cookieAdded, this, &TCloudflareChallengeController::onCookieAdded);
#endif
}

TCloudflareChallengeController::~TCloudflareChallengeController()
{
    m_timeout.stop();
}

QObject* TCloudflareChallengeController::webProfileObject() const
{
#if defined(TIBIA_DESKTOP_WEBENGINE)
    return m_profile;
#else
    return nullptr;
#endif
}

QUrl TCloudflareChallengeController::targetUrl() const
{
    return m_targetUrl;
}

void TCloudflareChallengeController::setTargetUrl(const QUrl& v)
{
    ::cip::TCloudflareChallengeController::setTargetUrl(v);
}

bool TCloudflareChallengeController::isRunning()
{
    return m_running;
}

void TCloudflareChallengeController::cancel(const QString& Reason)
{
    Q_UNUSED(Reason);
    fail(TCloudflareChallengeError::Cancelled);
}

void TCloudflareChallengeController::cancel()
{
    cancel(QString());
}

void TCloudflareChallengeController::startChallenge(const QUrl& url)
{
    if (m_running)
        cancel(QStringLiteral("restart"));

    m_finished = false;
    m_clearance = {};
    setTargetUrl(url);

#if !defined(TIBIA_DESKTOP_WEBENGINE)
    fail(TCloudflareChallengeError::Unknown);
    return;
#else
    if (!m_profile || !url.isValid()) {
        fail(TCloudflareChallengeError::Unknown);
        return;
    }

    m_clearance.userAgent = m_profile->httpUserAgent();
    Q_EMIT userAgentChanged();
    m_running = true;
    m_timeout.start(SolveTimeoutMs);
    Q_EMIT challengeSolveStarted(url);
    dialog()->show();
#endif
}

void TCloudflareChallengeController::onCookieAdded(const QNetworkCookie& Cookie)
{
    if (!m_running || m_finished)
        return;
    m_clearance.cookies.append(Cookie);
    if (Cookie.name() == QByteArray(ClearanceCookieName) && !Cookie.value().isEmpty())
        succeed();
}

void TCloudflareChallengeController::stopAndCloseDialog()
{
    Q_EMIT stopSolverWebview();
    if (dialog()->isShown())
        dialog()->close();
}

void TCloudflareChallengeController::onSolveTimeout()
{
    fail(TCloudflareChallengeError::Timeout);
}

void TCloudflareChallengeController::fail(const TCloudflareChallengeError error)
{
    if (m_finished)
        return;
    m_finished = true;
    m_running = false;
    m_timeout.stop();
    stopAndCloseDialog();
    Q_EMIT challengeSolveFailed(error);
}

void TCloudflareChallengeController::succeed()
{
    if (m_finished)
        return;
    m_finished = true;
    m_running = false;
    m_timeout.stop();
#if defined(TIBIA_DESKTOP_WEBENGINE)
    if (m_profile)
        m_clearance.userAgent = m_profile->httpUserAgent();
#endif
    stopAndCloseDialog();
    Q_EMIT challengeSolveSuccess(m_clearance);
}

}
