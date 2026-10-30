#pragma once

#include <QQmlAbstractUrlInterceptor>
#include <QSet>
#include <QUrl>

namespace tibia::app {

/// Legacy hook for a `<name>.override.qml` resource next to a CIP file.
/// Current overrides are compiled into the qmlcomponents module at the CIP
/// path, so this scan finds nothing unless a resource is still shipped that way.
class OverrideInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    /// Scans qrc:/qt/qml for overrides; inactive when there are none.
    OverrideInterceptor();

    bool isEmpty() const { return m_overridden.isEmpty(); }
    QUrl intercept(const QUrl& url, DataType type) override;

private:
    /// Resource paths (":/qt/qml/...X.qml") that have an override.
    QSet<QString> m_overridden;
};

}
