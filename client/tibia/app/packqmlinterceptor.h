#pragma once

#include <QQmlAbstractUrlInterceptor>
#include <QUrl>

namespace tibia::app {

/// Remap qrc:/qt/qml/<module>/… to pack filesystem modules under
/// mods/<id>/qml/<module>/ (paths from ModLoader::packQmlImportPaths).
/// Always installed so product C++ can keep qrc URLs without embedding QML
/// in the native MODULE.
class PackQmlInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl& url, DataType type) override;
};

} // namespace tibia::app
