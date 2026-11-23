#include <gtest/gtest.h>

#include "assets/asset.h"
#include "assets/assetfilesystemhelper.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <set>

using tibia::assets::TAsset;
using tibia::assets::TAssetFileSystemHelper;

namespace {

void writeFile(const QDir& dir, const QString& name, const QByteArray& bytes)
{
    QFile file(dir.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

}

TEST(AssetFileSystemHelper, WriteAndReadAnAssetInItsTypesDirectory)
{
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(QDir(assets.path()), QDir(sounds.path()));

    const TAsset sprite(TAsset::Sprite, QStringLiteral("sprites.bin"));
    const QString spritePath = helper.writeAsset(sprite, QByteArrayLiteral("pixels"));
    EXPECT_EQ(spritePath, QDir(assets.path()).filePath(QStringLiteral("sprites.bin")));
    EXPECT_EQ(helper.readAsset(sprite), QByteArrayLiteral("pixels"));

    const TAsset sound(TAsset::Sound, QStringLiteral("hit.ogg"));
    EXPECT_EQ(helper.writeAsset(sound, QByteArrayLiteral("noise")), QDir(sounds.path()).filePath(QStringLiteral("hit.ogg")));
    EXPECT_EQ(helper.readAsset(sound), QByteArrayLiteral("noise"));
    EXPECT_EQ(helper.assetsDirectory(), QDir(assets.path()));
    EXPECT_EQ(helper.soundsDirectory(), QDir(sounds.path()));
}

TEST(AssetFileSystemHelper, RemoveAssetDeletesTheFileOfTheAssetsDirectory)
{
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(QDir(assets.path()), QDir(sounds.path()));
    writeFile(QDir(assets.path()), QStringLiteral("old.dat"), "x");

    helper.removeAsset(TAsset(TAsset::Appearances, QStringLiteral("old.dat")));
    EXPECT_FALSE(QFile::exists(QDir(assets.path()).filePath(QStringLiteral("old.dat"))));
}

TEST(AssetFileSystemHelper, FilesThatAreNotRetainedAreDeleted)
{
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(QDir(assets.path()), QDir(sounds.path()));
    const QDir dir(assets.path());
    writeFile(dir, QStringLiteral("keep.dat"), "k");
    writeFile(dir, QStringLiteral("drop.dat"), "d");

    std::set<QString> asked;
    helper.forEachLocalFileDecideWhetherItShouldBeRetained([&asked](const QString& name) {
        asked.insert(name);
        return name == QStringLiteral("keep.dat");
    });

    EXPECT_EQ(asked, (std::set<QString>{ QStringLiteral("drop.dat"), QStringLiteral("keep.dat") }));
    EXPECT_TRUE(QFile::exists(dir.filePath(QStringLiteral("keep.dat"))));
    EXPECT_FALSE(QFile::exists(dir.filePath(QStringLiteral("drop.dat"))));
}
