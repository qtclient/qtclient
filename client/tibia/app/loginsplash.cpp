#include "loginsplash.h"

#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>

#include <QtQuick/private/qquickimage_p.h>

namespace tibia {
namespace app {

namespace {

QQuickImage* findSplashImage(QQuickWindow* window, QQuickItem* placeholder)
{
    if (!window)
        return nullptr;
    QQuickItem* content = window->contentItem();
    if (!content)
        return nullptr;
    for (QQuickItem* child : content->childItems()) {
        if (!child || child == placeholder)
            continue;
        if (auto* image = qobject_cast<QQuickImage*>(child))
            return image;
    }
    return nullptr;
}

} // namespace

QUrl loginSplashTipUrl(const QUrl& assetBase)
{
    if (!assetBase.isValid() || assetBase.isEmpty())
        return {};
    QString base = assetBase.toString();
    while (base.endsWith(u'/'))
        base.chop(1);
    return QUrl(base + QLatin1Char('/') + QLatin1String(kLoginSplashRelPath));
}

void applyLoginSplash(QQuickWindow* window, QQuickItem* placeholder, const QUrl& assetBase)
{
    const QUrl tip = loginSplashTipUrl(assetBase);
    const QUrl fallback(QString::fromLatin1(kLoginSplashRccFallback));
    const QUrl primary = tip.isValid() ? tip : fallback;

    if (placeholder)
        placeholder->setProperty("splashImageSource", primary.toString());

    QQuickImage* image = findSplashImage(window, placeholder);
    if (!image)
        return;

    image->setSource(primary);
    if (!tip.isValid())
        return;

    // Fall back to stock CIP title art when the tip file is missing.
    QPointer<QQuickItem> holder = placeholder;
    QObject::connect(image, &QQuickImageBase::statusChanged, image,
                     [image, tip, fallback, holder](QQuickImageBase::Status status) {
                         if (status != QQuickImageBase::Error)
                             return;
                         if (image->source() != tip)
                             return;
                         image->setSource(fallback);
                         if (holder)
                             holder->setProperty("splashImageSource", fallback.toString());
                     });
}

} // namespace app
} // namespace tibia
