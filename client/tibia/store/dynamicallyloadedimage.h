#pragma once

#include <cip_gen.h>

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia {
namespace store {

/// The store's web images (CIP: TDynamicallyLoadedImageManager, ctor
/// , metacall): `<base>/<size>/<key>` 
/// downloaded once and kept.
class TDynamicallyLoadedImageManager final : public cip::tibia::store::TDynamicallyLoadedImageManager
{
    Q_OBJECT
public:
    explicit TDynamicallyLoadedImageManager(QObject* parent = nullptr);

    void setBaseUrl(const QUrl& baseUrl);
    /// CIP : the image, now if known, else imageLoaded or
    /// imageLoadFailed later.
    void request(const QString& key, int size);
    /// `path` relative to the base (the home page's banners).
    QUrl url(const QString& path) const;

Q_SIGNALS:
    /// CIP signals 0 and 1.
    void imageLoaded(const QString& key, int size, const QImage& image);
    void imageLoadFailed(const QString& key, int size);

private:
    /// CIP .
    void onFinished(QNetworkReply* reply, const QString& key, int size);

    QNetworkAccessManager* m_network;
    QUrl m_baseUrl;
    QHash<QString, QImage> m_images;
    QSet<QString> m_pending;
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

private:
    int size() const;
    /// CIP .
    void requestImage();
    /// CIP .
    void showPlaceholder(bool failed);
    void onImageLoaded(const QString& key, int size, const QImage& image);
    void onImageLoadFailed(const QString& key, int size);

    QPointer<store::TDynamicallyLoadedImageManager> m_manager;
    QString m_requestedKey;
    int m_requestedSize = -1;
    QImage m_image;
};

}
}
