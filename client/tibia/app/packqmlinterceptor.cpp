#include "packqmlinterceptor.h"

#include "mods/registry.h"

#include <QDir>
#include <QFileInfo>

namespace tibia::app {

namespace {

constexpr QLatin1StringView kQmlPrefix("/qt/qml/");

} // namespace

QUrl PackQmlInterceptor::intercept(const QUrl& url, const DataType type)
{
    if (url.scheme() != QLatin1String("qrc"))
        return url;
    if (type != QmlFile && type != JavaScriptFile && type != UrlString)
        return url;

    const QString path = url.path();
    if (!path.startsWith(kQmlPrefix))
        return url;

    const QString rel = path.mid(kQmlPrefix.size()); // e.g. idleot/TopBar.qml or qmlcomponents/qml/X.qml
    if (rel.isEmpty() || rel.startsWith(QLatin1String("QtQuick/")))
        return url;

    // CIP stock-path forks from pack overrides/ (later packs win via load order).
    if (rel.startsWith(QLatin1String("qmlcomponents/"))) {
        const auto& roots = tibia::mods::packOverrideRoots();
        for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
            const QString file = QDir(QString::fromStdString(*it)).filePath(rel);
            if (QFileInfo::exists(file))
                return QUrl::fromLocalFile(file);
        }
        return url;
    }

    for (const std::string& root : tibia::mods::packQmlImportPaths()) {
        const QString file = QDir(QString::fromStdString(root)).filePath(rel);
        if (QFileInfo::exists(file))
            return QUrl::fromLocalFile(file);
    }
    return url;
}

} // namespace tibia::app
