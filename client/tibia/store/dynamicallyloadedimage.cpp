#include "dynamicallyloadedimage.h"

#include "client/gamecore.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPainter>

namespace tibia::store {

namespace {

QString cacheKey(const QString& key, const int size)
{
    return QString::number(size) + QLatin1Char('/') + key;
}

/// CIP: QNetworkDiskCache under tip Resources/storeimages/ (data8/*.d).
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
    , m_network(new QNetworkAccessManager(this))
{
    attachStoreDiskCache(m_network);
}

void TDynamicallyLoadedImageManager::setBaseUrl(const QUrl& baseUrl)
{
    if (m_baseUrl == baseUrl)
        return;
    m_baseUrl = baseUrl;
    m_images.clear();
}

QUrl TDynamicallyLoadedImageManager::url(const QString& path) const
{
    QString base = m_baseUrl.toString();
    if (!base.endsWith(QLatin1Char('/')))
        base.append(QLatin1Char('/'));
    return QUrl(base + path);
}

void TDynamicallyLoadedImageManager::request(const QString& key, const int size)
{
    const QString cached = cacheKey(key, size);
    if (const auto it = m_images.constFind(cached); it != m_images.constEnd()) {
        Q_EMIT imageLoaded(key, size, *it);
        return;
    }
    if (!m_baseUrl.isValid() || m_baseUrl.isEmpty()) {
        Q_EMIT imageLoadFailed(key, size);
        return;
    }
    if (m_pending.contains(cached))
        return;
    m_pending.insert(cached);
    QNetworkRequest req(url(cached));
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply* reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, size] { onFinished(reply, key, size); });
}

void TDynamicallyLoadedImageManager::onFinished(QNetworkReply* reply, const QString& key, const int size)
{
    reply->deleteLater();
    const QString cached = cacheKey(key, size);
    m_pending.remove(cached);
    QImage image;
    if (reply->error() == QNetworkReply::NoError)
        image.loadFromData(reply->readAll());
    if (image.isNull()) {
        Q_EMIT imageLoadFailed(key, size);
        return;
    }
    m_images.insert(cached, image);
    Q_EMIT imageLoaded(key, size, image);
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
        connect(m_manager, &store::TDynamicallyLoadedImageManager::imageLoaded, this, &TDynamicallyLoadedImageQuickItem::onImageLoaded);
        connect(m_manager, &store::TDynamicallyLoadedImageManager::imageLoadFailed, this,
                &TDynamicallyLoadedImageQuickItem::onImageLoadFailed);
    }
    Q_EMIT propertyChanged();
    m_requestedKey.clear();
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
    if (m_requestedKey == m_imageKey && m_requestedSize == imageSize)
        return;
    m_requestedKey = m_imageKey;
    m_requestedSize = imageSize;
    showPlaceholder(false);
    m_manager->request(m_imageKey, imageSize);
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

void TDynamicallyLoadedImageQuickItem::onImageLoaded(const QString& key, const int size, const QImage& image)
{
    if (key != m_requestedKey || size != m_requestedSize)
        return;
    m_image = image;
    update();
}

void TDynamicallyLoadedImageQuickItem::onImageLoadFailed(const QString& key, const int size)
{
    if (key != m_requestedKey || size != m_requestedSize)
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
