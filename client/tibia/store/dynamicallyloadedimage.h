#pragma once

#include <cip_gen.h>

#include <QHash>
#include <QImage>
#include <QSize>

#include <map>
#include <memory>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia {
namespace store {

/// The store's web images (CIP: TDynamicallyLoadedImageManager, ctor
/// , metacall): `<base>/<size>/<key>` 
/// downloaded once and kept.
/// One image the store asks for: its key and its size (13, 32 or 64 pixels square).
struct TImageRequest
{
    QString key;
    QSize size{ -1, -1 };

    /// CIP: a key and a positive size.
    bool isValid() const { return !key.isEmpty() && size.width() > 0 && size.height() > 0; }
    bool operator==(const TImageRequest&) const = default;
    /// The order of the requests as map keys: the key (case-sensitive), then
    /// the width ascending, then the height descending.
    bool operator<(const TImageRequest& other) const
    {
        const int byKey = key.compare(other.key, Qt::CaseSensitive);
        if (byKey != 0)
            return byKey < 0;
        if (size.width() != other.size.width())
            return size.width() < other.size.width();
        return size.height() > other.size.height();
    }
};

/// The store's images (CIP: store::TDynamicallyLoadedImageManager): downloaded from
/// <base>/<size>/<key> through a disk cache and decoded by the sprite system's decompressor.
class TDynamicallyLoadedImageManager final : public cip::tibia::store::TDynamicallyLoadedImageManager
{
    Q_OBJECT
public:
    explicit TDynamicallyLoadedImageManager(QObject* parent = nullptr);

    void setBaseUrl(const QUrl& baseUrl) { m_baseUrl = baseUrl; }
    const QUrl& baseUrl() const { return m_baseUrl; }
    /// The image of a valid request (13x13, 32x32 or 64x64) is loaded unless it is on its way;
    /// any other request fails at once.
    void requestImage(const TImageRequest& request);
    /// `path` relative to the base (the home page's banners).
    QUrl url(const QString& path) const;

Q_SIGNALS:
    void imageReady(const tibia::store::TImageRequest& ImageRequest, const QImage& Image);
    void imageDownloadOrDecodingError(const tibia::store::TImageRequest& ImageRequest);

private:
    static bool isValidSize(const QSize& size);
    /// <base>/<width>/<key>.
    QUrl imageUrl(const TImageRequest& request) const;
    void download(const TImageRequest& request);
    bool isDecompressing(const QUrl& url) const;
    void decompress(const TImageRequest& request, const QByteArray& data);
    void onDownloadSuccess(const QUrl& Url, const QByteArray& ImageData);
    void onDownloadError(const QUrl& Url);
    void onImageDecompressionFinished(const QString& FullImageUrl, const std::shared_ptr<QImage>& pImagePointer);

    QNetworkAccessManager* m_downloader;
    QUrl m_baseUrl;
    std::map<QUrl, TImageRequest> m_downloads;
    std::map<QString, TImageRequest> m_decompressions;
};

}
}

namespace tibia {
namespace qmlcomponents {

/// DynamicallyLoadedImage (CIP: TDynamicallyLoadedImageQuickItem, metacall
/// ): one image of the manager, sized 13, 32 or 64; a loading
/// image meanwhile and an error image on failure .
class TDynamicallyLoadedImageQuickItem : public cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem
{
    Q_OBJECT
public:
    explicit TDynamicallyLoadedImageQuickItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    /// CIP .
    void setImageKey(const QString& key) override;
    /// CIP .
    void setDynamicallyLoadedImageManager(QObject* manager) override;
    void setSourceImageSize(int size) override;
    void setScaleUp(bool scaleUp) override;
    int tinySize13() const override { return 13; }
    int smallSize32() const override { return 32; }
    int bigSize64() const override { return 64; }

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

public Q_SLOTS:
    /// The manager's answer to the pending request: the image is shown.
    void onImageReady(const tibia::store::TImageRequest& ImageRequest, const QImage& Image);
    /// The pending request failed: the error icon is shown.
    void onImageDownloadOrDecodingError(const tibia::store::TImageRequest& ImageRequest);
    /// No answer within the loading delay: the loading icon (the error icon
    /// without an image key).
    void onLoadingDelayElapsedShowAppropriateIcon();

private:
    int size() const;
    /// Restarts the loading delay and asks the manager for the image.
    void requestImage();

    QPointer<store::TDynamicallyLoadedImageManager> m_manager;
    /// The request waiting for the manager's answer.
    store::TImageRequest m_request;
    QTimer m_loadingDelayTimer;
    QImage m_image;
};

}
}
