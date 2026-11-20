// CIP process log (apps/qtclient/shared/src/logging.cpp).
#include <gtest/gtest.h>

#include "logging.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>

#include <stdexcept>

namespace {

QStringList readLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

}

// One test: the logger and the message handler are installed once per process.
TEST(Logging, WritesEveryMessageAsCipSoftLinesAndRotatesToOneBackup)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("client.log"));

    QStringList forwarded;
    tibia::shared::setUpLogging(path, 4096);
    tibia::shared::setLogMessageCallback([&forwarded](const QString& message) { forwarded << message; });

    qDebug() << "Using spritesheet cache size of" << 128;
    qWarning() << "Failed to open" << QStringLiteral("spells.json");

    const QStringList lines = readLines(path);
    ASSERT_EQ(lines.size(), 2);
    const QRegularExpression timestamp(QStringLiteral(R"(^\[ \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3} \] )"));
    EXPECT_TRUE(timestamp.match(lines[0]).hasMatch()) << lines[0].toStdString();
    EXPECT_TRUE(lines[0].endsWith(QStringLiteral("] Using spritesheet cache size of 128")));
    EXPECT_TRUE(lines[1].endsWith(QStringLiteral("] Failed to open \"spells.json\"")));
    EXPECT_EQ(forwarded, QStringList({ QStringLiteral("Using spritesheet cache size of 128"),
                                       QStringLiteral("Failed to open \"spells.json\"") }));

    // A second handler is refused.
    EXPECT_THROW(tibia::shared::installQtMessageHandler(&tibia::shared::qtMessageHandler), std::runtime_error);

    // Past the size the file rotates; only one backup is kept.
    tibia::shared::setLogMessageCallback({});
    for (int i = 0; i < 400; ++i)
        qInfo() << "StartDownload: " << i;
    EXPECT_TRUE(QFile::exists(QDir(directory.path()).filePath(QStringLiteral("client.1.log"))));
    EXPECT_FALSE(QFile::exists(QDir(directory.path()).filePath(QStringLiteral("client.2.log"))));

    qInstallMessageHandler(nullptr);
}
