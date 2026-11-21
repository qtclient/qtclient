#include "loginsplash.h"

#include "mods/modloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QStringList>

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

/// mods/<id>/images/launcher/BackgroundImage.jpg next to the binary (local F5).
QString findPackSplashPath()
{
    const QString rel = QStringLiteral("images/launcher/BackgroundImage.jpg");
    const auto tryRoot = [&](const QString& modsRoot) -> QString {
        QDir mods(modsRoot);
        if (!mods.exists())
            return {};
        const QFileInfoList entries =
            mods.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& info : entries) {
            if (info.fileName().startsWith(QLatin1Char('_')))
                continue;
            const QString path = QDir(info.absoluteFilePath()).filePath(rel);
            if (QFileInfo::exists(path))
                return path;
        }
        // Exclusive QTCLIENT_MODS_DIR may already be mods/<id>.
        const QString direct = mods.filePath(rel);
        if (QFileInfo::exists(direct))
            return direct;
        return {};
    };

    const QString env = qEnvironmentVariable("QTCLIENT_MODS_DIR");
    if (!env.isEmpty()) {
        if (const QString hit = tryRoot(env); !hit.isEmpty())
            return hit;
    }
    for (const QString& root : tibia::mods::ModLoader::tipModsRoots()) {
        if (const QString hit = tryRoot(root); !hit.isEmpty())
            return hit;
    }
    return {};
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
    const QString packPath = findPackSplashPath();
    const QUrl packUrl = packPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(packPath);
    const QUrl tip = loginSplashTipUrl(assetBase);
    const QUrl fallback(QString::fromLatin1(kLoginSplashRccFallback));

    // Pack disk → tip CDN → RCC title.jpg.
    const QUrl primary = packUrl.isValid() ? packUrl : (tip.isValid() ? tip : fallback);

    if (placeholder)
        placeholder->setProperty("splashImageSource", primary.toString());

    QQuickImage* image = findSplashImage(window, placeholder);
    if (!image)
        return;

    image->setSource(primary);
    if (primary == fallback || primary == packUrl)
        return;

    // Tip HTTP miss → pack disk if any, else stock CIP title art.
    QPointer<QQuickItem> holder = placeholder;
    QObject::connect(image, &QQuickImageBase::statusChanged, image,
                     [image, tip, packUrl, fallback, holder](QQuickImageBase::Status status) {
                         if (status != QQuickImageBase::Error)
                             return;
                         if (image->source() != tip)
                             return;
                         const QUrl next = packUrl.isValid() ? packUrl : fallback;
                         image->setSource(next);
                         if (holder)
                             holder->setProperty("splashImageSource", next.toString());
                     });
}

} // namespace app
} // namespace tibia
