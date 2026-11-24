#include "cloudflarechallengecontroller.h"

#include <QMetaType>
#include <QNetworkCookie>

#if defined(TIBIA_DESKTOP_WEBENGINE)
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#endif

namespace tibia::authentication {

namespace {

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
    fail(TCloudflareChallengeError::Cancelled, Reason);
}

void TCloudflareChallengeController::cancel()
{
    cancel(QString());
}

void TCloudflareChallengeController::start(const int Timeout)
{
    // A running challenge is cancelled first.
    if (m_running)
        cancel(QStringLiteral("Restarted"));
    m_clearance = TCloudflareChallengeClearanceInfo{ QNetworkCookie(), QString() };
    m_error.reset();

#if !defined(TIBIA_DESKTOP_WEBENGINE)
    failWithoutSolver();
    return;
#else
    if (!m_profile || !m_targetUrl.isValid()) {
        failWithoutSolver();
        return;
    }
    m_running = true;
    // The solver starts without cookies.
    if (QWebEngineCookieStore* store = m_profile->cookieStore())
        store->deleteAllCookies();
    m_timeout.start(Timeout);
    dialog()->create();
    dialog()->show();
    Q_EMIT challengeSolveStarted(m_targetUrl);
#endif
}

void TCloudflareChallengeController::onCookieAdded(const QNetworkCookie& Cookie)
{
    // Only the clearance cookie is kept; succeed() ignores it unless a challenge runs.
    if (Cookie.name() != QByteArray(ClearanceCookieName))
        return;
    m_clearance.cookie = Cookie;
    succeed();
}

void TCloudflareChallengeController::stopAndCloseDialog()
{
    m_running = false;
    m_timeout.stop();
    dialog()->close();
}

void TCloudflareChallengeController::onSolveTimeout()
{
    fail(TCloudflareChallengeError::Timeout, QString());
}

void TCloudflareChallengeController::fail(const int code, const QString& reason)
{
    if (!m_running)
        return;
    stopAndCloseDialog();
    m_error = TCloudflareChallengeError{ code, reason };
    Q_EMIT challengeSolveFailed(*m_error);
}

void TCloudflareChallengeController::failWithoutSolver()
{
    // Host: no WebEngine solver (browser build) or no usable target; the challenge fails at once.
    m_error = TCloudflareChallengeError{};
    Q_EMIT challengeSolveFailed(*m_error);
}

void TCloudflareChallengeController::succeed()
{
    if (!m_running)
        return;
    stopAndCloseDialog();
#if defined(TIBIA_DESKTOP_WEBENGINE)
    if (m_profile)
        m_clearance.userAgent = m_profile->httpUserAgent();
#endif
    Q_EMIT challengeSolveSuccess(m_clearance);
}

}
