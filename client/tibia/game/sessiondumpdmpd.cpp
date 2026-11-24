#include "game/sessiondumpdmpd.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtGlobal>

#include <cstring>

namespace tibia::game {

bool isSessiondumpDmpd(const QByteArray& bytes)
{
    return bytes.size() >= 3 && std::memcmp(bytes.constData(), "dmp", 3) == 0;
}

bool isSessiondumpDmpdFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return isSessiondumpDmpd(file.read(3));
}

QString resolveTutorialSessiondumpPath()
{
    const QString env = qEnvironmentVariable("QTCLIENT_TUTORIAL_SESSIONDUMP");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return QFileInfo(env).absoluteFilePath();

    const QStringList candidates = {
        QStringLiteral("testdata/sessiondumps/tutorial-sessiondump.dmp"),
        QStringLiteral("apps/qtclient/testdata/sessiondumps/tutorial-sessiondump.dmp"),
        QStringLiteral("tmp/re/tibia/ui-art/tutorial/tutorial-sessiondump.dmp"),
        QStringLiteral("tmp/re/tibia/gfx/tutorial/tutorial-sessiondump.dmp"),
    };
    for (const auto& rel : candidates) {
        if (QFileInfo::exists(rel))
            return QFileInfo(rel).absoluteFilePath();
    }

    // Walk tmp/re/tibia/*/tutorial/
    const QDir root(QStringLiteral("tmp/re/tibia"));
    if (root.exists()) {
        for (const auto& entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString path = root.filePath(entry + QStringLiteral("/tutorial/tutorial-sessiondump.dmp"));
            if (QFileInfo::exists(path))
                return QFileInfo(path).absoluteFilePath();
        }
    }
    return {};
}

}
