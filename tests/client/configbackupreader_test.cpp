#include <gtest/gtest.h>

#include "config/configbackupreader.h"
#include "config/optionsarchive.h"
#include "filesystemhelper.h"

#include <QFile>
#include <QTemporaryDir>

#include <memory>

using tibia::config::TArchiveEntry;
using tibia::config::TConfigBackupReader;

namespace {

QString writeArchive(const QTemporaryDir& dir, const QList<TArchiveEntry>& entries)
{
    const QString path = dir.filePath(QStringLiteral("backup.zip"));
    EXPECT_NO_THROW(tibia::config::optionsarchive::write(path, entries));
    return path;
}

std::vector<QString> allowed()
{
    return { QStringLiteral("conf"), QStringLiteral("characterdata"), QStringLiteral("minimap") };
}

}

TEST(ConfigBackupReader, ExtractsOnlyAllowedDirectories)
{
    QTemporaryDir source;
    QTemporaryDir target;
    const QString path = writeArchive(source, { { QStringLiteral("conf/clientoptions.json"), "{}" },
                                                { QStringLiteral("minimap/./a.png"), "png" },
                                                { QStringLiteral("other/x.txt"), "x" },
                                                { QStringLiteral("confx/y.txt"), "y" } });
    const auto fileSystem = std::make_shared<tibia::shared::TFileSystemHelper>();
    const TConfigBackupReader reader(fileSystem, path, allowed());
    reader.extractTo(QDir(target.path()));

    EXPECT_TRUE(QFile::exists(target.filePath(QStringLiteral("conf/clientoptions.json"))));
    // Entry names are cleaned paths.
    EXPECT_TRUE(QFile::exists(target.filePath(QStringLiteral("minimap/a.png"))));
    EXPECT_FALSE(QFile::exists(target.filePath(QStringLiteral("other/x.txt"))));
    EXPECT_FALSE(QFile::exists(target.filePath(QStringLiteral("confx/y.txt"))));
    EXPECT_TRUE(reader.containsMinimap());
    EXPECT_TRUE(reader.containsOptions());
}

TEST(ConfigBackupReader, ContainsLooksForTheDirectoryNamesAnywhere)
{
    QTemporaryDir source;
    const QString path = writeArchive(source, { { QStringLiteral("characterdata/a/b.json"), "{}" } });
    const TConfigBackupReader reader(std::make_shared<tibia::shared::TFileSystemHelper>(), path, allowed());
    EXPECT_TRUE(reader.containsOptions());
    EXPECT_FALSE(reader.containsMinimap());
}

TEST(ConfigBackupReader, ThrowsOnBrokenArchiveAndDirectoryEntries)
{
    QTemporaryDir source;
    const QString broken = source.filePath(QStringLiteral("broken.zip"));
    QFile file(broken);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not a zip");
    file.close();
    const auto fileSystem = std::make_shared<tibia::shared::TFileSystemHelper>();
    EXPECT_THROW(static_cast<void>(TConfigBackupReader(fileSystem, broken, allowed())), TConfigBackupReader::TConfigBackupException);

    const QString withDirectory = writeArchive(source, { { QStringLiteral("conf/"), QByteArray() } });
    EXPECT_THROW(static_cast<void>(TConfigBackupReader(fileSystem, withDirectory, allowed())), TConfigBackupReader::TConfigBackupException);
}
