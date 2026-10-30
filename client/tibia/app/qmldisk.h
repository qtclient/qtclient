#pragma once

#include <QQmlAbstractUrlInterceptor>
#include <QString>
#include <QStringList>
#include <QUrl>

class QObject;
class QQmlEngine;

namespace tibia::app {

#if defined(TIBIA_QML_DISK)

/// Disk-QML interceptor for the macos-dev / linux-dev presets.
///
/// Remaps only product trees to live files (dev / TIBIA_QML_DISK):
///   qrc:/qt/qml/<module>/…         → pack qml/<module>/… (FS module)
///   qrc:/qt/qml/… under overrides  → client/overrides/… and
///                                    client/mods/<pack>/overrides/… (colon list)
/// Production always uses PackQmlInterceptor for pack qrc→file remaps.
///
/// Never remaps stock qmlcomponents / LegacyControls off qrc — CIP QML stays
/// the cmake module so `import qmlcomponents` works and upstream/ is not forked at
/// runtime. Absolute /images/… from file:// documents rewrite to qrc.
class DiskQmlInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    /// overridesDirs: colon-separated roots (engine + enabled pack overrides).
    DiskQmlInterceptor(QString cipQmlDir, QString modQmlDir, QString overridesDirs);

    QUrl intercept(const QUrl& url, DataType type) override;

private:
    QUrl rewriteCipResourceUrl(const QUrl& url) const;
    QString findOverride(const QString& rel) const;

    QString m_cipQml;
    QString m_modQml;
    QStringList m_overrideRoots;
};

/// Watches QML trees and clears the component cache; F5 also clears it.
/// Existing instances keep their old tree until recreated (reopen dialog / restart).
void installQmlDiskHotReload(QQmlEngine* engine, QObject* context);

#endif

}
