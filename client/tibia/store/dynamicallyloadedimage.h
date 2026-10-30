#pragma once

#include <cip_gen.h>

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace tibia::store {

/// The store's web images (CIP: TDynamicallyLoadedImageManager, ctor
/// 0x101006f70, metacall 0x10086c900): `<base>/<size>/<key>` (0x1010079b0)
/// downloaded once and kept.
class TDynamicallyLoadedImageManager final : public cip::tibia::store::TDynamicallyLoadedImageManager
{
    Q_OBJECT
public:
    explicit TDynamicallyLoadedImageManager(QObject* parent = nullptr);

    void setBaseUrl(const QUrl& baseUrl);
    /// CIP 0x1010077a0: the image, now if known, else imageLoaded or
    /// imageLoadFailed later.
    void request(const QString& key, int size);
    /// `path` relative to the base (the home page's banners).
    QUrl url(const QString& path) const;

Q_SIGNALS:
    /// CIP signals 0 and 1.
    void imageLoaded(const QString& key, int size, const QImage& image);
    void imageLoadFailed(const QString& key, int size);

private:
    /// CIP 0x101007480.
    void onFinished(QNetworkReply* reply, const QString& key, int size);

    QNetworkAccessManager* m_network;
    QUrl m_baseUrl;
    QHash<QString, QImage> m_images;
    QSet<QString> m_pending;
};

}

namespace tibia::qmlcomponents {

/// DynamicallyLoadedImage (CIP: TDynamicallyLoadedImageQuickItem, metacall
/// 0x1010821e0): one image of the manager, sized 13, 32 or 64; a loading
/// image meanwhile (0x1010aa080) and an error image on failure (0x1010aa9e0).
class TDynamicallyLoadedImageQuickItem : public cip::tibia::qmlcomponents::TDynamicallyLoadedImageQuickItem
{
    Q_OBJECT
public:
    explicit TDynamicallyLoadedImageQuickItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    /// CIP 0x1010aa590.
    void setImageKey(const QString& key) override;
    /// CIP 0x1010aa760.
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
    /// CIP 0x1010aa5f0.
    void requestImage();
    /// CIP 0x1010aa080.
    void showPlaceholder(bool failed);
    void onImageLoaded(const QString& key, int size, const QImage& image);
    void onImageLoadFailed(const QString& key, int size);

    QPointer<store::TDynamicallyLoadedImageManager> m_manager;
    QString m_requestedKey;
    int m_requestedSize = -1;
    QImage m_image;
};

}
