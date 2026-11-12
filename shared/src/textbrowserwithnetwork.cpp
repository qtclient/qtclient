#include "textbrowserwithnetwork.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextDocument>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace shared {

QTextBrowserWithNetwork::QTextBrowserWithNetwork(QWidget* parent)
    : QTextBrowser(parent)
{
}

QVariant QTextBrowserWithNetwork::loadResource(int type, const QUrl& name)
{
    if (type != QTextDocument::ImageResource) {
        return QTextBrowser::loadResource(type, name);
    }

    const QUrl url = name.isRelative() ? source().resolved(name) : name;
    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                           && url.scheme() != QStringLiteral("https")
                           && url.scheme() != QStringLiteral("qrc"))) {
        return QTextBrowser::loadResource(type, name);
    }

    if (url.scheme() == QStringLiteral("qrc")) {
        return QTextBrowser::loadResource(type, url);
    }

    const auto it = m_cache.constFind(url);
    if (it != m_cache.cend()) {
        return *it;
    }

    request(url);
    return {};
}

void QTextBrowserWithNetwork::request(const QUrl& url)
{
    if (m_pending.contains(url) || m_cache.contains(url)) {
        return;
    }
    m_pending.insert(url);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        m_pending.remove(url);
        if (reply->error() != QNetworkReply::NoError) {
            onDownloadFailed(url, reply->error(), reply->errorString());
            return;
        }
        onDownloadFinished(url, reply->readAll());
    });
}

void QTextBrowserWithNetwork::onDownloadFinished(QUrl url, QByteArray data)
{
    if (data.isEmpty()) {
        return;
    }
    m_cache.insert(url, data);
    document()->addResource(QTextDocument::ImageResource, url, data);
    // CIP re-resolves remote images after the NAM reply; a plain viewport
    // update is not always enough for the first paint on WASM.
    if (m_pending.isEmpty()) {
        const QString html = toHtml();
        setHtml(html);
    } else {
        viewport()->update();
    }
}

void QTextBrowserWithNetwork::onDownloadFailed(QUrl url,
                                               QNetworkReply::NetworkError code,
                                               QString errorString)
{
    // Safari COEP often surfaces these as a permission resource error with no URL.
    qWarning() << "tibia.launcher hint resource failed" << url << code << errorString;
#ifdef __EMSCRIPTEN__
    const QByteArray u = url.toEncoded();
    const QByteArray e = errorString.toUtf8();
    EM_ASM({
        console.warn("[qtclient.launcher] hint resource failed", UTF8ToString($0), UTF8ToString($1));
    }, u.constData(), e.constData());
#else
    Q_UNUSED(code);
#endif
    // Leave a broken-image placeholder; next setHtml() can retry.
}

} // namespace shared
