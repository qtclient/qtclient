#include "textbrowserwithnetwork.h"

#include "downloadpool.h"

#include <QDebug>
#include <QImage>
#include <QStringList>
#include <QTextDocument>

namespace tibia::shared {

QTextBrowserWithNetwork::QTextBrowserWithNetwork(std::shared_ptr<TDownloadPool> downloadPool, const int retries,
    const qint64 transferTimeoutMs, QWidget* parent)
    : QTextBrowser(parent)
    , m_downloadPool(std::move(downloadPool))
    , m_retries(retries)
    , m_transferTimeoutMs(transferTimeoutMs)
{
    connect(m_downloadPool.get(), &TDownloadPool::success, this, &QTextBrowserWithNetwork::onDownloadFinished);
    connect(m_downloadPool.get(), &TDownloadPool::failed, this, &QTextBrowserWithNetwork::onDownloadFailed);
}

QTextBrowserWithNetwork::~QTextBrowserWithNetwork() = default;

QSize QTextBrowserWithNetwork::sizeHint() const
{
    return document()->size().toSize();
}

int QTextBrowserWithNetwork::heightForWidth(const int width) const
{
    Q_UNUSED(width);
    return int(document()->size().height());
}

QVariant QTextBrowserWithNetwork::loadResource(const int type, const QUrl& name)
{
    if (!m_resources.contains(name)) {
        const QStringList networkSchemes = { QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("ftp") };
        if (!networkSchemes.contains(name.scheme(), Qt::CaseSensitive)) {
            m_resources[name] = QTextBrowser::loadResource(type, name);
        } else {
            m_downloadPool->download(name, m_retries, m_transferTimeoutMs);
            if (type == QTextDocument::ImageResource) {
                QImage placeholder(32, 32, QImage::Format_ARGB32_Premultiplied);
                placeholder.fill(Qt::transparent);
                m_resources[name] = QVariant(placeholder);
            } else {
                m_resources[name] = QVariant(type);
            }
        }
    }
    return QVariant(m_resources[name]);
}

void QTextBrowserWithNetwork::onDownloadFinished(QUrl Url, QByteArray Data)
{
    m_resources[Url] = QVariant(Data);
    setHtml(toHtml());
    updateGeometry();
}

void QTextBrowserWithNetwork::onDownloadFailed(QUrl Url, QNetworkReply::NetworkError Code, QString ErrorString)
{
    qWarning() << "Download failed:" << Url << Code << ErrorString;
}

}
