#include "packqmlinterceptor.h"

#include "platform/platform.h"

#include "mods/registry.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <string>
#include <vector>

namespace tibia::app {

namespace {

constexpr QLatin1StringView kQmlPrefix("/qt/qml/");

bool isCipResourcePath(const QString& path)
{
    return path.startsWith(QLatin1String("/images/"))
        || path.startsWith(QLatin1String("/animations/"));
}

/// Pack FS QML resolves absolute /images/… as file:///images/… — map into
/// graphics_resources.rcc (same as DiskQmlInterceptor; required on wasm/release
/// where TIBIA_QML_DISK is off).
QUrl rewriteCipResourceUrl(const QUrl& url)
{
    QString path;
    if (url.scheme() == QLatin1String("file")) {
        path = tibia::platform::normalizeLocalPath(url.toLocalFile());
    } else if (url.scheme().isEmpty()) {
        path = url.path();
    } else {
        return {};
    }
    if (!isCipResourcePath(path))
        return {};
    QUrl qrc;
    qrc.setScheme(QStringLiteral("qrc"));
    qrc.setPath(path);
    return qrc;
}

/// Pack file lookup for a qrc:/qt/qml/<rel> URL; an invalid QUrl = keep qrc.
QUrl findPackFile(const QString& rel)
{
    // CIP stock-path forks from pack overrides/ (later packs win via load order).
    if (rel.startsWith(QLatin1String("qmlcomponents/"))) {
        const auto& roots = tibia::mods::packOverrideRoots();
        for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
            const QString file = QDir(QString::fromStdString(*it)).filePath(rel);
            if (QFileInfo::exists(file))
                return QUrl::fromLocalFile(file);
        }
        return {};
    }

    for (const std::string& root : tibia::mods::packQmlImportPaths()) {
        const QString file = QDir(QString::fromStdString(root)).filePath(rel);
        if (QFileInfo::exists(file))
            return QUrl::fromLocalFile(file);
    }
    return {};
}

/// Every QML component load and url-string binding under qrc:/qt/qml/ comes
/// through here. On the web each QFileInfo::exists is an OPFS lookup that
/// suspends the main thread (JSPI), and override files mostly do not exist
/// (play.diag showed ~350 OPFS lookups a second at the login screen); while
/// suspended, Qt queues DOM events and handles them after dispatch. Pack trees do not change while the client runs there,
/// so remember each answer; a change of the pack root lists drops the cache.
QUrl cachedPackFile(const QString& rel)
{
    static QMutex mutex; // the QML type loader thread intercepts too
    static QHash<QString, QUrl> cache;
    static std::vector<std::string> overrideRoots;
    static std::vector<std::string> importPaths;

    QMutexLocker lock(&mutex);
    const auto& roots = tibia::mods::packOverrideRoots();
    const auto& imports = tibia::mods::packQmlImportPaths();
    if (roots != overrideRoots || imports != importPaths) {
        cache.clear();
        overrideRoots.assign(roots.begin(), roots.end());
        importPaths.assign(imports.begin(), imports.end());
    }
    if (const auto it = cache.constFind(rel); it != cache.constEnd())
        return *it;
    const QUrl found = findPackFile(rel);
    cache.insert(rel, found);
    return found;
}

} // namespace

QUrl PackQmlInterceptor::intercept(const QUrl& url, const DataType type)
{
    if (const QUrl rewritten = rewriteCipResourceUrl(url); rewritten.isValid()) {
        if (type == UrlString || url.scheme() == QLatin1String("file")
            || url.scheme().isEmpty())
            return rewritten;
    }

    if (url.scheme() != QLatin1String("qrc"))
        return url;
    if (type != QmlFile && type != JavaScriptFile && type != UrlString)
        return url;

    const QString path = url.path();
    if (!path.startsWith(kQmlPrefix))
        return url;

    const QString rel = path.mid(kQmlPrefix.size()); // e.g. pack/TopBar.qml or qmlcomponents/qml/X.qml
    if (rel.isEmpty() || rel.startsWith(QLatin1String("QtQuick/")))
        return url;

    // Desktop dev: new pack files show up live. Browser: every lookup is an OPFS round trip.
    const QUrl found = tibia::platform::kCheapFileLookups ? findPackFile(rel) : cachedPackFile(rel);
    return found.isValid() ? found : url;
}

} // namespace tibia::app
