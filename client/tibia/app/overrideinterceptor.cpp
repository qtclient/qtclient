#include "overrideinterceptor.h"

#include <QDirIterator>

namespace tibia::app {

namespace {
constexpr QLatin1StringView Suffix(".override.qml");
}

OverrideInterceptor::OverrideInterceptor()
{
    QDirIterator it(QStringLiteral(":/qt/qml"), { QStringLiteral("*.override.qml") }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        path.chop(Suffix.size());
        m_overridden.insert(path + QStringLiteral(".qml"));
    }
}

QUrl OverrideInterceptor::intercept(const QUrl& url, const DataType type)
{
    if (type != QmlFile || url.scheme() != QLatin1String("qrc"))
        return url;
    const QString path = u':' + url.path();
    if (!m_overridden.contains(path))
        return url;
    QUrl out = url;
    QString p = url.path();
    p.chop(4);
    out.setPath(p + Suffix);
    return out;
}

}
