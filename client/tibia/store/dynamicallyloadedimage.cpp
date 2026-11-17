#include "dynamicallyloadedimage.h"

#include "client/gamecore.h"
#include "sprites/spritesheetdecompressor.h"
#include "sprites/spritesystem.h"

#include "readonlymemoryblob.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPainter>

namespace tibia::store {

namespace {

/// CIP: QNetworkDiskCache under qtclient Resources/storeimages/ (data8/*.d).
void attachStoreDiskCache(QNetworkAccessManager* network)
{
    if (!network)
        return;
    const QString resources = client::gamecore::cipResourcesDir();
    if (resources.isEmpty())
        return;
    const QString cacheDir = QDir(resources).filePath(QStringLiteral("storeimages"));
    QDir().mkpath(cacheDir);
    auto* disk = new QNetworkDiskCache(network);
    disk->setCacheDirectory(cacheDir);
    // Qt default is 50 MiB; CIP size unrecovered from RE — keep default.
    network->setCache(disk);
}

}

TDynamicallyLoadedImageManager::TDynamicallyLoadedImageManager(QObject* parent)
    : cip::tibia::store::TDynamicallyLoadedImageManager(parent)
    , m_downloader(new QNetworkAccessManager(this))
{
    attachStoreDiskCache(m_downloader);
    if (const auto& decompressor = sprites::TSpriteSystem::instance().decompressor())
        connect(decompressor.get(), &sprites::TSpritesheetDecompressor::decompressingSpritesheetFinished, this,
                &TDynamicallyLoadedImageManager::onImageDecompressionFinished);
}

QUrl TDynamicallyLoadedImageManager::url(const QString& path) const
{
    QString base = m_baseUrl.toString();
    if (!base.endsWith(QLatin1Char('/')))
        base.append(QLatin1Char('/'));
    return QUrl(base + path);
}

bool TDynamicallyLoadedImageManager::isValidSize(const QSize& size)
{
    return size == QSize(32, 32) || size == QSize(64, 64) || size == QSize(13, 13);
}

QUrl TDynamicallyLoadedImageManager::imageUrl(const TImageRequest& request) const
{
    return url(QString::number(request.size.width()) + QLatin1Char('/') + request.key);
}

void TDynamicallyLoadedImageManager::requestImage(const TImageRequest& request)
{
    if (m_baseUrl.isValid() && isValidSize(request.size)) {
        download(request);
        return;
    }
    Q_EMIT imageDownloadOrDecodingError(request);
}

void TDynamicallyLoadedImageManager::download(const TImageRequest& request)
{
    const QUrl imageUrl = this->imageUrl(request);
    if (m_downloads.contains(imageUrl) || isDecompressing(imageUrl))
        return;
    QNetworkRequest networkRequest(imageUrl);
    networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply* reply = m_downloader->get(networkRequest);
    connect(reply, &QNetworkReply::finished, this, [this, reply, imageUrl] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            onDownloadSuccess(imageUrl, reply->readAll());
        else
            onDownloadError(imageUrl);
    });
    m_downloads.insert_or_assign(imageUrl, request);
}

bool TDynamicallyLoadedImageManager::isDecompressing(const QUrl& url) const { return m_decompressions.contains(url.toString()); }

void TDynamicallyLoadedImageManager::decompress(const TImageRequest& request, const QByteArray& data)
{
    const QString key = imageUrl(request).toString();
    m_decompressions.insert_or_assign(key, request);
    const auto& decompressor = sprites::TSpriteSystem::instance().decompressor();
    if (!decompressor) {
        onImageDecompressionFinished(key, nullptr);
        return;
    }
    decompressor->decompress(key, std::make_shared<shared::TReadOnlyMemoryBlob>(data));
}

void TDynamicallyLoadedImageManager::onDownloadSuccess(const QUrl& Url, const QByteArray& ImageData)
{
    const auto it = m_downloads.find(Url);
    if (it == m_downloads.end())
        return;
    const TImageRequest request = it->second;
    m_downloads.erase(it);
    if (ImageData.isEmpty())
        Q_EMIT imageDownloadOrDecodingError(request);
    else
        decompress(request, ImageData);
}

void TDynamicallyLoadedImageManager::onDownloadError(const QUrl& Url)
{
    const auto it = m_downloads.find(Url);
    if (it == m_downloads.end())
        return;
    const TImageRequest request = it->second;
    m_downloads.erase(it);
    Q_EMIT imageDownloadOrDecodingError(request);
}

void TDynamicallyLoadedImageManager::onImageDecompressionFinished(const QString& FullImageUrl, const std::shared_ptr<QImage>& pImagePointer)
{
    const auto it = m_decompressions.find(FullImageUrl);
    if (it == m_decompressions.end())
        return;
    const TImageRequest request = it->second;
    m_decompressions.erase(it);
    if (pImagePointer && !pImagePointer->isNull())
        Q_EMIT imageReady(request, *pImagePointer);
    else
        Q_EMIT imageDownloadOrDecodingError(request);
}

}

namespace tibia::qmlcomponents {

TDynamicallyLoadedImageQuickItem::TDynamicallyLoadedImageQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem(parent)
{
    m_sourceImageSize = -1;
    showPlaceholder(true);
}

int TDynamicallyLoadedImageQuickItem::size() const
{
    return m_sourceImageSize < 0 ? static_cast<int>(width()) : m_sourceImageSize;
}

void TDynamicallyLoadedImageQuickItem::setImageKey(const QString& key)
{
    if (m_imageKey == key)
        return;
    m_imageKey = key;
    Q_EMIT propertyChanged();
    requestImage();
}

void TDynamicallyLoadedImageQuickItem::setDynamicallyLoadedImageManager(QObject* manager)
{
    if (m_dynamicallyLoadedImageManager == manager)
        return;
    if (m_manager)
        disconnect(m_manager, nullptr, this, nullptr);
    m_dynamicallyLoadedImageManager = manager;
    m_manager = qobject_cast<store::TDynamicallyLoadedImageManager*>(manager);
    if (m_manager) {
        connect(m_manager, &store::TDynamicallyLoadedImageManager::imageReady, this, &TDynamicallyLoadedImageQuickItem::onImageLoaded);
        connect(m_manager, &store::TDynamicallyLoadedImageManager::imageDownloadOrDecodingError, this,
                &TDynamicallyLoadedImageQuickItem::onImageLoadFailed);
    }
    Q_EMIT propertyChanged();
    m_request = {};
    requestImage();
}

void TDynamicallyLoadedImageQuickItem::setSourceImageSize(const int size)
{
    if (m_sourceImageSize == size)
        return;
    m_sourceImageSize = size;
    Q_EMIT propertyChanged();
    requestImage();
}

void TDynamicallyLoadedImageQuickItem::setScaleUp(const bool scaleUp)
{
    if (m_scaleUp == scaleUp)
        return;
    m_scaleUp = scaleUp;
    Q_EMIT propertyChanged();
    update();
}

void TDynamicallyLoadedImageQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        requestImage();
}

void TDynamicallyLoadedImageQuickItem::itemChange(const ItemChange change, const ItemChangeData& value)
{
    cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged && value.boolValue)
        requestImage();
}

void TDynamicallyLoadedImageQuickItem::requestImage()
{
    if (!m_manager || m_imageKey.isEmpty() || !isVisible())
        return;
    const int imageSize = size();
    if (imageSize != 13 && imageSize != 32 && imageSize != 64)
        return;
    const store::TImageRequest request{ m_imageKey, QSize(imageSize, imageSize) };
    if (m_request.isValid() && m_request == request)
        return;
    m_request = request;
    showPlaceholder(false);
    m_manager->requestImage(m_request);
}

void TDynamicallyLoadedImageQuickItem::showPlaceholder(const bool failed)
{
    static const QImage loading(QStringLiteral(":/images/dynamic/dynamic-image-loading.png"));
    static const QImage loadingSmall(QStringLiteral(":/images/dynamic/dynamic-image-loading_13x13.png"));
    static const QImage error(QStringLiteral(":/images/dynamic/dynamic-image-error.png"));
    static const QImage errorSmall(QStringLiteral(":/images/dynamic/dynamic-image-error_13x13.png"));
    const bool useSmall = size() < 32;
    if (failed)
        m_image = useSmall ? errorSmall : error;
    else
        m_image = useSmall ? loadingSmall : loading;
    update();
}

/// CIP: only an answer to the shown request.
bool TDynamicallyLoadedImageQuickItem::isCurrentRequest(const store::TImageRequest& request) const
{
    return m_request.isValid() && m_request == request;
}

void TDynamicallyLoadedImageQuickItem::onImageLoaded(const store::TImageRequest& request, const QImage& image)
{
    if (!isCurrentRequest(request))
        return;
    m_image = image;
    update();
}

void TDynamicallyLoadedImageQuickItem::onImageLoadFailed(const store::TImageRequest& request)
{
    if (!isCurrentRequest(request))
        return;
    showPlaceholder(true);
}

void TDynamicallyLoadedImageQuickItem::paint(QPainter* painter)
{
    if (m_image.isNull())
        return;
    const QSizeF area = boundingRect().size();
    QSizeF drawn = m_image.size();
    if (drawn.width() > area.width() || drawn.height() > area.height() || m_scaleUp)
        drawn = drawn.scaled(area, Qt::KeepAspectRatio);
    const QRectF target(QPointF((area.width() - drawn.width()) / 2, (area.height() - drawn.height()) / 2), drawn);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, drawn != QSizeF(m_image.size()));
    painter->drawImage(target, m_image);
}

}
