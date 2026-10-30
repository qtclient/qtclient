#include "qmldisk.h"

#if defined(TIBIA_QML_DISK)

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>

namespace tibia::app {

namespace {

constexpr QLatin1StringView QmlPrefix("/qt/qml/");

bool isCipResourcePath(const QString& path)
{
    return path.startsWith(QLatin1String("/images/")) || path.startsWith(QLatin1String("/animations/"));
}

class HotReloadFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        if (event->type() != QEvent::KeyPress)
            return false;
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() != Qt::Key_F5)
            return false;
        if (auto* engine = property("engine").value<QQmlEngine*>()) {
            engine->clearComponentCache();
            qInfo("qml-disk: cleared component cache (F5)");
        }
        return false;
    }
};

}

DiskQmlInterceptor::DiskQmlInterceptor(QString cipQmlDir, QString modQmlDir, QString overridesDirs)
    : m_cipQml(std::move(cipQmlDir))
    , m_modQml(std::move(modQmlDir))
    , m_overrideRoots(overridesDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts))
{
}

QString DiskQmlInterceptor::findOverride(const QString& rel) const
{
    for (const QString& root : m_overrideRoots) {
        const QString overrideFile = QDir(root).filePath(rel);
        if (QFileInfo::exists(overrideFile))
            return overrideFile;
    }
    return {};
}

QUrl DiskQmlInterceptor::rewriteCipResourceUrl(const QUrl& url) const
{
    QString path;
    if (url.scheme() == QLatin1String("file")) {
        path = url.toLocalFile();
#if defined(Q_OS_WIN)
        path.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (path.size() >= 2 && path.at(1) == QLatin1Char(':'))
            path = path.mid(2);
#endif
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

QUrl DiskQmlInterceptor::intercept(const QUrl& url, const DataType type)
{
    // Disk-loaded mod/overrides resolve absolute /images/… as file:///images/…
    // — map back into graphics_resources.rcc.
    if (const QUrl rewritten = rewriteCipResourceUrl(url); rewritten.isValid()) {
        if (type == UrlString || url.scheme() == QLatin1String("file") || url.scheme().isEmpty())
            return rewritten;
    }

    if (url.scheme() != QLatin1String("qrc"))
        return url;
    if (type != QmlFile && type != JavaScriptFile && type != UrlString)
        return url;

    const QString path = url.path();
    if (!path.startsWith(QmlPrefix))
        return url;
    if (path.endsWith(QLatin1String("/qmldir")) || path.endsWith(QLatin1String(".qmltypes")))
        return url;

    const QString rel = path.mid(QmlPrefix.size()); // e.g. <pack>/Foo.qml or qmlcomponents/qml/Foo.qml

    // Keep qmlcomponents (and LegacyControls) on qrc — they are the registered module.
    if (rel.startsWith(QLatin1String("qmlcomponents/"))
        || rel.startsWith(QLatin1String("QtQuick/"))) {
        // Optional: product override of a CIP file at the same relative path.
        if (const QString overrideFile = findOverride(rel); !overrideFile.isEmpty())
            return QUrl::fromLocalFile(overrideFile);
        return url;
    }

    if (const QString overrideFile = findOverride(rel); !overrideFile.isEmpty())
        return QUrl::fromLocalFile(overrideFile);

    // Pack QML FS module: qrc:/qt/qml/<module>/… → <qmlRoot>/<module>/…
    // TIBIA_QML_MOD_DIR is colon-separated pack qml/ roots.
    if (!m_modQml.isEmpty()) {
        const QStringList roots = m_modQml.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        for (const QString& root : roots) {
            const QString file = QDir(root).filePath(rel);
            if (QFileInfo::exists(file))
                return QUrl::fromLocalFile(file);
        }
    }

    Q_UNUSED(m_cipQml);
    return url;
}

void installQmlDiskHotReload(QQmlEngine* engine, QObject* context)
{
    if (!engine)
        return;

    auto* watcher = new QFileSystemWatcher(context);
    // Watch product trees only (cip qmlcomponents stays on qrc stubs).
    QStringList roots = QStringLiteral(TIBIA_QML_MOD_DIR).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    roots += QStringLiteral(TIBIA_QML_OVERRIDES_DIR).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const QStringList filters{ QStringLiteral("*.qml"), QStringLiteral("*.js") };
    for (const QString& root : roots) {
        if (root.isEmpty() || !QDir(root).exists())
            continue;
        QDirIterator it(root, filters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            watcher->addPath(it.next());
    }

    auto* debounce = new QTimer(context);
    debounce->setSingleShot(true);
    debounce->setInterval(150);
    QObject::connect(debounce, &QTimer::timeout, context, [engine] {
        engine->clearComponentCache();
        qInfo("qml-disk: source changed — component cache cleared (reopen dialogs; F5 to clear again)");
    });
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, context, [watcher, debounce](const QString& path) {
        if (QFileInfo::exists(path))
            watcher->addPath(path);
        debounce->start();
    });

    auto* filter = new HotReloadFilter(context);
    filter->setProperty("engine", QVariant::fromValue(engine));
    qApp->installEventFilter(filter);

    qInfo("qml-disk: mod QML + overrides from disk; qmlcomponents on qrc (F5 clears cache)");
}

}

#endif
