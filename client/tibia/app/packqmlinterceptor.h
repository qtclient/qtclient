#pragma once

#include <QQmlAbstractUrlInterceptor>
#include <QUrl>

namespace tibia::app {

/// Remap qrc:/qt/qml/… onto pack filesystem trees:
/// - pack qml modules: mods/<id>/qml/<module>/… (packQmlImportPaths)
/// - CIP stock forks: mods/<id>/overrides/<rel> for qmlcomponents/… (packOverrideRoots)
/// Host qrc stays stock; product packs overlay at runtime (live-reload friendly).
class PackQmlInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    QUrl intercept(const QUrl& url, DataType type) override;
};

} // namespace tibia::app
